/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef CHAOSFACTORY_H
#define CHAOSFACTORY_H

#include <structures/BuilderBase.h>

#include <array>

class ChaosFactory final : public BuilderBase
{
public:
    explicit ChaosFactory(House* newOwner);
    explicit ChaosFactory(InputStream& stream);
    ~ChaosFactory() override;

    void save(OutputStream& stream) const override;
    void updateBuildList() override;
    void doProduceItem(Uint32 itemID, bool multipleMode = false) override;
    void doCancelItem(Uint32 itemID, bool multipleMode = false) override;
    void doBuildRandom() override;

protected:
    void updateStructureSpecificStuff() override;

private:
    static constexpr int OfferCount = 2;
    static constexpr int StockPerOffer = 3;

    void init();
    void refreshOffers(bool initialRound);
    void rebuildOfferList();
    void cancelUnfinishedOrders();
    bool isOfferCandidate(Uint32 candidate) const;
    int getOfferIndex(Uint32 candidate) const;
    int getBasePrice(Uint32 candidate) const;
    int getRandomizedPrice(Uint32 candidate) const;

public:
    int getRemainingStock(Uint32 candidate) const;
    int getOfferSecondsRemaining() const;

    std::array<Uint32, OfferCount> offeredItems{{ItemID_Invalid, ItemID_Invalid}};
    std::array<int, OfferCount> offeredPrices{{0, 0}};
    std::array<int, OfferCount> remainingStock{{0, 0}};
    Uint32 offerCycleStart = 0;
};

#endif // CHAOSFACTORY_H
