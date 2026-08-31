/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef LOVEFACTORY_H
#define LOVEFACTORY_H

#include <structures/StarPort.h>

class LoveFactory final : public StarPort
{
public:
    explicit LoveFactory(House* newOwner);
    explicit LoveFactory(InputStream& stream);
    ~LoveFactory() override;

    void save(OutputStream& stream) const override;

    void handleProduceItemClick(Uint32 itemID, bool multipleMode = false) override;
    void doProduceItem(Uint32 itemID, bool multipleMode = false) override;
    void doCancelItem(Uint32 itemID, bool multipleMode = false) override;
    void doBuildRandom() override;
    void updateBuildList() override;

protected:
    void updateStructureSpecificStuff() override;
    void deployOrderedItem(Uint32 orderedItemID) override;

private:
    void ensureDeliveryStock();
    bool isDeliveryChoice(Uint32 itemID) const;
    bool isDeliveryCandidate(Uint32 unitItemID, Uint32 deliveryID) const;
    int getDeliveryMaximum(Uint32 deliveryID) const;
    int getRandomizedPrice(Uint32 deliveryID) const;

    Uint32 lastPriceUpdateCycle;
};

#endif // LOVEFACTORY_H
