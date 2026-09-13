#include <players/QuantBot.h>
#include <players/HumanPlayer.h>
#include <House.h>
#include <Game.h>
#include <Map.h>
#include <sand.h>
#include <structures/StructureBase.h>
#include <units/UnitBase.h>
#include <Trigger/ReinforcementTrigger.h>
#include <Trigger/TriggerManager.h>

bool QuantBot::isCampaignEnemy() const {
    if (!currentGame || !isCampaignGameType(currentGame->gameType) || supportMode
        || difficulty == Difficulty::Defend) return false;
    for (const auto& player : getHouse()->getPlayerList())
        if (dynamic_cast<const HumanPlayer*>(player.get())) return false;
    // AI-only allies of a human must not inherit beginner-facing enemy caps.
    for (int h=0;h<NUM_HOUSES;++h) if (const auto* house=getHouse(h)) {
        if (house->getTeamID()!=getHouse()->getTeamID()) continue;
        for (const auto& player : house->getPlayerList())
            if (dynamic_cast<const HumanPlayer*>(player.get())) return false;
    }
    return true;
}

std::vector<const QuantBot*> QuantBot::campaignAlliance() const {
    std::vector<const QuantBot*> result;
    for (int h=0;h<NUM_HOUSES;++h) if (const auto* house=getHouse(h)) {
        if (house->getTeamID()!=getHouse()->getTeamID()) continue;
        for (const auto& player : house->getPlayerList())
            if (const auto* bot=dynamic_cast<const QuantBot*>(player.get()); bot && bot->isCampaignEnemy()) {
                result.push_back(bot); break; // One offensive controller per house.
            }
    }
    return result;
}

CampaignDifficultyPolicy::Profile QuantBot::campaignProfile() const {
    int tier=static_cast<int>(difficulty);
    for (const auto* bot : campaignAlliance())
        if (bot->getHouse()->getNumUnits() || bot->getHouse()->getNumStructures())
            tier=std::min(tier,static_cast<int>(bot->difficulty));
    return CampaignDifficultyPolicy::profile(tier,currentGame->techLevel);
}

bool QuantBot::campaignCombatUnit(const UnitBase* unit) const {
    return unit && unit->getOwner()==getHouse() && unit->getHealth()>0
        && (unit->canAttack() || unit->getItemID()==Unit_Saboteur)
        && unit->getItemID()!=Unit_Harvester && unit->getItemID()!=Unit_Sandworm
        && unit->getItemID()!=Unit_MCV && unit->getItemID()!=Unit_Carryall;
}

CampaignDifficultyPolicy::Pressure QuantBot::campaignPressure() const {
    CampaignDifficultyPolicy::Pressure result;
    for (const auto* bot : campaignAlliance()) {
        result.lastActive=std::max(result.lastActive,bot->campaignWave.lastActive);
        bool active=false;
        for (auto id : bot->campaignWave.members) {
            const auto* unit=dynamic_cast<const UnitBase*>(getObject(id));
            // Include transported survivors so pickup cannot free an assault slot.
            if (!bot->campaignCombatUnit(unit)) continue;
            active=true; ++result.units;
            result.value+=std::max(100,currentGame->objectData.data[unit->getItemID()][unit->getOriginalHouseID()].price);
        }
        if (active) ++result.houses;
    }
    return result;
}

