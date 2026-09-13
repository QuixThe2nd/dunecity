// Test-only entry point, linked against the real menu objects by run-menu-probe.py.
#include <Menu/HouseChoiceMenu.h>
#include <Menu/CustomGamePlayers.h>
#include <Menu/CustomGameMenu.h>
#include <Menu/CrossplayMenu.h>
#include <Menu/OptionsMenu.h>
#include <Menu/PlaySetup.h>
#include <mod/ModManager.h>
#include <misc/FileSystem.h>
#include <FileClasses/LoadSavePNG.h>
#include <players/PlayerFactory.h>

void menuRequire(bool value, const char* message) {
    if(!value) { SDL_Log("MENU_PROBE_FAIL: %s", message); throw std::runtime_error(message); }
}
void menuCapture(MenuBase& menu, const std::string& name) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    menu.draw();
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    auto surface = sdl2::surface_ptr(SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888));
    menuRequire(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,surface->pixels,surface->pitch)==0,"Capture failed");
    const auto path = std::string(std::getenv("MENU_PROBE_OUT"))+"/"+std::getenv("MENU_PROBE_WIDTH")+"-"+name+".bmp";
    menuRequire(SDL_SaveBMP(surface.get(),path.c_str())==0,"Screenshot write failed");
}
int runMenuProbe() {
    menuRequire(getRendererWidth()==std::atoi(std::getenv("MENU_PROBE_WIDTH"))
                && getRendererHeight()==std::atoi(std::getenv("MENU_PROBE_HEIGHT")),
                "Probe did not render at the requested dimensions");
    settings.general.playerName = "Menu tester";
    settings.network.relayUseDevelopmentEndpoint = true;
    // Disable online discovery in this fixture; admission and transport have separate tests.
    settings.network.directDevelopmentEndpoint = "";
    ModManager::instance().setActiveMod("vanilla");
    effectiveGameOptions = ModManager::instance().loadEffectiveGameOptions(settings.gameOptions);
    {
        MainMenu menu;
        menuRequire(menu.campaignButton.isVisible() && menu.customButton.isVisible() && menu.onlineButton.isVisible(), "Home play actions missing");
        menuRequire(!menu.displayButton.isVisible(),"Duplicate Display entry remains");
        menuCapture(menu,"home");
        SDL_Event tab{}; tab.type=SDL_KEYDOWN; tab.key.keysym.sym=SDLK_TAB;
        for(auto* expected : {&menu.customButton,&menu.onlineButton,&menu.loadButton,&menu.optionsButton,&menu.modesButton,&menu.quitButton,&menu.campaignButton}) {
            menu.handleInput(tab);
            menuRequire(menu.windowWidget.pActiveChildWidget==expected,"Home Tab order reaches a hidden destination or disagrees with screen order");
        }
    }
    {
        HouseChoiceMenu menu;
        menu.onHouseButton(1);
        menuRequire(!menu.isQuiting(),"Selecting a house starts the game");
        HouseChoiceMenu::s_startLevel = 7;
        HouseChoiceMenu::s_singleMission = false;
        menu.populateLevels();
        menuRequire(menu.startLevelDropDown.getNumEntries()==9,"Campaign level range wrong");
        menuCapture(menu,"campaign-offline");
        HouseChoiceMenu::s_currentGameOptions.fogOfWar = !effectiveGameOptions.fogOfWar;
    }
    {
        HouseChoiceMenu menu(true,true);
        menuRequire(HouseChoiceMenu::getStartLevel()==7,"Retry lost campaign level");
        menuRequire(HouseChoiceMenu::getGameOptions().fogOfWar != effectiveGameOptions.fogOfWar,"Retry lost game rules");
        menuRequire(!menu.supportBotDropDown.isEnabled(),"Online campaign AI selector should defer to lobby");
        menuCapture(menu,"campaign-online");
        HouseChoiceMenu::s_singleMission = true;
        menu.populateLevels();
        menuRequire(menu.startLevelDropDown.getNumEntries()==22,"Single mission range wrong");
    }
    CustomPlaySetup setup;
    auto files = getFileNamesList(getDuneLegacyDataDir()+"/maps/multiplayer/","ini",true);
    auto dir = getDuneLegacyDataDir()+"/maps/multiplayer/";
    if(files.empty()) { dir = getDuneLegacyDataDir()+"/data/maps/multiplayer/"; files=getFileNamesList(dir,"ini",true); }
    menuRequire(!files.empty(),"No bundled custom maps");
    for(const auto& file : files) setup.maps.push_back(dir+file);
    setup.mods = ModManager::instance().listMods();
    for(size_t i=0;i<setup.mods.size();++i) if(setup.mods[i].name=="vanilla") setup.mod=i;
    setup.rules = effectiveGameOptions;
    for(bool online : {false, true}) {
        auto mapSetup = setup;
        mapSetup.online = online;
        CustomGameMenu browser(false, false, &mapSetup);
        menuRequire(browser.mapList.getNumEntries() > 10, "Original scrolling map browser is missing");
        menuRequire(browser.getSelectedMapPath() == setup.maps[setup.map], "Map browser lost selected map");
        menuRequire(browser.mainVBox.getMinimumSize().x <= browser.mainVBox.getSize().x, "Map browser controls exceed menu width");
        menuRequire(browser.mainVBox.getMinimumSize().y <= browser.mainVBox.getSize().y, "Map browser controls exceed menu height");
        menuCapture(browser, online ? "map-browser-online" : "map-browser-offline");
        browser.onNext();
        menuRequire(browser.retVal == MENU_SETUP_PLAYERS, "Map selection did not proceed to players");
        menuRequire(mapSetup.online == online && mapSetup.map == setup.map, "Map browser changed mode or map");
    }
    const auto map = setup.maps.front();
    GameInitSettings init(getBasename(map,true),readCompleteFile(map),false,setup.rules);
    {
        GameInitSettings coop(HOUSE_ATREIDES, setup.rules, 1);
        coop.enableCoop(true, "Keyboard co-op");
        auto scenario = pFileManager->openCampaignFile(coop.getFilename());
        std::string scenarioData(static_cast<size_t>(SDL_RWsize(scenario.get())), '\0');
        SDL_RWread(scenario.get(), scenarioData.data(), 1, scenarioData.size());
        coop.setScenarioData(scenarioData);
        CustomGamePlayers menu(coop,true);
        SDL_Event tab{}; tab.type=SDL_KEYDOWN; tab.key.keysym.sym=SDLK_TAB;
        // A partner arriving enables Start after the lobby was built with it disabled.
        menu.update();
        menu.setPlayer2Slot("Keyboard partner",1);
        menu.update();
        bool reachedStart=false;
        for(int i=0;i<40;++i) { menu.handleInput(tab); if(menu.nextButton.isActive()) { SDL_Log("MENU_PROBE: campaign Start reached after %d Tabs",i+1); reachedStart=true; break; } }
        menuRequire(reachedStart,"Campaign lobby keyboard focus cannot reach Start after a partner joins");
        menuCapture(menu,"campaign-lobby");
    }
    ChangeEventList original;
    {
        CustomGamePlayers menu(init,true,false,&setup);
        original=menu.getChangeEventList();
        menuRequire(menu.numHouses>=2,"Fixture needs two houses");
        menu.update();
        menuRequire(menu.mainVBox.getMinimumSize().x<=menu.mainVBox.getSize().x,"Custom controls exceed menu width");
        menuCapture(menu,"custom-offline");
        menu.onCancel();
        menuRequire(menu.retVal == MENU_SETUP_MAP,"Back did not return to map browser");
        menuRequire(!setup.players.changeEventList.empty(),"Custom setup lost roster");
    }
    {
        auto roundTrip = setup;
        CustomGameMenu browser(false, false, &roundTrip);
        browser.onNext();
        menuRequire(roundTrip.players.changeEventList.size() == original.changeEventList.size(), "Returning through map browser lost the roster");
        menuRequire(roundTrip.map == setup.map, "Returning through map browser changed map");
    }
    setup.online=true;
    {
        CustomGamePlayers menu(init,true,false,&setup);
        auto restored=menu.getChangeEventList();
        menuRequire(restored.changeEventList.size()==original.changeEventList.size(),"Roster shape changed across online toggle");
        auto restoredEvent=restored.changeEventList.begin();
        for(const auto& a : original.changeEventList) {
            const auto& b=*restoredEvent++;
            menuRequire(a.eventType==b.eventType && a.slot==b.slot && a.newValue==b.newValue && a.newStringValue==b.newStringValue,"Online toggle changed player choices");
        }
        menu.update();
        menuCapture(menu,"custom-online");
        menu.onNext();
        menuRequire(!menu.isQuiting() && menu.hasChildWindow(),"Online setup allowed a lobby with every guest seat occupied by AI");
        menu.closeChildWindow();
        menu.processChildWindowOpenCloses();
        auto& opponent = menu.houseInfo[1].player1DropDown;
        for(int i=0;i<opponent.getNumEntries();++i)
            if(opponent.getEntry(i)==_("open")) { opponent.setSelectedItem(i); break; }
        menu.onNext();
        menuRequire(menu.retVal==MENU_SETUP_HOST,"Create Lobby did not return prepared setup");
    }
    {
        setup.sharedHouse=true;
        setup.players=ChangeEventList{};
        GameInitSettings sharedInit(getBasename(map,true),readCompleteFile(map),true,setup.rules);
        CustomGamePlayers menu(sharedInit,true,false,&setup);
        menuRequire(menu.mainVBox.getMinimumSize().x<=menu.mainVBox.getSize().x,"Shared house controls exceed menu width");
        menu.update(); menuCapture(menu,"custom-shared");
    }
    for(const auto& candidate : setup.maps) {
        if(getBasename(candidate).find("6P - 64x64") == std::string::npos) continue;
        for(bool shared : {false, true}) {
            auto sixSetup=setup;
            sixSetup.online=shared;
            sixSetup.sharedHouse=shared;
            sixSetup.map=static_cast<int>(std::distance(setup.maps.begin(), std::find(setup.maps.begin(), setup.maps.end(), candidate)));
            sixSetup.players=ChangeEventList{};
            GameInitSettings sixInit(getBasename(candidate,true),readCompleteFile(candidate),shared,setup.rules);
            CustomGamePlayers menu(sixInit,true,false,&sixSetup);
            menuRequire(menu.numHouses==6,"Six-house fixture not loaded");
            menuRequire(menu.mainVBox.getMinimumSize().y<=menu.mainVBox.getSize().y,"Six-house controls exceed menu height");
            menu.update();
            menuCapture(menu,shared ? "custom-six-houses" : "custom-six-offline");
        }
        break;
    }
    {
        menuRequire(ModManager::instance().setActiveMod("Tornie"),"Bonus-color fixture mod unavailable");
        GameInitSettings modInit(getBasename(map,true),readCompleteFile(map),false,setup.rules);
        CustomGamePlayers menu(modInit,true);
        menuRequire(menu.mainVBox.getMinimumSize().x<=menu.mainVBox.getSize().x,"Bonus color controls exceed menu width");
        auto& row=menu.houseInfo[0];
        menuRequire(row.bonusColorCheckbox.isVisible() && row.bonusColorCheckbox.isEnabled(),"Classic bonus color selector is unavailable");
        row.bonusColorCheckbox.setChecked(true);
        menu.onBonusColorCheckbox(0);
        menuRequire(row.colorDropDown.getNumEntries()>1,"Bonus palette has no colors");
        menuCapture(menu,"custom-bonus-colors");
        ModManager::instance().setActiveMod("vanilla");
    }
    setup.players=original;
    {
        CrossplayMenu menu;
        menu.playerNameTextBox.setText("");
        menu.onHostCustomGame();
        menuRequire(menu.hasChildWindow(),"Custom hosting bypasses the entered player name");
        menu.closeChildWindow(); menu.processChildWindowOpenCloses();
        menu.onHostCampaignCoop();
        menuRequire(menu.hasChildWindow(),"Campaign hosting bypasses the entered player name");
        menu.closeChildWindow(); menu.processChildWindowOpenCloses();
        menu.playerNameTextBox.setText(settings.general.playerName);
        menu.stage=CrossplayMenu::Stage::Choosing;
        PublicRelayGame coop, custom;
        coop.mode="coop"; coop.hostName="Campaign host"; coop.roomCode="AAAA-BBBB-CCCC"; coop.players=1; coop.maxPeers=2;
        custom.mode="custom"; custom.hostName="Custom host"; custom.roomCode="DDDD-EEEE-FFFF"; custom.players=2; custom.maxPeers=4;
        menu.allPublicGames={coop,custom}; menu.refreshDirectory();
        menu.modeFilter.setSelectedItem(1); menu.refreshDirectory();
        menuRequire(menu.publicGames.size()==1 && menu.publicGames[0].mode=="coop","Campaign filter includes custom games");
        menu.modeFilter.setSelectedItem(2); menu.refreshDirectory();
        menuRequire(menu.publicGames.size()==1 && menu.publicGames[0].mode=="custom","Custom filter includes campaign games");
        menu.modeFilter.setSelectedItem(0); menu.refreshDirectory();
        menuCapture(menu,"join");
        menu.showChat=true; menu.layoutControls(); menu.refreshControls();
        menuCapture(menu,"public-chat");
    }
    {
        GameInitSettings networkInit(getBasename(map,true),readCompleteFile(map),"Prepared custom",false,setup.rules);
        CrossplayMenu menu(networkInit,false,setup.players);
        menuRequire(menu.preparedGame->getFiledata()==init.getFiledata(),"Prepared hosting changed the selected map");
        menuRequire(menu.preparedPlayers.changeEventList.size()==original.changeEventList.size(),"Prepared hosting lost the roster");
        menuRequire(menu.visibilityChoice.getSelectedIndex()==0,"Private room is not the default");
    }
    {
        OptionsMenu menu;
        for(int page=0;page<5;++page) { menu.showPage(page); menuCapture(menu,"settings-"+std::to_string(page)); }
        menuRequire(menu.interfaceSizeDropDownBox.getParent()==&menu.interfaceHBox,"Interface size missing from Settings");
        menuRequire(menu.pages[1].getMinimumSize().y <= menu.pages[1].getSize().y,"Graphics controls overflow");
        menu.showPage(2);
        settings.network.metaServer=""; menu.metaServerTextBox.setText("");
        menu.playSFXCheckbox.setChecked(!settings.audio.playSFX);
        menu.onOptionsOK();
        menuRequire(menu.isQuiting(),"Unchanged legacy network field blocked audio apply");
    }
    SDL_Log("MENU_PROBE_PASS: home, campaign retry, mission ranges, custom roster, online transition, settings validation and rendering");
    return MENU_QUIT_DEFAULT;
}
