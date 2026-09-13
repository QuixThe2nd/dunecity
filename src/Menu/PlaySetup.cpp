#include <Menu/PlaySetup.h>
#include <Menu/CustomGamePlayers.h>
#include <Menu/CustomGameMenu.h>
#include <Menu/CrossplayMenu.h>
#include <Menu/MenuBase.h>
#include <FileClasses/GFXManager.h>
#include <FileClasses/TextManager.h>
#include <GUI/MsgBox.h>
#include <GUI/dune/LoadSaveWindow.h>
#include <misc/FileSystem.h>
#include <misc/IMemoryStream.h>
#include <misc/fnkdat.h>
#include <misc/string_util.h>
#include <mod/ModManager.h>
#include <globals.h>
#include <sand.h>
#include <algorithm>
#include <utility>

namespace {
std::string userDirectory(const char* name) {
    char path[FILENAME_MAX];
    return fnkdat(name, path, FILENAME_MAX, FNKDAT_USER | FNKDAT_CREAT) >= 0 ? path : "";
}

std::vector<std::string> maps() {
    std::vector<std::string> result;
    for(const char* kind : {"singleplayer", "multiplayer"}) {
        std::string dir = getDuneLegacyDataDir() + "/maps/" + kind + "/";
        if(getFileNamesList(dir, "ini", true).empty()) dir = getDuneLegacyDataDir() + "/data/maps/" + kind + "/";
        for(const auto& source : {dir, userDirectory((std::string("maps/") + kind + "/").c_str())}) {
            if(source.empty()) continue;
            for(const auto& file : getFileNamesList(source, "ini", true)) result.push_back(source + file);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return strToLower(getBasename(a)) < strToLower(getBasename(b));
    });
    return result;
}

// Use the existing validated save header parser. A file's folder is not its game type.
GameInitSettings readSetup(const std::string& path, GameInitSettings::HouseInfoList& houses) {
    const auto bytes = readCompleteFile(path);
    IMemoryStream stream(bytes.data(), bytes.size());
    return GameInitSettings::readSaveSetup(stream, houses);
}

class PlayError final : public MenuBase {
public:
    explicit PlayError(const std::string& message) {
        setBackground(pGFXManager->getUIGraphic(UI_MenuBackground));
        resize(getTextureSize(pGFXManager->getUIGraphic(UI_MenuBackground)));
        openWindow(MsgBox::create(message));
    }
    void onChildWindowClose(Window*) override { quit(); }
};

class GameLibrary final : public MenuBase {
public:
    explicit GameLibrary(bool replay) : replay(replay) {
        setBackground(pGFXManager->getUIGraphic(UI_MenuBackground));
        resize(getTextureSize(pGFXManager->getUIGraphic(UI_MenuBackground)));
    }
    int showMenu() override {
        const auto saves = userDirectory("save/");
        const auto shared = userDirectory("mpsave/");
        const auto recordings = userDirectory("replay/");
        if(replay) openWindow(LoadSaveWindow::create(false, _("Replays"), recordings, "rpl"));
        else openWindow(LoadSaveWindow::create(false, _("Load Game"),
            std::vector<std::string>{saves, shared}, std::vector<std::string>{_("Saved games"), _("Online saves")}, "dls"));
        return MenuBase::showMenu();
    }
    void onChildWindowClose(Window* child) override {
        auto* picker = dynamic_cast<LoadSaveWindow*>(child);
        if(!picker) { quit(); return; }
        const auto path = picker->getFilename();
        if(path.empty()) { quit(); return; }
        // Run after the modal has been removed, rather than starting a nested game from its destructor.
        selected = path;
    }
    void update() override {
        if(selected.empty()) return;
        const auto path = std::exchange(selected, {});
        try {
            if(replay) startReplay(path);
            else {
                GameInitSettings::HouseInfoList houses;
                const auto saved = readSetup(path, houses);
                if(isNetworkGameType(saved.getGameType())) {
                    auto& mods = ModManager::instance();
                    if(!mods.setActiveMod(saved.getModName())) throw std::runtime_error("Install this save's mod before hosting it.");
                    GameInitSettings init(getBasename(path), readCompleteFile(path), settings.general.playerName + "'s saved game");
                    if(isCoopGameType(saved.getGameType())) {
                        init.configureCoopSave(saved, houses);
                        init.enableCoop(true, settings.general.playerName + "'s co-op campaign");
                    }
                    CrossplayMenu(init, false).showMenu();
                } else startSinglePlayerGame(GameInitSettings(path));
            }
            quit();
        } catch(const std::exception& error) { openWindow(MsgBox::create(error.what())); }
    }
private:
    bool replay;
    std::string selected;
};

std::string recentGame() {
    const auto dir = userDirectory("save/");
    if(dir.empty()) return {};
    const auto installed = ModManager::instance().listMods();
    for(const auto& file : getFileNamesList(dir, "dls", true, FileListOrder_ModifyDate_Dsc)) {
        try {
            GameInitSettings::HouseInfoList houses;
            const auto saved = readSetup(dir + file, houses);
            if(!isNetworkGameType(saved.getGameType())
               && std::any_of(installed.begin(), installed.end(), [&](const auto& mod) { return mod.name == saved.getModName(); })) return dir + file;
        } catch(const std::exception&) { /* Skip corrupt, newer or unavailable saves. */ }
    }
    return {};
}
}

