// Reproduce the level-four 100-harvester override and oversized readiness goal.
void runCampaignPacingProbe(Game& game) {
    auto require=[](bool ok,const char* reason){if(!ok){SDL_Log("CAMPAIGN_PACING_FAIL: %s",reason);throw std::runtime_error(reason);}};
    std::vector<QuantBot*> enemies;QuantBot* partner=nullptr;
    game.gameCycleCount=123;
    for(int h=0;h<NUM_HOUSES;++h) if(auto* house=game.getHouse(h))
        for(const auto& player:house->getPlayerList()) if(auto* bot=dynamic_cast<QuantBot*>(player.get())) {
            bot->update();
            if(bot->isCampaignEnemy()) enemies.push_back(bot);else if(h==HOUSE_HARKONNEN) partner=bot;
        }
    require(partner && !enemies.empty(),"Pacing fixture needs partner and enemy");
    auto* bot=enemies.front();
    for(auto tier:{QuantBot::Difficulty::Easy,QuantBot::Difficulty::Medium,QuantBot::Difficulty::Hard}) {
        bot->difficulty=tier;bot->initialItemCount[Structure_Refinery]=2;
        bot->initialMilitaryValue=1000;bot->lastCalculatedSpice=200000;bot->harvesterLimit=100;
        game.gameInitSettings.gameOptions.maximumNumberOfHarvestersOverride=100;
        bot->update();
        const int expected=2*getQuantBotConfig().getSettings(int(tier)).harvesterLimitPerRefineryMultiplier;
        require(bot->harvesterLimit==expected,"High game ceiling replaced campaign difficulty economy cap");
        bot->update();require(bot->harvesterLimit==expected,"Campaign worker cap drifted on next update");
        game.gameInitSettings.gameOptions.maximumNumberOfHarvestersOverride=1;
        bot->update();require(bot->harvesterLimit==1,"Lower player ceiling was ignored");
        SDL_Log("CAMPAIGN_WORKER_LIMIT_PASS: tier=%d initial_refineries=2 limit=%d",int(tier),expected);
    }
    game.gameInitSettings.gameOptions.maximumNumberOfHarvestersOverride=100;
    partner->lastCalculatedSpice=200000;partner->update();
    require(partner->harvesterLimit>2,"Human partner incorrectly inherited small enemy worker cap");
    for(auto* enemy:enemies) {
        enemy->difficulty=QuantBot::Difficulty::Easy;enemy->campaignWave={};
        enemy->campaignWave.initialized=true;enemy->attackTimer=INT32_MAX;
    }
    // Ensure a usable army exists; these real troops still pass through the
    // normal percentage selection and shared-wave budget code.
    auto savedUnits=unitList;std::vector<UnitBase*> army;
    auto* owner=const_cast<House*>(bot->getHouse());int spawned=0;
    for(int y=1;y<currentGameMap->getSizeY()-1 && spawned<8;++y)
        for(int x=1;x<currentGameMap->getSizeX()-1 && spawned<8;++x)
            if(!currentGameMap->getTile(x,y)->hasAGroundObject() && !currentGameMap->getTile(x,y)->isMountain()) {
                auto* unit=owner->createUnit(Unit_Tank);require(unit,"Pacing tank creation failed");
                unit->deploy(Coord(x,y));bot->doSetAttackMode(unit,AREAGUARD);army.push_back(unit);++spawned;
            }
    savedUnits=unitList;unitList.clear();for(auto* unit:army)unitList.push_back(unit);
    try {
    bot->militaryValueLimit=9200;bot->attackTimer=0;
    game.techLevel=4;bot->campaignWave.opening=game.getGameCycleCount()+100;
    bot->attack(2400);require(bot->campaignWave.members.empty(),"Small-wave readiness bypassed opening grace");
    game.gameCycleCount+=100;
    bot->attack(2399);require(bot->campaignWave.members.empty(),"Easy launched without its defensive reserve");
    bot->attack(2400);
    require(bot->campaignWave.members.size()==4,"Eight tanks did not send four while keeping four defenders");
    const auto first=bot->campaignWave.members;bot->attack(2400);
    require(bot->campaignWave.members==first,"Readiness change topped up an active wave");
    const auto used=bot->campaignPressure();const auto limit=bot->campaignProfile();
    require(used.houses<=limit.houses && used.units<=limit.units && used.value<=limit.value,"Small wave exceeded assault limits");
    // Late-game threshold regressions: the same readiness calculation serves
    // enemy fairness and dispatch; a full partner also escapes a depleted map.
    game.gameCycleCount=MILLI2CYCLES(16*60000);game.techLevel=9;
    bot->lastCalculatedSpice=0;partner->lastCalculatedSpice=0;
    // Keep the house's remaining credits: idle money was another deadlock.
    require(bot->campaignRequiredArmy(16000)==0,"Exhausted enemy still demands an unaffordable army");
    require(partner->campaignRequiredArmy(16000)==0,"Exhausted partner still waits for 16000 army value");
    partner->lastCalculatedSpice=1000;
    require(partner->campaignRequiredArmy(8000)==8000,"Live economy lost its normal readiness goal");
    bot->campaignWave={};bot->campaignWave.initialized=true;
    require(!owner->hasRepairYard(),"Fixture must lack a repair yard");
    bot->attack(2400);
    require(!bot->campaignWave.members.empty(),"Late-game survivors did not attack without a repair yard");
    } catch (...) {unitList=savedUnits;throw;}
    unitList=savedUnits;
    SDL_Log("CAMPAIGN_PACING_PASS: eight 300-value tanks send four and retain four at 2400 readiness; opening and shared limits preserved");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=pacing outcome=passed");
}
