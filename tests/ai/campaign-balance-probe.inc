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
#include "campaign-stats-probe.inc"
#include "campaign-pressure-probe.inc"

int runCampaignBalanceProbe() {
    const int level = std::atoi(std::getenv("BALANCE_LEVEL"));
    const int minutes = std::atoi(std::getenv("BALANCE_MINUTES"));
    const auto seed = static_cast<Uint32>(std::stoul(std::getenv("BALANCE_SEED")));
    const std::string partner = std::getenv("BALANCE_PARTNER");
    const std::string partnerClass = partner == "hard" ? "qBotHard" : partner == "medium" ? "qBotMedium"
        : partner == "brutal" ? "qBotBrutal" : "qBotEasy";
    const std::string enemy = std::getenv("BALANCE_ENEMY") ? std::getenv("BALANCE_ENEMY") : "easy";
    const std::string enemyClass = enemy == "hard" ? "qBotHard" : enemy == "medium" ? "qBotMedium"
        : enemy == "brutal" ? "qBotBrutal" : "qBotEasy";
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
        info.addPlayerInfo({getHouseNameByNumber(static_cast<HOUSETYPE>(h)),h==HOUSE_HARKONNEN ? partnerClass : enemyClass});
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
    if (std::getenv("BALANCE_PRESSURE_PROBE")) {
        runCampaignPressureProbe(*game);
        game.reset(); currentGame=nullptr; pLocalPlayer=nullptr;
        return MENU_QUIT_DEFAULT;
    }
    if (std::getenv("BALANCE_STATS_PROBE")) {
        runCampaignStatsProbe(*game, level);
        game.reset(); currentGame = nullptr; pLocalPlayer = nullptr;
        return MENU_QUIT_DEFAULT;
    }
    if (std::getenv("BALANCE_STARPORT_PROBE")) {
        runStarportEconomyProbe(*game);
        game.reset(); currentGame = nullptr; pLocalPlayer = nullptr;
        return MENU_QUIT_DEFAULT;
    }
    const Uint32 end = MILLI2CYCLES(minutes * 60000);
    while (!game->finished && game->getGameCycleCount() < end) {
        game->updateGameState();
        if (game->getGameCycleCount()%250==0) {
            for (int h=0;h<NUM_HOUSES;++h) if (const auto* house=game->getHouse(h))
                for (const auto& player:house->getPlayerList())
                    if (const auto* bot=dynamic_cast<const QuantBot*>(player.get());bot && bot->isCampaignEnemy()) {
                        const auto used=bot->campaignPressure();const auto limit=bot->campaignProfile();
                        if (used.houses>limit.houses || used.units>limit.units || used.value>limit.value)
                            throw std::runtime_error("Live campaign exceeded shared assault limits");
                    }
        }
    }
    SDL_Log("CAMPAIGN_BALANCE_RESULT: level=%d seed=%u attack_percent=%d partner=%s enemy=%s cycle=%u outcome=%s",
        level, seed, percent, partner.c_str(), enemy.c_str(), game->getGameCycleCount(), game->finished ? (game->won ? "won" : "lost") : "time_limit");
    game.reset();
    currentGame = nullptr;
    pLocalPlayer = nullptr;
    return MENU_QUIT_DEFAULT;
}
