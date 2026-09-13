#ifndef CAMPAIGN_DIFFICULTY_POLICY_H
#define CAMPAIGN_DIFFICULTY_POLICY_H

#include <algorithm>
#include <cstdint>
#include <set>

namespace CampaignDifficultyPolicy {
struct Profile {
    int houses, units, value, recoveryMs, graceMs, sortieMs, reservePercent;
    int enemyCommitPercent;
    bool limitedWave;
};
// Mission-scale trial values, in game time. Never grant free units/resources.
inline Profile profile(int difficulty, int tech) {
    const int stage = tech <= 3 ? 0 : tech <= 6 ? 1 : 2;
    switch (difficulty) {
        case 0: return {1, 3+stage, (3+stage)*300, 180000-stage*30000, 120000, 150000, 25, 50, true};
        case 1: return {1, 6+stage, (6+stage)*425, 120000-stage*15000, 60000, 180000, 15, 50, true};
        case 2: return {2, 10+stage*2, (10+stage*2)*550, 75000-stage*15000, 0, 240000, 10, 80, false};
        default:return {8, 16+stage*4, (16+stage*4)*700, 40000-stage*10000, 0, 300000, 5, 100, false};
    }
}
struct Wave {
    bool initialized = false;
    uint32_t opening = 0, launched = 0, lastActive = 0;
    uint32_t front = UINT32_MAX;
    std::set<uint32_t> members;
    template<class Stream> void save(Stream& s) const {
        s.writeBool(initialized); s.writeUint32(opening);
        s.writeUint32(launched); s.writeUint32(lastActive); s.writeUint32(front);
        s.writeUint32(static_cast<uint32_t>(members.size()));
        for (auto id : members) s.writeUint32(id);
    }
    template<class Stream> void load(Stream& s) {
        initialized=s.readBool(); opening=s.readUint32();
        launched=s.readUint32(); lastActive=s.readUint32(); front=s.readUint32();
        members.clear(); const auto n=s.readUint32();
        for (uint32_t i=0;i<n;++i) members.insert(s.readUint32());
    }
};
struct Pressure { int houses=0, units=0, value=0; uint32_t lastActive=0; };
inline int requiredArmy(const Profile& p, int configuredThreshold) {
    // Preserve mission-scaled readiness independently of commitment. Hard and
    // Brutal use units/value for readiness only, not to cap their assault.
    return std::min(std::max(0,configuredThreshold),2*p.value);
}
inline bool canLaunch(const Profile& p, const Pressure& used, uint32_t now,
                      uint32_t opening, uint32_t recoveryCycles) {
    return now >= opening && used.houses < p.houses
        && (!p.limitedWave || (used.units < p.units && used.value < p.value))
        && (used.houses > 0 || used.lastActive == 0
            || now-used.lastActive >= recoveryCycles);
}
inline bool fits(const Profile& p, const Pressure& used, int value) {
    return value > 0 && (!p.limitedWave || (used.units < p.units && value <= p.value-used.value));
}
inline bool needsWindtrap(int produced, int required, int queuedDemand, int nextDemand,
                          bool generatorPending) {
    return !generatorPending && produced < required + std::max(0,queuedDemand) + std::max(0,nextDemand);
}
}
#endif