void showGameLibrary(bool replays) { GameLibrary(replays).showMenu(); }
bool hasRecentGame() { return !recentGame().empty(); }
void continueRecentGame() {
    const auto path = recentGame();
    if(!path.empty()) startSinglePlayerGame(GameInitSettings(path));
    else showGameLibrary();
}

void playCustomGame(bool online) {
    CustomPlaySetup setup;
    setup.maps = maps();
    setup.mods = ModManager::instance().listMods();
    setup.rules = effectiveGameOptions;
    setup.online = online;
    // One controller per house is the legible default; advanced sharing stays available.
    setup.sharedHouse = false;
    for(size_t i = 0; i < setup.mods.size(); ++i)
        if(setup.mods[i].name == ModManager::instance().getActiveModName()) setup.mod = static_cast<int>(i);
    if(setup.maps.empty()) { PlayError(_("No custom maps found. Create a map in Extras > Map Editor, then return here.")).showMenu(); return; }
    bool chooseMap = true;
    for(;;) {
        if(chooseMap) {
            CustomGameMenu browser(false, false, &setup);
            if(browser.showMenu() != MENU_SETUP_PLAYERS) return;
            chooseMap = false;
        }
        auto& mods = ModManager::instance();
        const auto oldMod = mods.getActiveModName();
        if(!setup.mods.empty() && setup.mods[setup.mod].name != oldMod) {
            if(mods.setActiveMod(setup.mods[setup.mod].name)) setup.rules = effectiveGameOptions = mods.loadEffectiveGameOptions(settings.gameOptions);
            else for(size_t i = 0; i < setup.mods.size(); ++i) if(setup.mods[i].name == oldMod) setup.mod = static_cast<int>(i);
        }
        const auto path = setup.maps[setup.map];
        GameInitSettings init(getBasename(path, true), readCompleteFile(path), setup.sharedHouse, setup.rules);
        int result;
        {
            CustomGamePlayers menu(init, true, false, &setup);
            result = menu.showMenu();
        }
        if(result == MENU_SETUP_CHANGED) continue;
        if(result == MENU_SETUP_MAP || result == MENU_QUIT_DEFAULT) { chooseMap = true; continue; }
        if(result != MENU_SETUP_HOST) return;
        GameInitSettings networkInit(getBasename(path, true), readCompleteFile(path), settings.general.playerName + "'s custom game", setup.sharedHouse, setup.rules);
        if(CrossplayMenu(networkInit, setup.publicGame, setup.players).showMenu() == MENU_QUIT_GAME_FINISHED) return;
    }
}
