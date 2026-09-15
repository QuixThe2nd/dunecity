// Exercise the real house power predicate and radar transitions in each mod.
#include <RadarView.h>
void runRadarPowerProbe(Game& game) {
    auto require=[](bool ok,const char* why){if(!ok){SDL_Log("RADAR_FAIL: %s",why);throw std::runtime_error(why);}};
    auto* house=pLocalHouse;
    const int radarCount=house->numItem[Structure_Radar];
    const int produced=house->producedPower, required=house->powerRequirement;
    house->numItem[Structure_Radar]=1;
    house->producedPower=100;house->powerRequirement=101;
    require(!house->hasRadarOn(),"Radar active below required power");
    if(!game.isCitySimEnabled()) require(house->hasPower(),"Vanilla general power behavior changed");
    RadarView view;view.setRadarMode(false);view.update();
    require(view.currentRadarMode==RadarView::RadarMode::RadarOff,"Underpowered radar started activation");
    house->powerRequirement=100;view.update();
    require(house->hasRadarOn(),"Exact required power should activate radar");
    require(view.currentRadarMode==RadarView::RadarMode::AnimationRadarOn,"Power restoration did not activate radar");
    house->powerRequirement=101;view.update();
    require(view.currentRadarMode!=RadarView::RadarMode::AnimationRadarOn && view.currentRadarMode!=RadarView::RadarMode::RadarOn,
        "Power loss failed to interrupt activation");
    view.setRadarMode(true);view.update();
    require(view.currentRadarMode==RadarView::RadarMode::AnimationRadarOff,"Active radar did not shut down");
    house->powerRequirement=99;view.update();
    require(view.currentRadarMode==RadarView::RadarMode::AnimationRadarOn || view.currentRadarMode==RadarView::RadarMode::RadarOn,
        "Restoration during shutdown did not reactivate radar");
    house->numItem[Structure_Radar]=0;
    require(!house->hasRadarOn(),"Power alone enabled radar without an outpost");
    house->numItem[Structure_Radar]=radarCount;house->producedPower=produced;house->powerRequirement=required;
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=radar outcome=passed");
}
