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

int QuantBot::campaignRequiredArmy(int configuredThreshold) const {
    int required = isCampaignEnemy()
        ? CampaignDifficultyPolicy::requiredArmy(campaignProfile(), configuredThreshold)
        : configuredThreshold;
    // Once the map is depleted, stop waiting for a larger army. A leftover
    // cash balance or a few cheap survivors must not keep the game idle.
    // Dispatch still enforces home reserves, opening and enemy wave limits.
    // Delay this fallback beyond the initial spice scan and opening phase.
    if (getGameCycleCount() >= MILLI2CYCLES(15 * 60000) && lastCalculatedSpice == 0)
        required = 0;
    return required;
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
            || !getQuantBotConfig().getSettings(static_cast<int>(bot->difficulty)).attackEnabled) continue;
        int value=0; bool usable=false;
        for (const auto* unit : getUnitList()) if (bot->campaignCombatUnit(unit)) {
            value+=currentGame->objectData.data[unit->getItemID()][unit->getOriginalHouseID()].price;
            usable |= unit->isActive() && unit->isRespondable() && !unit->isBadlyDamaged()
                && unit->getAttackMode()!=RETREAT && !unit->hasATarget();
        }
        const auto& settings=getQuantBotConfig().getSettings(static_cast<int>(bot->difficulty));
        const int ready=bot->campaignRequiredArmy(
            (bot->militaryValueLimit * (FixPoint(static_cast<int>(settings.attackThresholdPercent*100))/100)).lround());
        if (!usable || value < ready) continue;
        if (std::make_pair(bot->campaignWave.launched,bot->getHouse()->getHouseID())
            < std::make_pair(campaignWave.launched,getHouse()->getHouseID())) return false;
    }
    return true;
}

bool QuantBot::campaignLocalContact(const ObjectBase* target) const {
    if (!target || target->getHealth()<=0 || !target->isActive()) return false;
    // Artillery can fire from beyond the old seven-tile perimeter. Include its
    // firing range so a base cannot be shelled without its defenders responding.
    const int radius=std::max(difficulty<=Difficulty::Medium ? 7 : 10,target->getWeaponRange()+2);
    for (const auto* building : getStructureList())
        if (building->getOwner()==getHouse() && building->getHealth()>0
            && blockDistance(target->getLocation(),building->getClosestPoint(target->getLocation()))<=radius) return true;
    // Workers need protection wherever the spice field is. The contact remains
    // bounded around the worker, including an artillery attacker's firing range.
    for (const auto* worker : getUnitList())
        if (worker->getOwner()==getHouse() && worker->getItemID()==Unit_Harvester && worker->isActive()
            && blockDistance(target->getLocation(),worker->getLocation())<=std::max(4,target->getWeaponRange()+2)) return true;
    return false;
}

bool QuantBot::campaignDefensiveContact(const UnitBase* unit, const ObjectBase* target) const {
    if (!target || target->getHealth()<=0 || !target->isActive()) return false;
    if (campaignLocalContact(target)) return true;
    // A lone defender also fights back when shot outside the base perimeter.
    // Anchor this response to where it was hit, rather than authorizing a chase
    // across the map. Existing assignment/guard-point state survives save/load.
    const auto assigned=defenceAssignments.find(unit->getObjectID());
    return assigned!=defenceAssignments.end() && assigned->second==target->getObjectID()
        && blockDistance(target->getLocation(),unit->getGuardPoint())
            <= std::max(unit->getWeaponRange(),target->getWeaponRange())+2;
}

void QuantBot::holdCampaignUnit(const UnitBase* unit) {
    if (!unit->isActive() || !unit->isRespondable() || unit->getAttackMode()==RETREAT) return;
    const StructureBase* home=nullptr; int distance=INT32_MAX;
    for (const auto* building : getStructureList()) {
        if (building->getOwner()!=getHouse() || building->getHealth()<=0) continue;
        int d=blockDistance(unit->getLocation(),building->getLocation()).lround();
        if (d<distance) {home=building;distance=d;}
    }
    const Coord point=home ? home->getClosestPoint(unit->getLocation()) : unit->getLocation();
    defenceAssignments.erase(unit->getObjectID());
    if (unit->hasATarget()) doMove2Pos(unit,unit->getX(),unit->getY(),false);
    if (unit->hasATarget() || unit->getAttackMode()!=AREAGUARD) doSetAttackMode(unit,AREAGUARD);
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
        if (unit->getAttackMode()==HUNT || (unit->hasATarget() && !campaignDefensiveContact(unit,unit->getTarget())))
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

bool QuantBot::scoutCampaignFront(const UnitBase* unit) {
    // HUNT can repeatedly acquire a distant carryall, which ground units then
    // refuse to chase. Explore terrain when there is no visible base instead
    // of treating HUNT alone as an exploration order. No hidden objects are read.
    if (!unit->isAGroundUnit() || !unit->isActive() || !unit->isRespondable()
        || unit->isMoving() || unit->hasATarget() || unit->isBadlyDamaged()
        || unit->getAttackMode()==RETREAT || humanControls(unit)) return false;
    Coord destination=Coord::Invalid();int best=-1;
    for (int y=0;y<getMap().getSizeY();++y) for (int x=0;x<getMap().getSizeX();++x) {
        if (getMap().getTile(x,y)->isExploredByTeam(getHouse()->getTeamID()) || !unit->canPass(x,y)) continue;
        const int distance=blockDistance(unit->getLocation(),Coord(x,y)).lround();
        if (distance>best) {best=distance;destination=Coord(x,y);}
    }
    if (!destination.isValid()) return false;
    doMove2Pos(unit,destination.x,destination.y,true);
    doSetAttackMode(unit,HUNT);
    traceDecision("campaign_scout",AITelemetry::Record().set("unit",unit->getObjectID())
        .set("x",destination.x).set("y",destination.y));
    return true;
}

bool QuantBot::campaignControlsUnit(const UnitBase* unit) {
    if (!campaignCombatUnit(unit)) return false;
    if (!isCampaignEnemy()) {
        // Only an already dispatched helper attacker can scout. Home guards,
        // economy-only support and manual orders keep their existing roles.
        if (isCampaignGameType(currentGame->gameType) && !supportMode
            && unit->isActive() && unit->isRespondable() && !unit->isBadlyDamaged()
            && !humanControls(unit) && unit->getAttackMode()==HUNT
            && !unit->isMoving() && !unit->hasATarget()) {
            if (const auto* objective=campaignObjective(unit,0)) {
                doAttackObject(unit,objective,true);
                return true;
            }
            return scoutCampaignFront(unit);
        }
        return false;
    }
    if (!campaignWave.members.count(unit->getObjectID())) {
        if (!campaignDefensiveContact(unit,unit->getTarget())) {holdCampaignUnit(unit);return true;}
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
        } else if (!scoutCampaignFront(unit)) doSetAttackMode(unit,HUNT);
    }
    return false;
}