bool QuantBot::campaignCanLaunch() const {
    if (!isCampaignEnemy()) return true;
    if (!campaignWave.initialized || !campaignWave.members.empty()) return false;
    const auto profile=campaignProfile();
    const auto pressure=campaignPressure();
    if (!CampaignDifficultyPolicy::canLaunch(profile,pressure,getGameCycleCount(),
            campaignWave.opening,MILLI2CYCLES(profile.recoveryMs))) return false;
    // Deterministic fairness among houses that are ready, rather than always
    // letting the first updated house take the next Easy/Medium turn.
    for (const auto* bot : campaignAlliance()) {
        if (bot==this || !bot->campaignWave.initialized || !bot->campaignWave.members.empty()
            || getGameCycleCount()<bot->campaignWave.opening || bot->attackTimer>0
            || !getQuantBotConfig().getSettings(static_cast<int>(bot->difficulty)).attackEnabled
            || (currentGame->techLevel>4 && !bot->getHouse()->hasRepairYard())) continue;
        int value=0; bool usable=false;
        for (const auto* unit : getUnitList()) if (bot->campaignCombatUnit(unit)) {
            value+=currentGame->objectData.data[unit->getItemID()][unit->getOriginalHouseID()].price;
            usable |= unit->isActive() && unit->isRespondable() && !unit->isBadlyDamaged()
                && unit->getAttackMode()!=RETREAT && !unit->hasATarget();
        }
        const auto& settings=getQuantBotConfig().getSettings(static_cast<int>(bot->difficulty));
        if (!usable || value < static_cast<int>(bot->militaryValueLimit*settings.attackThresholdPercent)) continue;
        if (std::make_pair(bot->campaignWave.launched,bot->getHouse()->getHouseID())
            < std::make_pair(campaignWave.launched,getHouse()->getHouseID())) return false;
    }
    return true;
}

bool QuantBot::campaignLocalContact(const ObjectBase* target) const {
    if (!target || target->getHealth()<=0 || !target->isActive()) return false;
    const int radius=difficulty<=Difficulty::Medium ? 7 : 10;
    for (const auto* building : getStructureList())
        if (building->getOwner()==getHouse() && building->getHealth()>0
            && blockDistance(target->getLocation(),building->getClosestPoint(target->getLocation()))<=radius) return true;
    // Harvesters get close protection, not a whole army pursuing across the map.
    for (const auto* worker : getUnitList())
        if (worker->getOwner()==getHouse() && worker->getItemID()==Unit_Harvester && worker->isActive()
            && blockDistance(target->getLocation(),worker->getLocation())<=4) {
            for (const auto* building : getStructureList())
                if (building->getOwner()==getHouse() && building->getHealth()>0
                    && blockDistance(worker->getLocation(),building->getClosestPoint(worker->getLocation()))
                        <= (difficulty<=Difficulty::Medium ? 12 : 20)) return true;
        }
    return false;
}

void QuantBot::holdCampaignUnit(const UnitBase* unit) {
    if (!unit->isActive() || !unit->isRespondable()) return;
    const StructureBase* home=nullptr; int distance=INT32_MAX;
    for (const auto* building : getStructureList()) {
        if (building->getOwner()!=getHouse() || building->getHealth()<=0) continue;
        int d=blockDistance(unit->getLocation(),building->getLocation()).lround();
        if (d<distance) {home=building;distance=d;}
    }
    const Coord point=home ? home->getClosestPoint(unit->getLocation()) : unit->getLocation();
    defenceAssignments.erase(unit->getObjectID());
    if (unit->hasATarget() || unit->getAttackMode()!=GUARD) doSetAttackMode(unit,GUARD);
    const_cast<UnitBase*>(unit)->setGuardPoint(point);
    if (home && distance>6 && (!unit->isMoving() || !unit->wasForced()))
        doMove2Pos(unit,point.x,point.y,true);
}

