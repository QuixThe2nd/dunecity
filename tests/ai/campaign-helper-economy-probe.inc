// Real campaign helper, above-normal market prices and competing producers.
void runCampaignHelperEconomyProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    auto* house=game.getHouse(HOUSE_HARKONNEN);
    QuantBot* bot=nullptr;
    for(const auto& player:house->getPlayerList())
        if(auto* candidate=dynamic_cast<QuantBot*>(player.get()))bot=candidate;
    require(bot && !bot->isCampaignEnemy(),"Missing human-side campaign helper");
    require(bot->difficulty>=QuantBot::Difficulty::Hard,"Fixture requires Hard/Brutal helper");
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
    for(int i=0;i<3;++i)require(place(Structure_Refinery),"Refinery placement failed");
    auto* factory=dynamic_cast<BuilderBase*>(place(Structure_HeavyFactory));
    auto* port=dynamic_cast<StarPort*>(place(Structure_StarPort));
    require(factory && port,"Producer placement failed");
    while(house->getNumItems(Unit_Harvester)<8)house->createUnit(Unit_Harvester,true);
    if(!house->getNumItems(Unit_Carryall))house->createUnit(Unit_Carryall,true);
    auto& market=house->getChoam();
    for(auto& item:market.availableItems) {
        item.num=item.itemID==Unit_Harvester ? 10 : 0;
        if(item.itemID==Unit_Harvester)item.price=900;
    }
    port->updateBuildList();
    require(!market.isCheap(Unit_Harvester),"Fixture requires above-market harvesters");
    bot->gameMode=QuantBot::GameMode::Custom;bot->harvesterLimit=15;
    bot->militaryValueLimit=40000;bot->lastCalculatedSpice=36000;
    house->takeCredits(house->getCredits());house->addCredits(9000);
    bot->build(4200);
    auto queuedWorkers=[&]() {
        int count=0;for(const auto& item:port->currentProductionQueue)count+=item.itemID==Unit_Harvester;
        return count;
    };
    require(queuedWorkers()==7,"Helper failed to fill its 15-worker target before other spending");
    require(!port->okToOrder(),"Worker order was not dispatched");
    // Paid cargo must count toward the target but not drain the budget twice.
    // Isolate its unpaid-cash test from unrelated construction commitments.
    auto saved=structureList;structureList.clear();structureList.push_back(port);structureList.push_back(factory);
    house->takeCredits(house->getCredits());house->addCredits(2500);
    factory->currentProductionQueue.clear();factory->updateBuildList();
    try {bot->build(4200);}catch(...){structureList=saved;throw;}
    structureList=saved;
    require(queuedWorkers()==7,"Pending deliveries were ordered twice");
    require(factory->getProductionQueueSize()>0,"Paid Starport cargo reserved cash again and blocked production");
    require(factory->getCurrentProducedItem()!=Unit_Harvester,"Pending workers did not count toward helper target");
    SDL_Log("CAMPAIGN_HELPER_ECONOMY_PASS: eight workers plus seven above-market imports reach 15; pending cargo counted once, cash not reserved twice");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=helper_economy outcome=passed");
}
