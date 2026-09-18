// Real queue/placement regressions for a lost or exposed campaign reactor site.
#include <players/TacticalSafetyPolicy.h>
void runCampaignNuclearProbe(Game& game) {
    const bool threatened = std::getenv("BALANCE_REACTOR_SAFETY_PROBE");
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
    // Even a one-power shortage must prefer an affordable, placeable reactor.
    house->addCredits(10000);house->powerRequirement=house->getProducedPower()+1;
    while(yard->getProductionQueueSize())yard->doCancelItem(yard->getCurrentProducedItem());
    yard->buildList.push_back(BuildItem(Structure_NuclearPlant,1000));
    yard->buildList.push_back(BuildItem(Structure_WindTrap,300));
    yard->upgrading=false;
    auto structures=structureList;structureList.clear();structureList.push_back(yard);
    try {
        for(int y=0;y<currentGameMap->getSizeY();++y)for(int x=0;x<currentGameMap->getSizeX();++x)
            if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Rock);
        bot->planningBuilder=yard->getObjectID();bot->clearPlacementCache();
        if (threatened) {
            bot->refreshTacticalDanger();
            bot->dangerUpdated=game.getGameCycleCount();
            bot->tacticalDanger.assign(currentGameMap->getSizeX()*currentGameMap->getSizeY(),100);
            bot->factoryEnemyClearance.assign(bot->tacticalDanger.size(),0);
            require(bot->findPlaceLocation(Structure_NuclearPlant).isInvalid(),"Reactor planning accepts enemy fire");
            bot->tacticalDanger.assign(bot->tacticalDanger.size(),0);
            bot->factoryEnemyClearance.assign(bot->tacticalDanger.size(),12);
            bot->lossDanger.assign(bot->tacticalDanger.size(),0);
            bot->clearPlacementCache();
            // Two legal 3x3 plots: put the forward plot just outside weapon
            // reach, with a second plot farther away. Exercise the real search,
            // not just the rank helper, including its ordinary packing bonuses.
            const auto front=bot->findPlaceLocation(Structure_NuclearPlant);
            require(front.isValid(),"Missing forward reactor fixture plot");
            Coord rear=Coord::Invalid();
            const auto extent=getStructureSize(Structure_NuclearPlant);
            const int width=currentGameMap->getSizeX(),height=currentGameMap->getSizeY();
            for(int y=0;y<=height-extent.y && rear.isInvalid();++y)
                for(int x=0;x<=width-extent.x && rear.isInvalid();++x) {
                    if(std::max(std::abs(x-front.x),std::abs(y-front.y))<12)continue;
                    if(currentGameMap->okayToPlaceStructure(x,y,extent.x,extent.y,false,house,false,Structure_NuclearPlant))rear=Coord(x,y);
                }
            require(rear.isValid(),"Missing rear reactor fixture plot");
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)
                if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Sand);
            for(Coord plot:{front,rear})for(int dy=0;dy<extent.y;++dy)for(int dx=0;dx<extent.x;++dx)
                currentGameMap->getTile(plot.x+dx,plot.y+dy)->setType(Terrain_Rock);
            for(Coord plot:{front,rear}) {
                bool access=false;
                for(Coord edge:{Coord(plot.x,plot.y-1),Coord(plot.x,plot.y+extent.y),
                    Coord(plot.x-1,plot.y),Coord(plot.x+extent.x,plot.y)}) {
                    if(!currentGameMap->tileExists(edge) || currentGameMap->getTile(edge)->hasAGroundObject())continue;
                    currentGameMap->getTile(edge)->setType(Terrain_Rock);access=true;break;
                }
                require(access,"Reactor fixture plot lacks road frontage");
            }
            const Coord threat(front.x>0 ? front.x-1 : front.x+extent.x,front.y);
            bot->tacticalDanger[threat.y*width+threat.x]=100;
            bot->factoryEnemyClearance=TacticalSafetyPolicy::enemyClearance(bot->tacticalDanger,width,height);
            bot->clearPlacementCache();
            const auto weaponChoice=bot->findPlaceLocation(Structure_NuclearPlant);
            SDL_Log("REACTOR_SITES: front=(%d,%d) rear=(%d,%d) chosen=(%d,%d) quality=%s",front.x,front.y,rear.x,rear.y,
                weaponChoice.x,weaponChoice.y,bot->placementScoreDetails[Structure_NuclearPlant].json().c_str());
            require(weaponChoice==rear,"Reactor ignored safer plot farther beyond enemy weapons");
            // When neither plot is near weapons, enemy-base direction still
            // decides before the usual adjacency and map-edge packing score.
            bot->tacticalDanger.assign(bot->tacticalDanger.size(),0);
            bot->factoryEnemyClearance.assign(bot->tacticalDanger.size(),12);
            bot->visibleEnemyBases={front};
            // Equal blast clearance isolates the rear-position preference.
            const auto savedList=structureList;
            structureList.clear();
            for(auto* building:structures)
                if(building->getOwner()==house && building->getItemID()==Structure_WindTrap)structureList.push_back(building);
            require(!structureList.empty(),"Missing noncritical base anchor");
            bot->clearPlacementCache();
            const auto rearChoice=bot->findPlaceLocation(Structure_NuclearPlant);
            structureList=savedList;
            require(rearChoice==rear,"Reactor ignored rear position away from enemy base");
            bot->visibleEnemyBases.clear();
            for(int y=0;y<height;++y)for(int x=0;x<width;++x)
                if(!currentGameMap->getTile(x,y)->hasAStructure())currentGameMap->getTile(x,y)->setType(Terrain_Rock);
            bot->clearPlacementCache();
        }
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
        if (threatened) {
            // The footprint remains legal, but enemy fire now covers it.
            // Ignore the friendly blocker when looking for potential sites.
            bot->tacticalDanger.assign(bot->tacticalDanger.size(),100);
            bot->factoryEnemyClearance.assign(bot->tacticalDanger.size(),0);
        } else {
            for(int dy=0;dy<size.y;++dy)for(int dx=0;dx<size.x;++dx)currentGameMap->getTile(site.x+dx,site.y+dy)->setType(Terrain_Sand);
        }
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
            if (threatened) for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)
                bot->tacticalDanger[(y+dy)*currentGameMap->getSizeX()+x+dx]=0;
            for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)currentGameMap->getTile(x+dx,y+dy)->setType(Terrain_Rock);
            bot->clearPlacementCache();opened=bot->findPlaceLocation(Structure_WindTrap).isValid();
            if(!opened)for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx)currentGameMap->getTile(x+dx,y+dy)->setType(Terrain_Sand);
        }
        require(opened,"No small windtrap fixture site");
        // The no-site planning pass may have started an unrelated yard upgrade.
        yard->upgrading=false;
        house->powerRequirement=house->getProducedPower()+50;bot->build(0);
        require(yard->getCurrentProducedItem()==Structure_WindTrap,"Yard did not recover with smaller windtrap");
        if (threatened) {
            const auto recovery=bot->reservedStructures.at(yard->getObjectID()).location;
            require(bot->dangerAt(recovery,getStructureSize(Structure_WindTrap))==0,"Recovery windtrap abandoned safe opening");
        }
    }catch(...){structureList=structures;throw;}
    structureList=structures;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=%s outcome=passed",threatened ? "reactor-safety" : "nuclear");
}
