// Real queues, market, placement, power and household accounting in both mods.
void runSharedSpendingProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("SHARED_SPENDING_FAIL: %s",why);throw std::runtime_error(why);}};
    auto* house=game.getHouse(HOUSE_HARKONNEN);
    auto* bot=dynamic_cast<QuantBot*>(game.getPlayerByName("Harkonnen"));
    require(house && bot,"Missing fixture bot");
    game.gameType=GameType::CustomGame;
    bot->gameMode=QuantBot::GameMode::Custom;bot->difficulty=QuantBot::Difficulty::Brutal;
    bot->militaryValueLimit=20000;bot->harvesterLimit=12;
    auto place=[&](Uint32 item)->StructureBase* {
        const Coord size=getStructureSize(item);
        for(int y=2;y+size.y<currentGameMap->getSizeY();++y)for(int x=2;x+size.x<currentGameMap->getSizeX();++x) {
            bool clear=true;
            for(int dy=0;dy<size.y;++dy)for(int dx=0;dx<size.x;++dx)
                clear &= !currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject();
            if(clear)return house->placeStructure(NONE_ID,item,x,y,true,true);
        }
        throw std::runtime_error("No shared spending fixture space");
    };
    for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
        if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Rock);
    for(Uint32 item:{Structure_WindTrap,Structure_Radar,Structure_LightFactory,Structure_HeavyFactory,
        Structure_HighTechFactory,Structure_StarPort,Structure_RepairYard,Structure_IX,Structure_Palace})
        if(!house->getNumItems(item))require(place(item),"Prerequisite placement failed");
    while(house->getNumItems(Structure_Refinery)<4)place(Structure_Refinery);
    while(house->getNumItems(Unit_Harvester)<2)house->createUnit(Unit_Harvester,true);
    if(!house->getNumItems(Unit_Carryall))house->createUnit(Unit_Carryall,true);
    if(game.isCitySimEnabled()) {
        auto& hs=game.getCitySimulation()->houseState_[HOUSE_HARKONNEN];
        hs.resValve=2000;hs.comValve=1500;hs.indValve=1500;
        hs.avgLandValue=160;hs.taxBaseEighths=8000;
        hs.civicDemandBlocked=0;
    }
    BuilderBase *yard=nullptr,*heavy=nullptr;
    StructureBase* refinery=nullptr;
    for(auto* structure:structureList)if(structure->getOwner()==house) {
        if(structure->getItemID()==Structure_Refinery)refinery=structure;
        if(!structure->isABuilder())continue;
        auto* builder=static_cast<BuilderBase*>(structure);
        builder->upgrading=false;builder->updateBuildList();
        if(builder->getItemID()==Structure_ConstructionYard)yard=builder;
        if(builder->getItemID()==Structure_HeavyFactory)heavy=builder;
    }
    require(yard && heavy && refinery,"Missing real producers");
    // A nearby field makes workers worthwhile; a large remaining reserve is
    // supplied explicitly, just as the normal terrain survey does before build.
    for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx) {
        const int x=refinery->getX()+dx,y=refinery->getY()+dy;
        if(currentGameMap->tileExists(x,y) && !currentGameMap->getTile(x,y)->hasAGroundObject()) {
            currentGameMap->getTile(x,y)->setType(Terrain_Spice);
            currentGameMap->getTile(x,y)->setSpice(10000_fix);
        }
    }
    auto* secondYard=static_cast<BuilderBase*>(place(Structure_ConstructionYard));
    secondYard->upgrading=false;secondYard->updateBuildList();
    const auto saved=structureList;
    auto reset=[&](int credits,int spice) {
        for(auto* builder:{yard,heavy,secondYard}) {
            while(builder->getProductionQueueSize())builder->doCancelItem(builder->getCurrentProducedItem());
            builder->upgrading=false;
        }
        bot->builderPlaceLocations.clear();bot->reservedStructures.clear();bot->clearPlacementCache();
        bot->lastCalculatedSpice=spice;
        house->producedPower=10000;house->takeCredits(house->getCredits());house->addCredits(credits);
        structureList.clear();structureList.push_back(yard);structureList.push_back(heavy);structureList.push_back(refinery);
    };
    auto count=[&](BuilderBase* builder,Uint32 item) {
        return std::count_if(builder->currentProductionQueue.begin(),builder->currentProductionQueue.end(),
            [&](const auto& order){return order.itemID==item;});
    };
    const int price=game.objectData.data[Unit_Harvester][HOUSE_HARKONNEN].price;
    try {
        reset(price,1000000);bot->build(10000);
        require(count(heavy,Unit_Harvester)==1,"Next worthwhile worker lost its exact purchase budget");
        require(yard->getProductionQueueSize()==0,"Yard spent the worker's protected cash");
        reset(price+1000,1000000);bot->build(10000);
        require(count(heavy,Unit_Harvester)==1,"Funded parallel worker missing");
        require(yard->getProductionQueueSize()>0,"Independent construction failed to use remaining cash");
        if(game.isCitySimEnabled()) {
            bool zone=false;
            for(const auto& order:yard->currentProductionQueue)zone|=DuneCity::isCityZoneStructure(order.itemID);
            require(zone,"City did not grow demanded R/I/C alongside worker");
        }
        if (game.isCitySimEnabled()) {
            reset(price+1000,1000000);
            structureList.push_back(secondYard);
            bot->build(10000);
            for (auto* builder:{yard,secondYard}) {
                bool zone=false;
                for (const auto& order:builder->currentProductionQueue) zone|=DuneCity::isCityZoneStructure(order.itemID);
                require(zone,"Funded second construction yard failed to grow R/I/C in parallel");
            }
            require(count(heavy,Unit_Harvester)==1,"Parallel yards consumed the worker reserve");
        }
        reset(3000,0);bot->build(10000);
        require(count(heavy,Unit_Harvester)==0 && count(yard,Structure_Refinery)==0,"Depleted spice still attracts economy spending");
        require(heavy->getProductionQueueSize()>0,"Military stopped when spice investment ceased");
        reset(200,1000000);bot->build(10000);
        require(heavy->getProductionQueueSize()==0 && yard->getProductionQueueSize()==0,"Spent savings below next worker cost");
        if(game.isCitySimEnabled()) {
            auto& hs=game.getCitySimulation()->houseState_[HOUSE_HARKONNEN];
            hs.resValve=hs.comValve=hs.indValve=-1000;hs.taxBaseEighths=0;
        }
        reset(1000,0);heavy->doProduceItem(Unit_Tank);bot->build(10000);
        require(!count(yard,Structure_HeavyFactory),"Added a factory while existing line was cash-starved");
        reset(40000,0);heavy->doProduceItem(Unit_Tank);bot->build(1000);
        require(count(yard,Structure_HeavyFactory)==1,"Funded military backlog did not add production capacity");
        SDL_Log("SHARED_SPENDING_PASS: exact worker reserve, simultaneous construction, depletion, scarce credits (%s)",game.isCitySimEnabled()?"city":"vanilla");
    }catch(...){structureList=saved;throw;}
    structureList=saved;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=shared-spending outcome=passed");
}
