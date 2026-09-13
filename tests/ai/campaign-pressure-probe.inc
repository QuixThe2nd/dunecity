// Real engine fixture; only compiled by the diagnostic driver.
#include <misc/IMemoryStream.h>
#include <misc/OMemoryStream.h>
#include <Menu/HouseChoiceMenu.h>

void runCampaignPressureProbe(Game& game) {
    auto require=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);};
    std::vector<QuantBot*> enemies;
    QuantBot* partner=nullptr;
    for (int h=0;h<NUM_HOUSES;++h) if (auto* house=game.getHouse(h))
        for (const auto& player:house->getPlayerList()) if(auto* bot=dynamic_cast<QuantBot*>(player.get())) {
            if (h==HOUSE_HARKONNEN) partner=bot;
            else if (house->getNumUnits()) enemies.push_back(bot);
        }
    require(enemies.size()>=2,"Pressure probe needs two campaign enemy houses");
    require(partner && !partner->isCampaignEnemy(),"Human partner inherited enemy restrictions");
    game.gameCycleCount=1000;
    game.techLevel=3; // No repair-yard prerequisite in the controlled wave fixture.
    auto reset=[&](QuantBot::Difficulty difficulty) {
        for (auto* bot:enemies) {
            bot->difficulty=difficulty;bot->gameMode=QuantBot::GameMode::Campaign;
            bot->militaryValueLimit=0;bot->attackTimer=0;
            bot->campaignWave={};bot->campaignWave.initialized=true;
            for (auto* unit:unitList) if (unit->getOwner()==bot->getHouse()) {
                unit->setHealth(unit->getMaxHealth()); bot->doSetAttackMode(unit,GUARD);
            }
        }
    };
    for (auto tier:{QuantBot::Difficulty::Easy,QuantBot::Difficulty::Medium,QuantBot::Difficulty::Hard,QuantBot::Difficulty::Brutal}) {
        reset(tier);
        enemies[0]->launchGroundHunt();
        require(!enemies[0]->campaignWave.members.empty(),"First enemy failed to launch fixture wave");
        const auto first=enemies[0]->campaignWave.members;
        enemies[0]->launchGroundHunt();
        require(enemies[0]->campaignWave.members==first,"Repeated check replenished active wave");
        enemies[1]->launchGroundHunt();
        require(enemies[1]->campaignWave.members.empty()==(tier<=QuantBot::Difficulty::Medium),"Incorrect simultaneous-house rule");
        const auto profile=enemies[0]->campaignProfile();const auto used=enemies[0]->campaignPressure();
        require(used.units<=profile.units && used.value<=profile.value,"Alliance exceeded count/value cap");
        require(used.houses<=profile.houses,"Alliance exceeded house cap");
        SDL_Log("PRESSURE_TIER_PASS: tier=%d houses=%d units=%d value=%d",int(tier),used.houses,used.units,used.value);
    }
    reset(QuantBot::Difficulty::Easy);enemies[0]->launchGroundHunt();
    auto* first=enemies[0];auto* second=enemies[1];
    // Serialize an actual QuantBot, not just the extracted policy object.
    OMemoryStream out;out.open();first->save(out);
    const auto previousVersion=game.loadedSavegameVersion;game.loadedSavegameVersion=SAVEGAMEVERSION;
    IMemoryStream input(out.getData(),out.getDataLength());
    {QuantBot restored(input,const_cast<House*>(first->getHouse()));
        require(restored.campaignWave.members==first->campaignWave.members,"Bot save lost assault membership");
        require(restored.campaignWave.launched==first->campaignWave.launched,"Bot save lost launch time");
    }
    game.loadedSavegameVersion=9837;
    IMemoryStream legacyInput(out.getData(),out.getDataLength());
    {QuantBot legacy(legacyInput,const_cast<House*>(first->getHouse()));
        require(!legacy.campaignWave.initialized && legacy.campaignWave.members.empty(),"Legacy save invented wave state");
    }
    game.loadedSavegameVersion=previousVersion;
    // Scripted HUNT arrivals cannot become an unbudgeted second assault.
    const UnitBase* held=nullptr;
    for (auto* unit:unitList) if(second->campaignCombatUnit(unit) && unit->isActive()) {held=unit;break;}
    require(held,"Missing reinforcement stand-in");second->doSetAttackMode(held,HUNT);second->updateCampaignWave();
    require(held->getAttackMode()!=HUNT,"Unbudgeted reinforcement kept HUNT");
    // End the first sortie through the real expiration path, then verify the
    // next house waits for the entire post-wave recovery interval.
    game.gameCycleCount+=MILLI2CYCLES(first->campaignProfile().sortieMs);
    first->updateCampaignWave();require(first->campaignWave.members.empty(),"Expired sortie retained slot");
    const auto recovery=MILLI2CYCLES(first->campaignProfile().recoveryMs);
    second->launchGroundHunt();require(second->campaignWave.members.empty(),"No recovery interval after wave");
    game.gameCycleCount+=recovery-1;second->launchGroundHunt();
    require(second->campaignWave.members.empty(),"Recovery ended early");
    ++game.gameCycleCount;second->launchGroundHunt();
    require(!second->campaignWave.members.empty(),"Next enemy did not get its turn after recovery");
    // Enemy helper-role changes are read from controllers, not legacy isAI flags.
    require(!partner->isCampaignEnemy(),"Shared human house was restricted");
    for (auto* bot : {first,second,partner}) {
        bot->difficulty=bot==second ? QuantBot::Difficulty::Medium : QuantBot::Difficulty::Easy;
        auto* house=const_cast<House*>(bot->getHouse());
        BuilderBase* yard=nullptr;
        for (auto* building : structureList)
            if (building->getOwner()==house && building->getItemID()==Structure_ConstructionYard)
                {yard=dynamic_cast<BuilderBase*>(building);break;}
        require(yard,"Power fixture needs construction yard");
        const int oldPower=house->powerRequirement;
        house->powerRequirement=house->getProducedPower()+300;
        house->addCredits(10000);bot->lastCalculatedSpice=100000;
        auto structures=structureList; structureList.clear();structureList.push_back(yard);
        try {bot->build(0);}catch(...){structureList=structures;throw;}
        structureList=structures;
        require(yard->getCurrentProducedItem()==Structure_WindTrap,"Easy/Medium did not queue power deficit Windtrap");
        house->powerRequirement=oldPower;
    }
    HouseChoiceMenu menu;
    menu.setCurrentPosition(0,0,640,480);
    const auto size=menu.getSize();
    sdl2::texture_ptr imageTarget(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,size.x,size.y));
    require(imageTarget && SDL_SetRenderTarget(renderer,imageTarget.get())==0,"Menu render target failed");
    SDL_RenderSetLogicalSize(renderer,size.x,size.y);
    for (int tier=0;tier<4;++tier) {
        menu.supportBotDropDown.setSelectedItem(5+tier);menu.onSupportBotSelectionChanged(false);
        menu.enemyAIDropDown.setSelectedItem(tier);menu.onEnemyAISelectionChanged(false);
        SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
        menu.draw();auto screenshot=renderReadSurface(renderer);
        const std::string filename="campaign-difficulty-menu-"+std::to_string(tier)+".png";
        require(SavePNG(screenshot.get(),filename.c_str())==0,"Menu screenshot failed");
    }
    SDL_SetRenderTarget(renderer,nullptr);
    SDL_Log("CAMPAIGN_PRESSURE_PROBE_PASS: difficulty slots, aggregate budgets, no top-ups, save/load, reinforcement holding, expiration and recovery");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=pressure outcome=passed");
}
