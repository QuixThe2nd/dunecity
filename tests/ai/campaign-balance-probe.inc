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
#include "campaign-defence-probe.inc"
#include "campaign-pacing-probe.inc"
#include "campaign-repair-probe.inc"
#include "campaign-helper-economy-probe.inc"

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
    const auto humanHouse=static_cast<HOUSETYPE>(std::getenv("BALANCE_HOUSE") ? std::atoi(std::getenv("BALANCE_HOUSE")) : HOUSE_HARKONNEN);
    ModManager::instance().setActiveMod("vanilla");
    getQuantBotConfig().easy.attackForceMilitaryValueRatio = percent / 100.0f;
    settings.general.playerName = "Campaign balance observer";
    settings.network.metaServer = "http://127.0.0.1:1/";
    settings.gameOptions.fogOfWar = false;
    settings.gameOptions.immortalHumanPlayer = false;
    if (std::getenv("BALANCE_HARVESTER_LIMIT"))
        settings.gameOptions.maximumNumberOfHarvestersOverride=std::atoi(std::getenv("BALANCE_HARVESTER_LIMIT"));
    if (std::getenv("BALANCE_PACING_PROBE")) settings.gameOptions.maximumNumberOfHarvestersOverride=100;
    GameInitSettings init(humanHouse, settings.gameOptions, level);
    init.randomSeed = seed; // Test-only access, provided by -fno-access-control.
    init.setMultiplePlayersPerHouse(true);
    for (int h=0; h<NUM_HOUSES; ++h) {
        if (!isHouseAvailable(static_cast<HOUSETYPE>(h))) continue;
        GameInitSettings::HouseInfo info(static_cast<HOUSETYPE>(h), h==humanHouse ? 1 : 2);
        if (h==humanHouse) info.addPlayerInfo({settings.general.playerName,"HumanPlayer"});
        info.addPlayerInfo({getHouseNameByNumber(static_cast<HOUSETYPE>(h)),h==humanHouse ? partnerClass : enemyClass});
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
    if (std::getenv("BALANCE_REPAIR_PROBE")) {
        runCampaignRepairProbe(*game);
        game.reset();currentGame=nullptr;pLocalPlayer=nullptr;
        return MENU_QUIT_DEFAULT;
    }
    if (std::getenv("BALANCE_PACING_PROBE")) {
        runCampaignPacingProbe(*game);
        game.reset();currentGame=nullptr;pLocalPlayer=nullptr;
        return MENU_QUIT_DEFAULT;
    }
    if (std::getenv("BALANCE_DEFENCE_PROBE")) {
        runCampaignDefenceProbe(*game);
        game.reset();currentGame=nullptr;pLocalPlayer=nullptr;
        return MENU_QUIT_DEFAULT;
    }
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
    if (std::getenv("BALANCE_HELPER_ECONOMY_PROBE")) {
        runCampaignHelperEconomyProbe(*game);
        game.reset();currentGame=nullptr;pLocalPlayer=nullptr;
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
                        if (used.houses>limit.houses
                            || (limit.limitedWave && (used.units>limit.units || used.value>limit.value)))
                            throw std::runtime_error("Live campaign exceeded shared assault limits");
                    }
        }
    }
    SDL_Log("CAMPAIGN_BALANCE_RESULT: level=%d seed=%u attack_percent=%d partner=%s enemy=%s cycle=%u outcome=%s",
        level, seed, percent, partner.c_str(), enemy.c_str(), game->getGameCycleCount(), game->finished ? (game->won ? "won" : "lost") : "time_limit");
    if (!game->finished) {
        for (const auto* unit:unitList) {
            const auto* target=unit->getTarget();
            const auto* candidate=unit->findTarget();
            SDL_Log("CAMPAIGN_TARGET_AUDIT: id=%u attack_pos=%d find_timer=%d pending=%d candidate=%u candidate_item=%u",
                unit->getObjectID(),unit->attackPos.isValid(),unit->findTargetTimer,int(unit->pendingTargetRequest),
                candidate?candidate->getObjectID():NONE_ID,candidate?candidate->getItemID():NONE_ID);
            SDL_Log("CAMPAIGN_UNIT_AUDIT: house=%d id=%u item=%u hp=%d/%d active=%d moving=%d forced=%d mode=%d pos=%d,%d dest=%d,%d guard=%d,%d target=%u target_item=%u target_pos=%d,%d",
                unit->getOwner()->getHouseID(),unit->getObjectID(),unit->getItemID(),unit->getHealth().lround(),unit->getMaxHealth(),
                unit->isActive(),unit->isMoving(),unit->wasForced(),int(unit->getAttackMode()),
                unit->getLocation().x,unit->getLocation().y,unit->getDestination().x,unit->getDestination().y,
                unit->getGuardPoint().x,unit->getGuardPoint().y,target?target->getObjectID():NONE_ID,
                target?target->getItemID():NONE_ID,target?target->getLocation().x:-1,target?target->getLocation().y:-1);
        }
        for (const auto* building:structureList)
            SDL_Log("CAMPAIGN_BUILDING_AUDIT: house=%d item=%u hp=%d/%d pos=%d,%d",
                building->getOwner()->getHouseID(),building->getItemID(),building->getHealth().lround(),building->getMaxHealth(),
                building->getLocation().x,building->getLocation().y);
    }
    game.reset();
    currentGame = nullptr;
    pLocalPlayer = nullptr;
    return MENU_QUIT_DEFAULT;
}
