// Real-game fixture, included only by the diagnostic main.
#include <players/QuantBot.h>
#include <structures/StarPort.h>
#include <Choam.h>
#include <Map.h>
#include <stdexcept>

void runStarportEconomyProbe(Game& game) {
    auto* house = game.getHouse(HOUSE_HARKONNEN);
    auto* bot = dynamic_cast<QuantBot*>(game.getPlayerByName("Harkonnen"));
    if (!house || !bot) throw std::runtime_error("Starport fixture requires the Harkonnen AI partner");
    auto place = [&](Uint32 item) -> StructureBase* {
        for (int y=1; y<50; y+=4) for (int x=1; x<50; x+=4) {
            bool clear=true;
            for (int dy=0; dy<3; ++dy) for (int dx=0; dx<3; ++dx)
                if (currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject()) clear=false;
            if (clear) return house->placeStructure(NONE_ID,item,x,y,true,true);
        }
        throw std::runtime_error("No free fixture site");
    };
    for (int i=0; i<3; ++i) if (!place(Structure_Refinery)) throw std::runtime_error("Fixture refinery failed");
    if (!house->getNumItems(Structure_WindTrap)) place(Structure_WindTrap);
    for (auto* building : structureList) if (building->getOwner() == house
        && building->getItemID() == Structure_ConstructionYard) {
        auto* yard = static_cast<BuilderBase*>(building);
        yard->updateBuildList();
        if (!yard->isAvailableToBuild(Structure_StarPort))
            throw std::runtime_error("Starport unavailable before city population develops");
    }
    if (game.isCitySimEnabled()) {
        bot->initialItemCount[Structure_RepairYard] = 0;
        for (auto tier : {QuantBot::Difficulty::Easy, QuantBot::Difficulty::Medium,
                         QuantBot::Difficulty::Hard, QuantBot::Difficulty::Brutal}) {
            bot->difficulty = tier;
            if (!bot->canAddRepairYard(0)) throw std::runtime_error("City difficulty cannot add a repair yard");
        }
        bot->difficulty = QuantBot::Difficulty::Easy;
    }
    auto* port = dynamic_cast<StarPort*>(place(Structure_StarPort));
    if (!port) throw std::runtime_error("Fixture Starport failed");
    if (!house->getNumItems(Unit_Harvester)) house->createUnit(Unit_Harvester,true);
    if (house->getNumItems(Unit_Carryall)) throw std::runtime_error("Fixture already has a carryall");
    auto& market = house->getChoam();
    for (auto& item : market.availableItems) {
        item.num = item.itemID == Unit_Harvester ? 5 : item.itemID == Unit_Carryall ? 1 : 0;
        if (item.itemID == Unit_Harvester) item.price = 1200;
        if (item.itemID == Unit_Carryall) item.price = 1500;
    }
    port->updateBuildList();
    if (market.isCheap(Unit_Harvester) || market.isCheap(Unit_Carryall))
        throw std::runtime_error("Fixture must exercise non-discounted prices");
    house->takeCredits(house->getCredits()); house->addCredits(2700);
    bot->gameMode = QuantBot::GameMode::Custom; // Full-control human partner mode.
    bot->harvesterLimit = 15; bot->militaryValueLimit = 20000;
    // The normal first AI update surveys spice before build(); this fixture
    // calls build directly, so populate that survey from the actual map.
    bot->lastCalculatedSpice = 0;
    for (int y=0; y<currentGameMap->getSizeY(); ++y)
        for (int x=0; x<currentGameMap->getSizeX(); ++x)
            bot->lastCalculatedSpice += currentGameMap->getTile(x,y)->getSpice().lround();
    // Isolate this producer for one planning pass. Household structures/counts
    // and the real Starport ordering/payment code remain intact.
    auto savedStructures = structureList;
    structureList.clear(); structureList.push_back(port);
    try { bot->build(0); } catch (...) { structureList = savedStructures; throw; }
    structureList = savedStructures;
    int workers=0, transports=0;
    for (const auto& queued : port->currentProductionQueue) {
        workers += queued.itemID == Unit_Harvester;
        transports += queued.itemID == Unit_Carryall;
    }
    if (workers != 1 || transports != 1 || house->getCredits() != 0 || port->okToOrder())
        throw std::runtime_error("Economic imports did not spend reserved cash at above-normal prices");
    SDL_Log("STARPORT_ECONOMY_PROBE_PASS: 2700 credits buy first carryall at 1500 and harvester at 1200; order placed, no overspend");
    // Below the former 2000-credit floor, buy all cheap combat types, including
    // quads excluded by the old hardcoded list. Workers are already covered by
    // the preceding order; no transport stock is available in this second case.
    auto* bargainPort = dynamic_cast<StarPort*>(place(Structure_StarPort));
    for (auto& offer : market.availableItems) {
        offer.num = (offer.itemID == Unit_Quad || offer.itemID == Unit_Tank
            || offer.itemID == Unit_Launcher || offer.itemID == Unit_SiegeTank) ? 2 : 0;
        offer.price = 100;
    }
    bargainPort->updateBuildList();
    house->addCredits(800);
    bot->difficulty = QuantBot::Difficulty::Hard;
    savedStructures = structureList;
    structureList.clear(); structureList.push_back(port); structureList.push_back(bargainPort);
    try { bot->build(0); } catch (...) { structureList = savedStructures; throw; }
    structureList = savedStructures;
    for (Uint32 type : {Unit_Quad, Unit_Tank, Unit_Launcher, Unit_SiegeTank}) {
        int count = 0;
        for (const auto& queued : bargainPort->currentProductionQueue) count += queued.itemID == type;
        if (count != 2) throw std::runtime_error("Did not buy every cheap combat type in bulk");
    }
    if (house->getCredits() != 0 || bargainPort->okToOrder())
        throw std::runtime_error("Bargain order overspent or was not dispatched");
    SDL_Log("STARPORT_BARGAIN_PASS: 800 credits buy eight units across four types, including quads");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=starport outcome=passed");
}
