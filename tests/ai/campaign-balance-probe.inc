// Included only by run-campaign-balance.py, never by the shipped game.
#include <Game.h>
#include <GameInitSettings.h>
#include <House.h>
#include <sand.h>
#include <mod/ModManager.h>
#include <players/QuantBotConfig.h>
#include <memory>
#include <cstdlib>

#include "starport-economy-probe.inc"

int runCampaignBalanceProbe() {
    const int level = std::atoi(std::getenv("BALANCE_LEVEL"));
    const int minutes = std::atoi(std::getenv("BALANCE_MINUTES"));
    const auto seed = static_cast<Uint32>(std::stoul(std::getenv("BALANCE_SEED")));
    const std::string partner = std::getenv("BALANCE_PARTNER");
    const std::string partnerClass = partner == "hard" ? "qBotHard" : partner == "medium" ? "qBotMedium"
        : partner == "brutal" ? "qBotBrutal" : "qBotEasy";
    const int percent = std::atoi(std::getenv("BALANCE_ATTACK_PERCENT"));
    ModManager::instance().setActiveMod("vanilla");
    getQuantBotConfig().easy.attackForceMilitaryValueRatio = percent / 100.0f;
    settings.general.playerName = "Campaign balance observer";
    settings.network.metaServer = "http://127.0.0.1:1/";
    settings.gameOptions.fogOfWar = false;
    settings.gameOptions.immortalHumanPlayer = false;
    GameInitSettings init(HOUSE_HARKONNEN, settings.gameOptions, level);
    init.randomSeed = seed; // Test-only access, provided by -fno-access-control.
    init.setMultiplePlayersPerHouse(true);
    for (int h=0; h<NUM_HOUSES; ++h) {
        if (!isHouseAvailable(static_cast<HOUSETYPE>(h))) continue;
        GameInitSettings::HouseInfo info(static_cast<HOUSETYPE>(h), h==HOUSE_HARKONNEN ? 1 : 2);
        if (h==HOUSE_HARKONNEN) info.addPlayerInfo({settings.general.playerName,"HumanPlayer"});
        info.addPlayerInfo({getHouseNameByNumber(static_cast<HOUSETYPE>(h)),h==HOUSE_HARKONNEN ? partnerClass : "qBotEasy"});
        init.addHouseInfo(info);
    }
    auto game = std::make_unique<Game>();
    currentGame = game.get();
    // Suppress the interactive briefing and public match analytics at setup.
    // Restore ordinary AI/command execution before advancing any game cycles.
    game->bReplay = true;
    game->initGame(init);
    game->bReplay = false;
    game->initializeGameLoop();
    if (std::getenv("BALANCE_STARPORT_PROBE")) {
        runStarportEconomyProbe(*game);
        game.reset(); currentGame = nullptr; pLocalPlayer = nullptr;
        return MENU_QUIT_DEFAULT;
    }
    const Uint32 end = MILLI2CYCLES(minutes * 60000);
    while (!game->finished && game->getGameCycleCount() < end) game->updateGameState();
    SDL_Log("CAMPAIGN_BALANCE_RESULT: level=%d seed=%u attack_percent=%d partner=%s cycle=%u outcome=%s",
        level, seed, percent, partner.c_str(), game->getGameCycleCount(), game->finished ? (game->won ? "won" : "lost") : "time_limit");
    game.reset();
    currentGame = nullptr;
    pLocalPlayer = nullptr;
    return MENU_QUIT_DEFAULT;
}
