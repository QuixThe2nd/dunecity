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
    // Sold-out workers must not turn their reserved budget into military or
    // extra carryalls. Once stock returns, use the saved money in bulk.
    auto* restockPort = dynamic_cast<StarPort*>(place(Structure_StarPort));
    for (auto& offer : market.availableItems) {
        offer.num = (offer.itemID == Unit_Quad || offer.itemID == Unit_Carryall) ? 5 : 0;
        offer.price = offer.itemID == Unit_Harvester ? 300 : 100;
    }
    restockPort->updateBuildList(); house->addCredits(800);
    bot->difficulty = QuantBot::Difficulty::Hard;
    auto planPorts = [&](StarPort* extra) {
        savedStructures = structureList;
        structureList.clear(); structureList.push_back(port); structureList.push_back(restockPort);
        if (extra) structureList.push_back(extra);
        try { bot->build(0); } catch (...) { structureList = savedStructures; throw; }
        structureList = savedStructures;
    };
    planPorts(nullptr);
    if (!restockPort->currentProductionQueue.empty() || house->getCredits() != 800)
        throw std::runtime_error("Sold-out workers' reserve was spent on military or extra carryalls");
    for (auto& offer : market.availableItems) if (offer.itemID == Unit_Harvester) offer.num = 2;
    restockPort->updateBuildList(); planPorts(nullptr);
    int restockedWorkers = 0;
    for (const auto& queued : restockPort->currentProductionQueue) restockedWorkers += queued.itemID == Unit_Harvester;
    if (restockedWorkers != 2 || restockPort->currentProductionQueue.size() != 2
        || house->getCredits() != 200 || restockPort->okToOrder())
        throw std::runtime_error("Restocked workers did not get the saved cash before military");
    SDL_Log("STARPORT_WORKER_RESERVE_PASS: saved 800 while workers sold out; bought two at 300 on restock, retained 200");
    // Once the worker target is covered by existing and paid cargo, release
    // the reserve and buy all cheap combat types below the old 2000 floor.
    auto* bargainPort = dynamic_cast<StarPort*>(place(Structure_StarPort));
    bot->harvesterLimit = house->getNumItems(Unit_Harvester) + workers + restockedWorkers;
    for (auto& offer : market.availableItems) {
        offer.num = (offer.itemID == Unit_Quad || offer.itemID == Unit_Tank
            || offer.itemID == Unit_Launcher || offer.itemID == Unit_SiegeTank) ? 2 : 0;
        offer.price = offer.itemID == Unit_Harvester ? 300 : 100;
    }
    bargainPort->updateBuildList();
    house->takeCredits(house->getCredits()); house->addCredits(800);
    planPorts(bargainPort);
    for (Uint32 type : {Unit_Quad, Unit_Tank, Unit_Launcher, Unit_SiegeTank}) {
        int count = 0;
        for (const auto& queued : bargainPort->currentProductionQueue) count += queued.itemID == type;
        if (count != 2) throw std::runtime_error("Did not buy every cheap combat type in bulk");
    }
    if (house->getCredits() != 0 || bargainPort->okToOrder())
        throw std::runtime_error("Bargain order overspent or was not dispatched");
    SDL_Log("STARPORT_BARGAIN_PASS: 800 credits buy eight units across four types, including quads");
    // The reported regression: six existing workers, 180-credit harvesters,
    // and a 15-worker human-allied ceiling. Fill all nine vacancies at once,
    // even if a legacy/remaining-spice planner target still says six.
    auto* workerPort = dynamic_cast<StarPort*>(place(Structure_StarPort));
    house->maxHarvesters = 15;
    game.gameInitSettings.gameOptions.maximumNumberOfHarvestersOverride = 15;
    bot->difficulty = QuantBot::Difficulty::Brutal;
    while (house->getNumItems(Unit_Harvester) < 6) house->createUnit(Unit_Harvester,true);
    bot->harvesterLimit = 6; bot->lastCalculatedSpice = 20000;
    for (auto& offer : market.availableItems) {
        offer.num = offer.itemID == Unit_Harvester ? 9 : offer.itemID == Unit_Carryall ? 1 : 0;
        if (offer.itemID == Unit_Carryall) offer.price = 800;
        if (offer.itemID == Unit_Harvester) offer.price = 180;
    }
    workerPort->updateBuildList();house->addCredits(2420);
    savedStructures = structureList;
    structureList.clear();structureList.push_back(workerPort);
    try { bot->build(0); } catch (...) { structureList=savedStructures;throw; }
    structureList=savedStructures;
    workers=0;transports=0;
    for (const auto& queued : workerPort->currentProductionQueue) {
        workers += queued.itemID==Unit_Harvester;
        transports += queued.itemID==Unit_Carryall;
    }
    if (workers!=9 || transports!=1 || house->getCredits()!=0 || workerPort->okToOrder()
        || workerPort->currentProductionQueue.front().itemID!=Unit_Carryall)
        throw std::runtime_error("Human-allied Brutal failed to fill 15-worker fleet with nine bargain imports");
    SDL_Log("STARPORT_ALLIED_WORKERS_PASS: first carryall at 800, then six existing + nine workers at 180 = 15; paid 2420");
    // A factory must be able to spend that reserve on a worker, rather than
    // freezing behind its own protection or choosing a military unit/upgrade.
    auto* factory = dynamic_cast<BuilderBase*>(place(Structure_HeavyFactory));
    if (!factory) throw std::runtime_error("Worker factory fixture placement failed");
    factory->updateBuildList(); bot->harvesterLimit = 15; bot->lastCalculatedSpice = 200000;
    bot->rockExpansionSite = Coord::Invalid(); bot->rockSurveyCycle = game.getGameCycleCount();
    const int workerPrice = game.objectData.data[Unit_Harvester][HOUSE_HARKONNEN].price;
    house->takeCredits(house->getCredits()); house->addCredits(workerPrice);
    savedStructures = structureList;
    structureList.clear(); structureList.push_back(factory);
    try { bot->build(4000); } catch (...) { structureList = savedStructures; throw; }
    structureList = savedStructures;
    if (factory->getCurrentProducedItem() != Unit_Harvester || factory->isUpgrading())
        throw std::runtime_error("Factory failed to spend protected worker cash on a harvester");
    SDL_Log("FACTORY_WORKER_RESERVE_PASS: one worker's cash funds a harvester before military or upgrades");
    // Custom games buy workers and combat together, without the campaign
    // helper's full-fleet cash reservation or enemy seven-worker ceiling.
    game.gameType=GameType::CustomGame;
    bot->harvesterLimit=15;bot->lastCalculatedSpice=200000;
    if (bot->harvesterCountCeiling()!=0) throw std::runtime_error("Custom ally inherited campaign worker cap");
    for (int h=0;h<NUM_HOUSES;++h) if(auto* other=game.getHouse(h))
        for(const auto& player:other->getPlayerList()) if(auto* qb=dynamic_cast<QuantBot*>(player.get())) {
            qb->difficulty=QuantBot::Difficulty::Brutal;
            if(qb->harvesterCountCeiling()!=0) throw std::runtime_error("Custom opponent inherited campaign worker cap");
        }
    auto* mixedPort=dynamic_cast<StarPort*>(place(Structure_StarPort));
    for(auto& offer:market.availableItems) {
        offer.num=offer.itemID==Unit_Harvester ? 10 : offer.itemID==Unit_Tank ? 20 : 0;
        offer.price=offer.itemID==Unit_Harvester ? 180 : 100;
    }
    mixedPort->updateBuildList();house->takeCredits(house->getCredits());house->addCredits(1800);
    savedStructures=structureList;structureList.clear();structureList.push_back(mixedPort);
    try {bot->build(4000);}catch(...){structureList=savedStructures;throw;}
    structureList=savedStructures;
    int customWorkers=0,customTanks=0;
    for(const auto& queued:mixedPort->currentProductionQueue) {
        customWorkers+=queued.itemID==Unit_Harvester;customTanks+=queued.itemID==Unit_Tank;
    }
    if(customWorkers<1 || customTanks<1 || house->getCredits()<0
        || house->getCredits()!=1800-customWorkers*180-customTanks*100)
        throw std::runtime_error("Custom Starport failed to mix worker and army investment");
    SDL_Log("CUSTOM_MIXED_IMPORTS_PASS: workers=%d tanks=%d, campaign fleet reserve absent",customWorkers,customTanks);
    // Market prices can refresh before this port's build list does. The real
    // queue charges the displayed offer: never budget the newer market quote.
    auto* quotedPort=dynamic_cast<StarPort*>(place(Structure_StarPort));
    for (auto& offer:market.availableItems) {
        offer.num=offer.itemID==Unit_Tank ? 20 : 0;
        offer.price=120;
    }
    quotedPort->updateBuildList();
    for (auto& offer:market.availableItems) offer.price=80;
    bot->lastCalculatedSpice=0;
    house->takeCredits(house->getCredits());house->addCredits(350);
    savedStructures=structureList;structureList.clear();structureList.push_back(quotedPort);
    try {bot->build(4000);}catch(...){structureList=savedStructures;throw;}
    structureList=savedStructures;
    if (quotedPort->currentProductionQueue.size()!=2 || house->getCredits()!=110)
        throw std::runtime_error("Shared budget did not use Starport's actual quoted price");
    for (const auto& order:quotedPort->currentProductionQueue)
        if (order.itemID!=Unit_Tank || order.price!=120)
            throw std::runtime_error("Starport quote fixture ordered unexpected cargo");
    SDL_Log("STARPORT_QUOTED_PRICE_PASS: market 80, displayed/charged 120; 350 funds two tanks with 110 remaining");
    // Reported opening: workers sold out, first transport available for 640,
    // but optional construction gets the money first. Exercise both the custom
    // and allied campaign planning paths with real yard/Starport queues.
    BuilderBase* openingYard=nullptr;
    for(auto* building:structureList)
        if(building->getOwner()==house && building->getItemID()==Structure_ConstructionYard)
            openingYard=static_cast<BuilderBase*>(building);
    if(!openingYard) throw std::runtime_error("No yard for carryall priority fixture");
    openingYard->upgrading=false;openingYard->updateBuildList();
    for(const auto gameType:{GameType::CustomGame,GameType::Campaign}) {
        game.gameType=gameType;bot->gameMode=QuantBot::GameMode::Custom;
        auto* transportPort=dynamic_cast<StarPort*>(place(Structure_StarPort));
        auto* secondPort=dynamic_cast<StarPort*>(place(Structure_StarPort));
        for(auto& offer:market.availableItems) {
            offer.num=offer.itemID==Unit_Carryall ? 5 : offer.itemID==Unit_Tank ? 5 : 0;
            offer.price=offer.itemID==Unit_Carryall ? 640 : 100;
        }
        transportPort->updateBuildList();secondPort->updateBuildList();
        house->producedPower=10000;bot->lastCalculatedSpice=200000;
        house->takeCredits(house->getCredits());house->addCredits(639);
        savedStructures=structureList;structureList.clear();
        structureList.push_back(openingYard);structureList.push_back(transportPort);structureList.push_back(secondPort);
        try {
            bot->build(4000);
            if(transportPort->getProductionQueueSize() || secondPort->getProductionQueueSize()
                || openingYard->getProductionQueueSize() || house->getCredits()!=639)
                throw std::runtime_error("Cheap troops or construction consumed first carryall savings");
            house->addCredits(361);
            bot->build(4000);
        }catch(...){structureList=savedStructures;throw;}
        structureList=savedStructures;
        int firstTransports=0;
        for(auto* candidate:{transportPort,secondPort})for(const auto& order:candidate->currentProductionQueue)
            firstTransports+=order.itemID==Unit_Carryall;
        if(firstTransports!=1 || transportPort->currentProductionQueue.empty()
            || transportPort->currentProductionQueue.front().itemID!=Unit_Carryall || transportPort->okToOrder())
            throw std::runtime_error("First carryall lost priority or was duplicated across Starports");
        if(openingYard->getCurrentProducedItem()==Structure_RepairYard)
            throw std::runtime_error("Repair yard spent the first carryall's cash");
        while(openingYard->getProductionQueueSize())openingYard->doCancelItem(openingYard->getCurrentProducedItem());
        bot->builderPlaceLocations.clear();bot->reservedStructures.clear();
        SDL_Log("STARPORT_FIRST_TRANSPORT_PASS: sold-out workers, first carryall before repair yard, two ports, mode=%d",int(gameType));
    }
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=starport outcome=passed");
}
