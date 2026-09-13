// Diagnostic-only real construction queue checks for both campaign roles.
void runCampaignRepairProbe(Game& game) {
    auto require=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
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
        auto originalStructures=structureList;
        for(auto* building:originalStructures)
            if(building->getOwner()==house && building->getItemID()==Structure_RepairYard)
                building->destroy();
        BuilderBase* yard=nullptr;
        for(auto* building:structureList)
            if(building->getOwner()==house && building->getItemID()==Structure_ConstructionYard)
                {yard=dynamic_cast<BuilderBase*>(building);break;}
        require(yard,"Missing fixture construction yard");
        yard->updateBuildList();
        require(yard->isAvailableToBuild(Structure_RepairYard),"Repair prerequisites missing in fixture");
        house->addCredits(10000);bot->lastCalculatedSpice=100000;
        if(bot==partner) bot->gameMode=QuantBot::GameMode::Custom;
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
