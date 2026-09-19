// Exercise real harvester orders, remembered jobs and refinery redeployment.
void runHarvesterSafetyProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("HARVESTER_SAFETY_FAIL: %s",why);throw std::runtime_error(why);}};
    auto* owner=pLocalHouse;
    QuantBot* bot=nullptr;
    for(const auto& player:owner->getPlayerList())
        if(auto* candidate=dynamic_cast<QuantBot*>(player.get()))bot=candidate;
    require(bot,"Missing partner bot");
    House* enemy=nullptr;
    for(int id=0;id<NUM_HOUSES;++id)if(auto* house=game.getHouse(id))
        if(house->getTeamID()!=owner->getTeamID()) {enemy=house;break;}
    require(enemy,"Missing enemy house");
    Coord origin=Coord::Invalid();
    for(int y=1;y+8<currentGameMap->getSizeY() && origin.isInvalid();++y)
        for(int x=1;x+18<currentGameMap->getSizeX() && origin.isInvalid();++x) {
            bool clear=true;
            for(int dy=0;dy<8;++dy)for(int dx=0;dx<18;++dx)
                if(currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject())clear=false;
            if(clear)origin=Coord(x,y);
        }
    require(origin.isValid(),"Missing harvester fixture space");
    for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
        if(!currentGameMap->getTile(x,y)->hasAGroundObject())currentGameMap->getTile(x,y)->setType(Terrain_Sand);
    const Coord oldField=origin+Coord(10,3),safeField=origin+Coord(1,3);
    currentGameMap->getTile(oldField)->setSpice(1000);
    currentGameMap->getTile(safeField)->setSpice(1000);
    auto* worker=static_cast<Harvester*>(owner->createUnit(Unit_Harvester));
    auto* threat=enemy->createUnit(Unit_Tank);
    require(worker && threat,"Failed fixture unit creation");
    worker->deploy(oldField);threat->deploy(origin+Coord(14,3));
    threat->setVisible(owner->getTeamID(),true);
    auto* refinery=owner->placeStructure(NONE_ID,Structure_Refinery,origin.x,origin.y+5,true,true);
    require(refinery,"Missing fixture refinery");
    auto savedUnits=unitList;auto savedStructures=structureList;
    unitList.clear();unitList.push_back(worker);unitList.push_back(threat);
    structureList.clear();structureList.push_back(refinery);
    auto check=[&]() {
        bot->dangerUpdated=std::numeric_limits<Uint32>::max();bot->refreshTacticalDanger();
        bot->harvesterSafety[worker->getObjectID()].nextCheck=0;
        bot->manageHarvesterSafety(worker);
    };
    auto oldJob=[&](FixPoint cargo) {
        worker->setTarget(nullptr);worker->setAmountOfSpice(cargo);
        worker->doSetAttackMode(HARVEST);worker->doMove2Pos(oldField,false);
    };
    try {
        for(FixPoint cargo:{0_fix,100_fix}) {
            oldJob(cargo);check();
            require(!worker->isReturning(),"Evacuation unnecessarily returned to refinery");
            require(worker->getDestination()==safeField && worker->getGuardPoint()==safeField,
                "Evacuation did not replace the old field with safe spice");
            require(worker->getAttackMode()==HARVEST,"Safe alternate job is not harvesting");
        }
        oldJob(HARVESTERMAXSPICE);check();
        require(worker->getTarget()==refinery && worker->isReturning(),"Full load did not unload safely");
        require(worker->getGuardPoint()==safeField,"Unloading retained dangerous old job");
        check();require(worker->getTarget()==refinery,"Safety cancelled safe unloading");
        // The real deploy hook uses guardPoint after unloading or a carryall trip.
        worker->setTarget(nullptr);worker->setAmountOfSpice(0);
        currentGameMap->removeObjectFromMap(worker->getObjectID());worker->deploy(origin+Coord(3,4));
        require(worker->getDestination()==safeField,"Refinery redeployment returned to unsafe field");
        // Losing visibility is not evidence that the old field has become safe.
        threat->setVisible(owner->getTeamID(),false);oldJob(0);check();
        require(worker->getDestination()==safeField,"Forgot evacuated field when enemy became hidden");
        currentGameMap->getTile(safeField)->setType(Terrain_Sand);oldJob(0);check();
        require(worker->getAttackMode()==STOP && !worker->isReturning(),"No alternate field caused base/field loop");
        oldJob(100);check();
        require(worker->getTarget()==refinery && worker->getAttackMode()==STOP,
            "No safe field failed to unload cargo and disable the old job");
        check();require(worker->getTarget()==refinery,"Stopped post-unload job interrupted cargo return");
        worker->setTarget(nullptr);worker->setAmountOfSpice(0);
        currentGameMap->removeObjectFromMap(worker->getObjectID());worker->deploy(origin+Coord(3,4));
        check();require(worker->getAttackMode()==STOP && !worker->isReturning(),
            "Unloading without a safe alternative restarted dangerous harvesting");
        // Once danger has actually cooled down, harvesting can resume.
        game.gameCycleCount+=MILLI2CYCLES(120001);check();
        require(worker->getAttackMode()==HARVEST && worker->getDestination()==oldField,
            "Expired danger memory permanently stranded harvester");
        SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=harvester_safety outcome=passed");
    } catch(...) {unitList=savedUnits;structureList=savedStructures;throw;}
    unitList=savedUnits;structureList=savedStructures;
}
