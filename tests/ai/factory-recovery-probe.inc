// Reproduce the browser Harkonnen loss: both vehicle factories destroyed,
// then a busy replacement light line must not cause more light factories.
void runFactoryRecoveryProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("FACTORY_RECOVERY_FAIL: %s",why);throw std::runtime_error(why);}};
    auto* house=game.getHouse(HOUSE_HARKONNEN);
    auto* bot=dynamic_cast<QuantBot*>(game.getPlayerByName("Harkonnen"));
    require(house && bot,"Missing Harkonnen partner");
    game.gameType=GameType::CustomGame;
    bot->gameMode=QuantBot::GameMode::Custom;bot->difficulty=QuantBot::Difficulty::Brutal;
    bot->militaryValueLimit=40000;bot->harvesterLimit=6;bot->lastCalculatedSpice=100000;
    // Prepared sites isolate replacement priority from separately budgeted concrete.
    for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
        if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Slab);
    auto place=[&](Uint32 item)->StructureBase* {
        const Coord size=getStructureSize(item);
        for(int y=2;y+size.y<currentGameMap->getSizeY();++y)for(int x=2;x+size.x<currentGameMap->getSizeX();++x) {
            bool clear=true;
            for(int dy=0;dy<size.y;++dy)for(int dx=0;dx<size.x;++dx)
                clear &= !currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject();
            if(clear)return house->placeStructure(NONE_ID,item,x,y,true,true);
        }
        throw std::runtime_error("No factory fixture space");
    };
    BuilderBase* yard=nullptr;
    for(auto* building:structureList)if(building->getOwner()==house && building->getItemID()==Structure_ConstructionYard)
        yard=static_cast<BuilderBase*>(building);
    require(yard,"Missing fixture yard");
    for(Uint32 item:{Structure_WindTrap,Structure_Radar,Structure_StarPort,Structure_RepairYard})
        if(!house->getNumItems(item))require(place(item),"Missing prerequisite fixture");
    while(house->getNumItems(Structure_Refinery)<4)place(Structure_Refinery);
    auto* light=static_cast<BuilderBase*>(place(Structure_LightFactory));
    auto* heavy=place(Structure_HeavyFactory);
    require(light && heavy,"Missing vehicle factories");
    heavy->destroy();light->destroy();
    require(house->getNumLostItems(Structure_HeavyFactory)==1,"Factory loss not recorded");
    house->producedPower=10000;house->takeCredits(house->getCredits());house->addCredits(4000);
    yard->upgrading=false;yard->updateBuildList();
    auto resetYard=[&] {
        while(yard->getProductionQueueSize())yard->doCancelItem(yard->getCurrentProducedItem());
        bot->builderPlaceLocations.clear();bot->reservedStructures.clear();bot->clearPlacementCache();
        yard->upgrading=false;
    };
    auto queued=[&](Uint32 item) {
        return std::count_if(yard->currentProductionQueue.begin(),yard->currentProductionQueue.end(),
            [&](const auto& order){return order.itemID==item;});
    };
    // Isolate construction decisions; the real build list, orders, household
    // counts and strategic mix still run. Other factories cannot spend the cash.
    auto saved=structureList;structureList.clear();structureList.push_back(yard);
    try {
        bot->build(4000);
        require(queued(Structure_LightFactory)==1,"Lost heavy did not recover missing light prerequisite first");
        require(bot->getLastUnitMixBps()[0]>0 && bot->getLastUnitMixBps()[7]<10000,
            "Losing heavy producer changed strategic mix to all quads");
        resetYard();
        light=static_cast<BuilderBase*>(place(Structure_LightFactory));saved.push_back(light);
        light->updateBuildList();
        structureList.clear();structureList.push_back(yard);
        yard->updateBuildList();
        require(yard->isAvailableToBuild(Structure_HeavyFactory),"Heavy unavailable after replacement light");
        const int price=game.objectData.data[Structure_HeavyFactory][HOUSE_HARKONNEN].price;
        house->takeCredits(house->getCredits());house->addCredits(price-1);
        bot->build(4000);
        require(yard->getProductionQueueSize()==0,"Spent recovery savings on optional construction");
        house->addCredits(1);bot->build(4000);
        require(queued(Structure_HeavyFactory)==1 && !queued(Structure_LightFactory),"Did not rebuild heavy at its exact price");
        bot->build(4000);require(queued(Structure_HeavyFactory)==1,"Duplicated pending heavy replacement");
        resetYard();
        // Recreate the funded, saturated light-factory backlog from the report.
        structureList.push_back(light);
        light->doProduceItem(Unit_Quad);house->addCredits(10000);bot->build(4000);
        require(queued(Structure_HeavyFactory)==1 && !queued(Structure_LightFactory),"Busy light factory displaced heavy recovery");
        resetYard();
        for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
            if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Sand);
        bot->build(4000);
        require(!queued(Structure_HeavyFactory),"Queued unplaceable heavy factory");
        require(!queued(Structure_LightFactory),"Unplaceable heavy caused extra light factories");
    }catch(...){structureList=saved;throw;}
    structureList=saved;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=factory-recovery outcome=passed");
}
