/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SPECIALVEHICLE_H
#define SPECIALVEHICLE_H

#include <data.h>
#include <globals.h>

#include <units/HarvesterHelpers.h>

#include <vector>

struct HouseSpecialVehicleCandidateData {
    bool enabled = false;
    int builder = ItemID_Invalid;
    bool requiresHouseIx = false;
};

inline bool isSpecialVehicleSelectionCandidate(int itemID) {
    return isUnit(itemID)
        && !isFlyingUnit(itemID)
        && !isInfantryUnit(itemID)
        && !isHarvesterLikeUnit(itemID);
}

inline bool isHouseSpecialVehicleCandidate(
        int itemID,
        const HouseSpecialVehicleCandidateData& candidateData) {
    return candidateData.enabled
        && isSpecialVehicleSelectionCandidate(itemID)
        && candidateData.builder != ItemID_Invalid
        && candidateData.requiresHouseIx;
}

template<typename CandidateDataProvider>
inline std::vector<int> discoverHouseSpecialVehicleCandidates(
        CandidateDataProvider&& candidateDataProvider) {
    std::vector<int> candidates;
    for(int itemID = ItemID_FirstID; itemID <= ItemID_LastID; ++itemID) {
        if(isHouseSpecialVehicleCandidate(itemID, candidateDataProvider(itemID))) {
            candidates.push_back(itemID);
        }
    }

    return candidates;
}

inline std::vector<int> getSpecialVehicleFallbackPoolForHouse(
        int house,
        bool tornieActive) {
    if(tornieActive) {
        switch(getHouseFactionIdentity(static_cast<HOUSETYPE>(house))) {
            case HOUSE_HARKONNEN:  return { Unit_Devastator, Unit_FlameTank };
            case HOUSE_ATREIDES:   return { Unit_SonicTank, Unit_EliteLauncher };
            case HOUSE_ORDOS:      return { Unit_Deviator, Unit_EliteSiegeTank };
            case HOUSE_FREMEN:     return { Unit_EliteSiegeTank, Unit_FlameTank };
            case HOUSE_SARDAUKAR:  return { Unit_Devastator, Unit_SonicTank };
            case HOUSE_MERCENARY:  return { Unit_Devastator, Unit_Deviator };
            case HOUSE_NEUTRAL:    return { Unit_EliteLauncher, Unit_EliteSiegeTank };
            case HOUSE_REBELS:     return { Unit_SonicTank, Unit_FlameTank };
            case HOUSE_CUSTOM:     return { Unit_Devastator, Unit_EliteSiegeTank };
            case HOUSE_WILDSPADE:  return { Unit_EliteLauncher, Unit_FlameTank };
            case HOUSE_KLESHMERSH: return { Unit_FlameTank, Unit_EliteSiegeTank };
            case HOUSE_THARPIQUE:  return { Unit_Deviator, Unit_EliteLauncher };
            default:               return {};
        }
    }

    switch(house) {
        case HOUSE_HARKONNEN:  return { Unit_Devastator };
        case HOUSE_ATREIDES:   return { Unit_SonicTank };
        case HOUSE_ORDOS:      return { Unit_Deviator };
        case HOUSE_FREMEN:
        case HOUSE_SARDAUKAR:
        case HOUSE_MERCENARY:
        case HOUSE_NEUTRAL:
        case HOUSE_REBELS:
        case HOUSE_CUSTOM:     return { Unit_SonicTank, Unit_Devastator };
        default:               return {};
    }
}

inline std::vector<int> resolveSpecialVehiclePoolForHouse(
        int house,
        bool tornieActive,
        const std::vector<int>& objectDataIxCandidates) {
    const HOUSETYPE identity = getHouseFactionIdentity(static_cast<HOUSETYPE>(house));
    if(tornieActive) {
        if(identity == HOUSE_CUSTOM) {
            return { Unit_Devastator, Unit_EliteSiegeTank };
        }
        if(!objectDataIxCandidates.empty()) {
            return objectDataIxCandidates;
        }
    }

    // Preserve the generic custom-house behavior used by every other mod.
    if(identity == HOUSE_CUSTOM && !objectDataIxCandidates.empty()) {
        return objectDataIxCandidates;
    }

    return getSpecialVehicleFallbackPoolForHouse(house, tornieActive);
}

#endif // SPECIALVEHICLE_H
