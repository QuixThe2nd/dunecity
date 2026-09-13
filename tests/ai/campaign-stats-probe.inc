// Real results-screen regression, included only by the diagnostic main.
#include <Menu/CampaignStatsMenu.h>
#include <units/Harvester.h>
#include <FileClasses/LoadSavePNG.h>
#include <climits>

void runCampaignStatsProbe(Game& game, int level) {
    auto* local = game.getHouse(HOUSE_HARKONNEN);
    auto* enemy = game.getHouse(HOUSE_ATREIDES);
    auto* ally = game.getHouse(HOUSE_ORDOS);
    if (!local || !enemy || !ally || local != pLocalHouse || !local->isAI())
        throw std::runtime_error("Stats fixture requires a human house with a QuantBot partner");

    for (int h=0; h<NUM_HOUSES; ++h) if (auto* house = game.getHouse(h)) {
        house->numDestroyedUnits = house->numDestroyedStructures = house->destroyedValue = 0;
        house->harvestedSpice = 0;
        house->takeCredits(house->getCredits());
    }
    for (auto* unit : unitList) if (auto* harvester = dynamic_cast<Harvester*>(unit)) harvester->spice = 0;
    local->numDestroyedUnits = 7; local->numDestroyedStructures = 3;
    local->destroyedValue = 220; local->harvestedSpice = 1200; local->addCredits(900);
    enemy->numDestroyedUnits = 5; enemy->numDestroyedStructures = 2;
    enemy->destroyedValue = 70; enemy->harvestedSpice = 700;
    auto* cargo = dynamic_cast<Harvester*>(local->createUnit(Unit_Harvester,true));
    auto* enemyCargo = dynamic_cast<Harvester*>(enemy->createUnit(Unit_Harvester,true));
    if (!cargo || !enemyCargo) throw std::runtime_error("Stats fixture harvesters failed");
    cargo->spice = 100; enemyCargo->spice = 75;

    // An ordinary human campaign supplies the unchanged score/rank baseline.
    local->ai = false;
    CampaignStatsMenu human(level);
    const int ordinaryScore = human.totalScore;
    const auto ordinaryRank = human.rank;
    local->ai = true;
    CampaignStatsMenu assisted(level);
    if (assisted.unitsDestroyedByHuman != 7 || assisted.structuresDestroyedByHuman != 3
        || assisted.spiceHarvestedByHuman != 1300 || assisted.unitsDestroyedByAI != 5
        || assisted.structuresDestroyedByAI != 2 || assisted.spiceHarvestedByAI != 775
        || assisted.totalScore != ordinaryScore || assisted.rank != ordinaryRank)
        throw std::runtime_error("AI partner changed ownership of campaign score or statistics");

    // Controller type must not misclassify a human opponent or an AI ally either.
    enemy->ai = false;
    ally->teamID = local->getTeamID();
    ally->numDestroyedUnits = 2; ally->numDestroyedStructures = 1;
    ally->harvestedSpice = 300; ally->destroyedValue = 40; ally->addCredits(200);
    int alliedBuildingScore = 0;
    for (const auto* building : structureList) if (building->getOwner() == ally)
        alliedBuildingScore += game.objectData.data[building->getItemID()][building->getOriginalHouseID()].price / 100;
    CampaignStatsMenu teams(level);
    if (teams.unitsDestroyedByHuman != 9 || teams.structuresDestroyedByHuman != 4
        || teams.spiceHarvestedByHuman != 1600 || teams.unitsDestroyedByAI != 5
        || teams.structuresDestroyedByAI != 2 || teams.spiceHarvestedByAI != 775
        || teams.totalScore != ordinaryScore + 42 + alliedBuildingScore)
        throw std::runtime_error("Campaign statistics did not follow team ownership");

    const auto size = assisted.getSize();
    sdl2::texture_ptr target(SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size.x, size.y));
    if (!target || SDL_SetRenderTarget(renderer,target.get()) != 0)
        throw std::runtime_error("Stats render target failed");
    SDL_RenderSetLogicalSize(renderer,size.x,size.y);
    SDL_SetRenderDrawColor(renderer,0,0,0,255); SDL_RenderClear(renderer);
    while (assisted.currentState != CampaignStatsMenu::State_Finished) assisted.doState(INT_MAX);
    assisted.draw();
    auto screenshot = renderReadSurface(renderer);
    if (SavePNG(screenshot.get(),"campaign-assisted-stats.png") != 0)
        throw std::runtime_error("Stats screenshot failed");
    SDL_SetRenderTarget(renderer,nullptr);
    SDL_Log("CAMPAIGN_STATS_PROBE_PASS: AI partner retains human score/rank, harvest cargo, kills and buildings; allies and human opponents classified by team");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=stats outcome=passed");
}