void QuantBot::updateCampaignWave() {
    if (!isCampaignEnemy()) return;
    const auto now=getGameCycleCount();
    const auto profile=campaignProfile();
    if (!campaignWave.initialized) {
        // Use the scenario's first combat reinforcement as an opening anchor,
        // bounded by the existing tech pacing. Reinforcement arrival is unchanged.
        Uint32 anchor=MILLI2CYCLES(currentGame->techLevel==8 ? 720000
            : currentGame->techLevel==7 ? 600000 : currentGame->techLevel==6 ? 540000 : 480000);
        for (const auto& trigger : currentGame->getTriggerManager().getTriggers()) {
            const auto* reinforcement=dynamic_cast<const ReinforcementTrigger*>(trigger.get());
            if (!reinforcement || reinforcement->getHouseID()!=getHouse()->getHouseID()) continue;
            bool combat=false;
            for (auto item : reinforcement->getDroppedUnits())
                combat |= item!=Unit_Harvester && item!=Unit_Carryall && item!=Unit_MCV;
            if (!combat) continue;
            anchor=std::max(anchor,std::min<Uint32>(MILLI2CYCLES(720000),reinforcement->getCycleNumber()));
            break;
        }
        campaignWave.opening=anchor+MILLI2CYCLES(profile.graceMs);
        campaignWave.initialized=true;
    }
    const bool hadWave=!campaignWave.members.empty();
    for (auto it=campaignWave.members.begin();it!=campaignWave.members.end();) {
        const auto* unit=dynamic_cast<const UnitBase*>(getObject(*it));
        if (!campaignCombatUnit(unit)) {it=campaignWave.members.erase(it);continue;}
        if (unit->isBadlyDamaged() || unit->getAttackMode()==RETREAT
            || now-campaignWave.launched>=MILLI2CYCLES(profile.sortieMs)) {
            holdCampaignUnit(unit); it=campaignWave.members.erase(it); continue;
        }
        ++it;
    }
    if (hadWave || !campaignWave.members.empty()) campaignWave.lastActive=now;
    if (hadWave && campaignWave.members.empty())
        traceDecision("campaign_wave_end",AITelemetry::Record().set("recovery_ms",profile.recoveryMs));
    for (const auto* unit : getUnitList()) if (campaignCombatUnit(unit) && unit->isActive()
        && !campaignWave.members.count(unit->getObjectID())) {
        // Covers authored HUNT reinforcements and autonomous defensive pursuit.
        if (unit->getAttackMode()==HUNT || (unit->hasATarget() && !campaignLocalContact(unit->getTarget())))
            holdCampaignUnit(unit);
    }
}

const ObjectBase* QuantBot::campaignObjective(const UnitBase* unit, int group) const {
    const ObjectBase* chosen=nullptr; int best=INT32_MAX;
    for (const auto* building : getStructureList()) {
        if (building->getOwner()->getTeamID()==getHouse()->getTeamID() || building->getHealth()<=0
            || !building->isVisible(getHouse()->getTeamID())) continue;
        int score=blockDistance(unit->getLocation(),building->getLocation()).lround()*10;
        if (difficulty>=Difficulty::Hard && group>0
            && (building->getItemID()==Structure_Refinery || building->getItemID()==Structure_HeavyFactory)) score-=100;
        if (difficulty==Difficulty::Brutal && group>0)
            score+=(building->getOwner()->getHouseID()+getHouse()->getHouseID()+group)%3*30;
        if (score<best) {best=score;chosen=building;}
    }
    return chosen;
}

bool QuantBot::campaignControlsUnit(const UnitBase* unit) {
    if (!isCampaignEnemy() || !campaignCombatUnit(unit)) return false;
    if (!campaignWave.members.count(unit->getObjectID())) {
        if (!campaignLocalContact(unit->getTarget())) {holdCampaignUnit(unit);return true;}
        return unit->getItemID()==Unit_Saboteur; // Other defenders retain combat micro.
    }
    if (!unit->isActive() || !unit->isRespondable() || unit->isBadlyDamaged()) return true;
    // Ordinary combat micro may respond to close threats. Only redirect idle
    // survivors; never override an evasive move or a repair order.
    if (unit->isAGroundUnit() && !unit->hasATarget() && !unit->isMoving()) {
        const auto index=std::distance(campaignWave.members.begin(),campaignWave.members.find(unit->getObjectID()));
        const int group=difficulty>=Difficulty::Hard ? index%2 : 0;
        const auto* objective = group==0 ? getObject(campaignWave.front) : nullptr;
        if (!objective || objective->getHealth()<=0 || objective->getOwner()->getTeamID()==getHouse()->getTeamID()) {
            objective=campaignObjective(unit,group);
            if (group==0) campaignWave.front=objective ? objective->getObjectID() : NONE_ID;
        }
        if (objective) {
            doSetAttackMode(unit,AREAGUARD);
            doAttackObject(unit,objective,true);
        } else doSetAttackMode(unit,HUNT); // Explore when no enemy base is visible.
    }
    return false;
}
