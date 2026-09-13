// Real units and damage/AI command paths; diagnostic executable only.
void runCampaignDefenceProbe(Game& game) {
    auto require=[](bool ok,const char* reason){if(!ok){SDL_Log("CAMPAIGN_DEFENCE_FAIL: %s",reason);throw std::runtime_error(reason);}};
    auto* house=game.getHouse(HOUSE_ATREIDES);
    QuantBot* bot=nullptr;
    for (const auto& player:house->getPlayerList())
        if (auto* candidate=dynamic_cast<QuantBot*>(player.get())) bot=candidate;
    require(bot && bot->isCampaignEnemy(),"Missing campaign enemy defender");
    Coord origin=Coord::Invalid();
    for (int y=1;y+8<currentGameMap->getSizeY() && !origin.isValid();++y)
        for (int x=1;x+14<currentGameMap->getSizeX() && !origin.isValid();++x) {
            bool clear=true;
            for (int dy=0;dy<8;++dy) for(int dx=0;dx<14;++dx) {
                const auto* tile=currentGameMap->getTile(x+dx,y+dy);
                if(tile->hasAGroundObject() || tile->isMountain()) clear=false;
            }
            if(clear) origin=Coord(x,y);
        }
    require(origin.isValid(),"No open defense fixture rectangle");
    auto spawn=[&](House* owner,Uint32 item,int x,int y) {
        auto* unit=owner->createUnit(item);
        require(unit,"Defense fixture unit creation failed");
        unit->deploy(origin+Coord(x,y));unit->setVisible(house->getTeamID(),true);
        return unit;
    };
    auto* tank=spawn(house,Unit_Tank,2,3);
    auto* backup=spawn(house,Unit_Tank,1,5);
    auto* launcher=spawn(game.getHouse(HOUSE_HARKONNEN),Unit_Launcher,10,3);
    auto* worker=spawn(house,Unit_Harvester,2,5);
    auto* building=house->placeStructure(NONE_ID,Structure_WindTrap,origin.x,origin.y,true,true);
    require(building,"Defense fixture building creation failed");
    auto savedUnits=unitList;auto savedStructures=structureList;
    unitList.clear();unitList.push_back(tank);unitList.push_back(backup);unitList.push_back(launcher);
    structureList.clear();
    try {
        for (auto tier:{QuantBot::Difficulty::Easy,QuantBot::Difficulty::Medium,
                        QuantBot::Difficulty::Hard,QuantBot::Difficulty::Brutal}) {
            bot->difficulty=tier;bot->campaignWave={};bot->campaignWave.initialized=true;
            bot->campaignWave.opening=game.getGameCycleCount()+MILLI2CYCLES(600000);
            bot->defenceAssignments.clear();bot->defenceResponseCycles.clear();
            bot->doMove2Pos(tank,tank->getX(),tank->getY(),false);
            bot->doSetAttackMode(tank,GUARD);tank->setHealth(tank->getMaxHealth());
            require(blockDistance(tank->getLocation(),launcher->getLocation())>tank->getWeaponRange(),"Fixture launcher must outrange idle tank");
            require(!tank->isInWeaponRange(launcher),"Weapon-range query ignored its argument without a current target");
            tank->handleDamage(1,launcher->getObjectID(),launcher->getOwner());
            require(tank->getTarget()==launcher,"Outranged campaign tank did not retaliate when hit");
            require(tank->getAttackMode()==AREAGUARD,"Retaliating tank must use Area Guard");
            bot->updateCampaignWave();bot->checkAllUnits();
            require(tank->getTarget()==launcher,"Wave enforcement cancelled defensive retaliation");
            require(bot->campaignWave.members.empty(),"Self-defense incorrectly launched an assault wave");
            const auto contact=launcher->getLocation();
            Coord withdrawn=Coord::Invalid();
            for (int y=1;y<currentGameMap->getSizeY()-1 && !withdrawn.isValid();++y)
                for (int x=1;x<currentGameMap->getSizeX()-1 && !withdrawn.isValid();++x)
                    if (blockDistance(Coord(x,y),tank->getGuardPoint())>20
                        && !currentGameMap->getTile(x,y)->hasAGroundObject()
                        && !currentGameMap->getTile(x,y)->isMountain()) withdrawn=Coord(x,y);
            require(withdrawn.isValid(),"No fixture location outside defensive contact");
            launcher->setLocation(withdrawn);bot->updateCampaignWave();bot->checkAllUnits();
            require(tank->getTarget()!=launcher,"Defender pursued attacker outside its response area");
            launcher->setLocation(contact);

            // Artillery outside the previous seven-tile perimeter must bring
            // reinforcements on the first building hit, even before attack grace.
            for(auto* unit:{tank,backup}) {bot->doMove2Pos(unit,unit->getX(),unit->getY(),false);bot->doSetAttackMode(unit,GUARD);}
            bot->defenceAssignments.clear();bot->defenceResponseCycles.clear();
            structureList.push_back(building);bot->campaignAIAttackFlag=false;
            // Exercise the ordinary-human first-hit activation branch as well
            // as the AI-assisted human-house case used by the other tiers.
            const bool attackerWasAI=launcher->getOwner()->ai;
            if (tier==QuantBot::Difficulty::Easy) launcher->getOwner()->ai=false;
            building->handleDamage(1,launcher->getObjectID(),launcher->getOwner());
            launcher->getOwner()->ai=attackerWasAI;
            require(tank->getTarget()==launcher || backup->getTarget()==launcher,"Base hit did not scramble reinforcements");
            bot->updateCampaignWave();bot->checkAllUnits();
            require(tank->getTarget()==launcher || backup->getTarget()==launcher,"Base defenders lost their response order");
            structureList.clear();

            // A remote spice field remains protected without any nearby base.
            for(auto* unit:{tank,backup}) bot->doMove2Pos(unit,unit->getX(),unit->getY(),false);
            bot->defenceAssignments.clear();bot->defenceResponseCycles.clear();
            unitList.push_back(worker);worker->setHealth(worker->getMaxHealth());
            worker->handleDamage(1,launcher->getObjectID(),launcher->getOwner());
            require(tank->getTarget()==launcher || backup->getTarget()==launcher,"Remote harvester received no reinforcements");
            bot->updateCampaignWave();bot->checkAllUnits();
            require(tank->getTarget()==launcher || backup->getTarget()==launcher,"Harvester rescue was cancelled by wave limits");
            unitList.remove(worker);

            bot->defenceAssignments.clear();
            for(auto* unit:{tank,backup}) bot->doMove2Pos(unit,unit->getX(),unit->getY(),false);
            bot->holdCampaignUnit(tank);
            require(tank->getAttackMode()==AREAGUARD,"Waiting defenders must use Area Guard");
            bot->doSetAttackMode(tank,RETREAT);bot->holdCampaignUnit(tank);
            require(tank->getAttackMode()==RETREAT,"Wave holding cancelled a repair retreat");
            SDL_Log("CAMPAIGN_DEFENCE_TIER_PASS: tier=%d retaliation, base and remote-worker response, Area Guard, retreat",int(tier));
        }
    } catch (...) {unitList=savedUnits;structureList=savedStructures;throw;}
    unitList=savedUnits;structureList=savedStructures;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=defence outcome=passed");
}
