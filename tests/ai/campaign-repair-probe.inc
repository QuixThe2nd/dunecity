// Diagnostic-only real construction queue checks for both campaign roles.
void runCampaignRepairProbe(Game& game) {
    auto require=[](bool ok,const char* reason){if(!ok){SDL_Log("CAMPAIGN_REPAIR_FAIL: %s",reason);throw std::runtime_error(reason);}};
    QuantBot* partner=nullptr;QuantBot* enemy=nullptr;
    for(int h=0;h<NUM_HOUSES;++h) if(auto* house=game.getHouse(h))
        for(const auto& player:house->getPlayerList()) if(auto* bot=dynamic_cast<QuantBot*>(player.get())) {
            if(h==HOUSE_HARKONNEN) partner=bot;
            else if(!enemy && house->getNumItems(Structure_ConstructionYard)) enemy=bot;
        }
    require(partner && enemy,"Repair fixture requires both campaign roles");
    for(auto* bot:{partner,enemy}) {
        require(bot->difficulty>=QuantBot::Difficulty::Hard,"Run repair probe with Hard or Brutal controllers");
        auto* house=const_cast<House*>(bot->getHouse());
        auto place=[&](Uint32 item) {
            for(int y=1;y<currentGameMap->getSizeY()-4;y+=4)
                for(int x=1;x<currentGameMap->getSizeX()-4;x+=4) {
                    bool clear=true;
                    for(int dy=0;dy<3;++dy)for(int dx=0;dx<3;++dx)
                        if(currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject())clear=false;
                    if(clear && house->placeStructure(NONE_ID,item,x,y,true,true))return;
                }
            throw std::runtime_error("No fixture building site");
        };
        for(auto item:{Structure_WindTrap,Structure_Refinery,Structure_HeavyFactory,Structure_LightFactory})
            if(!house->getNumItems(item))place(item);
        if(!house->getNumItems(Unit_Harvester))house->createUnit(Unit_Harvester,true);
        auto originalStructures=structureList;
        for(auto* building:originalStructures)
            if(building->getOwner()==house && (building->getItemID()==Structure_RepairYard
                || building->getItemID()==Structure_Radar))building->destroy();
        BuilderBase* yard=nullptr;
        for(auto* building:structureList)
            if(building->getOwner()==house && building->getItemID()==Structure_ConstructionYard)
                {yard=dynamic_cast<BuilderBase*>(building);break;}
        require(yard,"Missing fixture construction yard");
        yard->updateBuildList();
        require(yard->isAvailableToBuild(Structure_Radar),"Radar prerequisites missing in fixture");
        house->addCredits(10000);bot->lastCalculatedSpice=100000;
        if(bot==partner) bot->gameMode=QuantBot::GameMode::Custom;
        bot->build(4000);
        require(yard->getCurrentProducedItem()==Structure_Radar,"Missing repair prerequisite was not prioritized");
        yard->doCancelItem(Structure_Radar);
        bot->builderPlaceLocations.erase(yard->getObjectID());
        bot->reservedStructures.erase(yard->getObjectID());
        place(Structure_Radar);yard->updateBuildList();
        require(yard->isAvailableToBuild(Structure_RepairYard),"Repair prerequisites still missing");
        bot->build(4000);
        require(yard->getCurrentProducedItem()==Structure_RepairYard,"Missing repair yard was not prioritized");
        bot->build(4000);
        int queued=0;
        for(auto* building:structureList) if(building->getOwner()==house)
            if(auto* builder=dynamic_cast<BuilderBase*>(building))
                for(const auto& item:builder->currentProductionQueue)
                    if(item.itemID==Structure_RepairYard) ++queued;
        require(queued==1,"Repair planning duplicated an already queued yard");
        SDL_Log("CAMPAIGN_REPAIR_PASS: tier=%d enemy=%d missing yard queued once",int(bot->difficulty),bot->isCampaignEnemy());
    }
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=repair outcome=passed");
}
