// Real command-path regression for artillery losing Hunt during short dodges.
void runArmyMovementProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    auto* owner=game.getHouse(HOUSE_HARKONNEN);
    QuantBot* bot=nullptr;
    for(const auto& player:owner->getPlayerList())
        if(auto* candidate=dynamic_cast<QuantBot*>(player.get()))bot=candidate;
    require(bot,"Missing friendly bot");
    bot->difficulty=QuantBot::Difficulty::Brutal;bot->gameMode=QuantBot::GameMode::Custom;
    Coord origin=Coord::Invalid();
    for(int y=2;y+10<currentGameMap->getSizeY() && origin.isInvalid();++y)
        for(int x=2;x+16<currentGameMap->getSizeX() && origin.isInvalid();++x) {
            bool clear=true;
            for(int dy=0;dy<10;++dy)for(int dx=0;dx<16;++dx) {
                const auto* tile=currentGameMap->getTile(x+dx,y+dy);
                if(tile->hasAGroundObject() || tile->isMountain())clear=false;
            }
            if(clear)origin=Coord(x,y);
        }
    require(origin.isValid(),"Missing artillery fixture space");
    auto spawn=[&](House* house,Uint32 item,Coord offset) {
        auto* unit=house->createUnit(item);require(unit,"Unit fixture creation failed");
        unit->deploy(origin+offset);unit->setVisible(owner->getTeamID(),true);return unit;
    };
    auto* artillery=spawn(owner,Unit_Launcher,Coord(3,5));
    auto* friend1=spawn(owner,Unit_Tank,Coord(4,6));
    auto* friend2=spawn(owner,Unit_Tank,Coord(5,6));
    auto* homeGuard=spawn(owner,Unit_Tank,Coord(0,0));
    auto* worker=spawn(owner,Unit_Harvester,Coord(0,1));
    auto* threat=spawn(game.getHouse(HOUSE_ATREIDES),Unit_Tank,Coord(9,5));
    auto* enemyBuilding=game.getHouse(HOUSE_ATREIDES)->placeStructure(NONE_ID,Structure_Silo,
        origin.x+12,origin.y+1,true,true);
    require(enemyBuilding,"Building fixture failed");
    auto saved=unitList;unitList.clear();
    for(auto* unit:{artillery,friend1,friend2,homeGuard,worker,threat})unitList.push_back(unit);
    try {
        for(auto* unit:{artillery,friend1,friend2})bot->doSetAttackMode(unit,HUNT);
        bot->doSetAttackMode(homeGuard,AREAGUARD);
        require(bot->findSquadCenter(HOUSE_HARKONNEN)==origin+Coord(4,5),
            "Home guard or worker dragged assault centre backwards");
        bot->doAttackObject(artillery,threat,false);
        const Coord start=artillery->getLocation();
        bot->checkAllUnits();
        require(artillery->getAttackMode()==HUNT,"Artillery dodge replaced Hunt with guard/regroup");
        require(artillery->getDestination()!=start && blockDistance(start,artillery->getDestination())<=3,
            "Dodge was absent or exceeded roughly two tiles");
        require(blockDistance(artillery->getDestination(),threat->getLocation())>blockDistance(start,threat->getLocation()),
            "Dodge moved toward approaching enemy");
        // Arrival after a dodge must retain the assault, not acquire a home order.
        artillery->setLocation(artillery->getDestination());artillery->setForced(false);
        bot->checkAllUnits();
        require(artillery->getAttackMode()==HUNT,"Artillery returned to guarding after dodge arrival");
        // Nearby buildings are siege targets, not approaching ground threats.
        artillery->setLocation(origin+Coord(10,3));
        bot->doMove2Pos(artillery,artillery->getX(),artillery->getY(),false);
        bot->doSetAttackMode(artillery,HUNT);bot->doAttackObject(artillery,enemyBuilding,false);
        const Coord destination=artillery->getDestination();
        bot->checkAllUnits();
        require(artillery->getAttackMode()==HUNT && artillery->getTarget()==enemyBuilding
            && artillery->getDestination()==destination,"Launcher backed away from a building instead of sieging it");
        // Regrouping never replaces a human's order.
        bot->onHumanUnitOrder(artillery->getObjectID());
        bot->doAttackObject(artillery,threat,false);
        const Coord manualDestination=artillery->getDestination();
        bot->checkAllUnits();require(artillery->getDestination()==manualDestination,"Kiting replaced a human order");
    } catch(...) {unitList=saved;throw;}
    unitList=saved;
    SDL_Log("ARMY_MOVEMENT_PASS: Hunt survives short dodge and arrival; buildings remain siege targets; assault centre excludes home guard/worker; human order preserved");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=army_movement outcome=passed");
}
