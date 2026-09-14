// Real queue/placement regression for a campaign nuclear footprint disappearing.
void runCampaignNuclearProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("NUCLEAR_FAIL: %s",why);throw std::runtime_error(why);}};
    require(game.isCitySimEnabled(),"Run nuclear fixture with Dune City mod");
    QuantBot* bot=nullptr;BuilderBase* yard=nullptr;
    for(auto* building:structureList) if(building->getItemID()==Structure_ConstructionYard)
        for(const auto& player:building->getOwner()->getPlayerList())
            if(auto* candidate=dynamic_cast<QuantBot*>(player.get());candidate && candidate->isCampaignEnemy()) {
                bot=candidate;yard=dynamic_cast<BuilderBase*>(building);break;
            }
    require(bot && yard,"Missing enemy construction yard");
    auto* house=const_cast<House*>(bot->getHouse());
    bot->difficulty=QuantBot::Difficulty::Easy;bot->gameMode=QuantBot::GameMode::Campaign;
    bot->initialMilitaryValue=1000;bot->militaryValueLimit=2000;bot->lastCalculatedSpice=100000;
    for(int i=ItemID_FirstID;i<=ItemID_LastID;++i)bot->initialItemCount[i]=house->getNumItems(i);
    ++bot->initialItemCount[Structure_NuclearPlant];
    house->addCredits(10000);house->powerRequirement=0;
    while(yard->getProductionQueueSize())yard->doCancelItem(yard->getCurrentProducedItem());
    yard->buildList.push_back(BuildItem(Structure_NuclearPlant,1000));
    yard->buildList.push_back(BuildItem(Structure_WindTrap,300));
    yard->upgrading=false;
    auto structures=structureList;structureList.clear();structureList.push_back(yard);
    try {
        for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
            if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Rock);
        bot->planningBuilder=yard->getObjectID();bot->clearPlacementCache();
        require(bot->findPlaceLocation(Structure_NuclearPlant).isValid(),"Fixture lacks initial nuclear site");
        bot->build(0);
        require(yard->getCurrentProducedItem()==Structure_NuclearPlant,"Campaign failed to order reactor");
        require(bot->reservedStructures.count(yard->getObjectID()),"Campaign reactor did not reserve site");
        require(!bot->builderPlaceLocations[yard->getObjectID()].empty(),"Campaign reactor lost placement plan");
        const auto site=bot->reservedStructures.at(yard->getObjectID()).location;
        // Keep only the reserved reactor footprint and occupy it with a tank.
        // This is temporary congestion, not a reason to refund construction.
        for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
            if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Sand);
        const auto size=getStructureSize(Structure_NuclearPlant);
        for(int dy=0;dy<size.y;++dy)for(int dx=0;dx<size.x;++dx)currentGameMap->getTile(site.x+dx,site.y+dy)->setType(Terrain_Rock);
        auto* blocker=house->createUnit(Unit_Tank);require(blocker,"Missing congestion fixture unit");blocker->deploy(site);
        yard->productionProgress=yard->getBuildItem(Structure_NuclearPlant)->price;yard->setWaitingToPlace();
        require(yard->isWaitingToPlace(),"Fixture reactor not finished");
        bot->build(0);
        require(yard->isWaitingToPlace(),"Moving unit caused needless reactor cancellation");
        for(int dy=0;dy<size.y;++dy)for(int dx=0;dx<size.x;++dx)currentGameMap->getTile(site.x+dx,site.y+dy)->setType(Terrain_Sand);
        const int creditsBefore=house->getCredits(), refund=yard->getProductionProgress().lround();
        bot->build(0);
        require(yard->getProductionQueueSize()==0,"Impossible reactor still blocks yard");
        require(house->getCredits()>=creditsBefore+refund,"Lost reactor production was not refunded");
        require(!bot->reservedStructures.count(yard->getObjectID()),"Cancelled reactor left reservation");
        bot->build(0);
        require(yard->getCurrentProducedItem()!=Structure_NuclearPlant,"Impossible reactor immediately requeued");
        // A 2x2 opening admits a windtrap but never the 3x3 reactor.
        bool opened=false;
        for(int y=0;y<currentGameMap->getSizeY()-2 && !opened;++y)for(int x=0;x<currentGameMap->getSizeX()-2 && !opened;++x) {
            if(!currentGameMap->isWithinBuildRange(x,y,house))continue;
            bool empty=true;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)empty &= !currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject();
            if(!empty)continue;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)currentGameMap->getTile(x+dx,y+dy)->setType(Terrain_Rock);
            bot->clearPlacementCache();opened=bot->findPlaceLocation(Structure_WindTrap).isValid();
            if(!opened)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)currentGameMap->getTile(x+dx,y+dy)->setType(Terrain_Sand);
        }
        require(opened,"No small windtrap fixture site");
        house->powerRequirement=house->getProducedPower()+50;bot->build(0);
        require(yard->getCurrentProducedItem()==Structure_WindTrap,"Yard did not recover with smaller windtrap");
    }catch(...){structureList=structures;throw;}
    structureList=structures;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=nuclear outcome=passed");
}
