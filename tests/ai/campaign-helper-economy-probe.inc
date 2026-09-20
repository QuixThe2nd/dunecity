// Real campaign helper, above-normal market prices and competing producers.
void runCampaignHelperEconomyProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("CAMPAIGN_HELPER_ECONOMY_FAIL: %s",why);throw std::runtime_error(why);}};
    auto* house=game.getHouse(HOUSE_HARKONNEN);
    QuantBot* bot=nullptr;
    for(const auto& player:house->getPlayerList())
        if(auto* candidate=dynamic_cast<QuantBot*>(player.get()))bot=candidate;
    require(bot && !bot->isCampaignEnemy(),"Missing human-side campaign helper");
    require(bot->difficulty>=QuantBot::Difficulty::Hard,"Fixture requires Hard/Brutal helper");
    if (game.isCitySimEnabled()) {
        // SCENH022, seed 1221113892: cramped human start, no income or factory.
        require(house->getCredits()==1000 && house->getNumItems(Structure_ConstructionYard)==1
            && house->getNumItems(Structure_HeavyFactory)==0 && house->getNumItems(Structure_Refinery)==0,
            "Run city helper opening fixture on the unmodified Harkonnen level-nine start");
        bot->build(4200);
        require(bot->rockExpansionSite.isValid(),"Fixture must offer an expansion that could reserve MCV cash");
        bool windtrapQueued=false;
        for (const auto* structure:structureList)
            if (structure->getOwner()==house && structure->getItemID()==Structure_ConstructionYard)
                windtrapQueued=static_cast<const BuilderBase*>(structure)->getCurrentProducedItem()==Structure_WindTrap;
        require(windtrapQueued,"Helper reserved nonexistent factory's MCV cash instead of buying its first windtrap");
        while (!game.finished && game.getGameCycleCount()<MILLI2CYCLES(300000)) game.updateGameState();
        require(house->getNumItems(Structure_WindTrap)>0 && house->getNumItems(Structure_Refinery)>0
            && house->getNumItems(Unit_Harvester)>0,"Helper failed to establish power and spice income within five minutes");
        SDL_Log("CAMPAIGN_HELPER_ECONOMY_PASS: cramped 1000-credit city start buys power, refinery and harvester before reserving expansion money");
        SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=helper_city_opening outcome=passed");
        return;
    }
    auto place=[&](Uint32 item) {
        for(int y=1;y+3<currentGameMap->getSizeY();y+=4)
            for(int x=1;x+3<currentGameMap->getSizeX();x+=4) {
                bool clear=true;
                for(int dy=0;dy<3;++dy)for(int dx=0;dx<3;++dx)
                    if(currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject())clear=false;
                if(clear)return house->placeStructure(NONE_ID,item,x,y,true,true);
            }
        throw std::runtime_error("No helper fixture site");
    };
    require(place(Structure_WindTrap),"Windtrap placement failed");
    for(int i=0;i<3;++i)require(place(Structure_Refinery),"Refinery placement failed");
    auto* factory=dynamic_cast<BuilderBase*>(place(Structure_HeavyFactory));
    auto* port=dynamic_cast<StarPort*>(place(Structure_StarPort));
    require(factory && port,"Producer placement failed");
    while(house->getNumItems(Unit_Harvester)<8)require(house->createUnit(Unit_Harvester,true),"Worker creation failed");
    if(!house->getNumItems(Unit_Carryall))house->createUnit(Unit_Carryall,true);
    auto& market=house->getChoam();
    for(auto& item:market.availableItems) {
        item.num=item.itemID==Unit_Harvester ? 5 : 0;
        if(item.itemID==Unit_Harvester)item.price=900;
    }
    port->updateBuildList();
    require(!market.isCheap(Unit_Harvester),"Fixture requires above-market harvesters");
    game.gameInitSettings.gameOptions.maximumNumberOfHarvestersOverride=15;
    bot->gameMode=QuantBot::GameMode::Custom;bot->harvesterLimit=15;
    bot->militaryValueLimit=40000;bot->lastCalculatedSpice=36000;
    house->takeCredits(house->getCredits());house->addCredits(9000);
    auto* yard=dynamic_cast<BuilderBase*>(place(Structure_ConstructionYard));
    require(yard,"Construction yard placement failed");
    yard->updateBuildList();
    yard->doProduceItem(Structure_Refinery);
    require(yard->getCurrentProducedItem()==Structure_Refinery,"Pending refinery setup failed");
    auto initialStructures=structureList;
    structureList.clear();structureList.push_back(port);structureList.push_back(yard);
    try {bot->build(4200);}catch(...){structureList=initialStructures;throw;}
    structureList=initialStructures;
    auto queuedWorkers=[&]() {
        int count=0;for(const auto& item:port->currentProductionQueue)count+=item.itemID==Unit_Harvester;
        return count;
    };
    require(queuedWorkers()==5,"Helper failed to buy all available workers toward its 15-worker target");
    require(!port->okToOrder(),"Worker order was not dispatched");
    // While five paid workers are in transit, a second port can buy exactly
    // the remaining one: the queued refinery supplies the other. Neither double reservation nor duplicate orders fit.
    auto* second=dynamic_cast<StarPort*>(place(Structure_StarPort));
    require(second,"Second port placement failed");
    for(auto& item:market.availableItems)if(item.itemID==Unit_Harvester)item.num=2;
    second->updateBuildList();
    auto saved=structureList;structureList.clear();structureList.push_back(port);structureList.push_back(second);structureList.push_back(yard);
    house->takeCredits(house->getCredits());house->addCredits(1800);
    try {bot->build(4200);}catch(...){structureList=saved;throw;}
    structureList=saved;
    int remaining=0;for(const auto& item:second->currentProductionQueue)remaining+=item.itemID==Unit_Harvester;
    require(queuedWorkers()==5 && remaining==1,"Paid cargo blocked later orders or was counted twice");
    require(house->getCredits()==900,"One worker import was not charged exactly once");
    // Another refinery would finish before paid cargo and create a sixteenth
    // worker. Capacity expansion must wait until those workers have arrived.
    require(place(Structure_LightFactory),"Light factory setup failed");
    require(place(Structure_Radar),"Radar setup failed");
    require(place(Structure_RepairYard),"Repair yard setup failed");
    auto* capacityYard=dynamic_cast<BuilderBase*>(place(Structure_ConstructionYard));
    require(capacityYard,"Capacity yard placement failed");
    capacityYard->updateBuildList();
    house->addCredits(20000);
    bot->build(4200);
    for(const auto& item:capacityYard->currentProductionQueue)
        require(item.itemID!=Structure_Refinery,"Refinery can overtake paid workers and exceed the cap");
    SDL_Log("CAMPAIGN_HELPER_ECONOMY_PASS: eight workers plus one refinery worker and six above-market imports reach 15; paid cargo counted once, capacity expansion deferred");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=helper_economy outcome=passed");
}
