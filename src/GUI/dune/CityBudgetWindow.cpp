/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Dune Legacy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dune Legacy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <GUI/dune/CityBudgetWindow.h>

#include <globals.h>
#include <Game.h>
#include <House.h>
#include <dunecity/CitySimulation.h>
#include <dunecity/CityEffects.h>
#include <Command.h>

#include <FileClasses/TextManager.h>
#include <FileClasses/GFXManager.h>
#include <misc/format.h>
#include <misc/MenuPalette.h>

#include <algorithm>

namespace {

constexpr int kBudgetWindowWidth = 620;
constexpr int kBudgetWindowHeight = 460;

Uint32 centeredCoordinate(int available, int extent) {
    return static_cast<Uint32>(std::max(0, (available - extent) / 2));
}

void configureSectionHeading(Label& label, const std::string& text) {
    label.setText(text);
    label.setTextFontSize(15);
    label.setTextColor(MenuTheme::accent,COLOR_TRANSPARENT);
}

void configureValueLabel(Label& label, Alignment_Enum alignment = Alignment_Left) {
    label.setTextColor(COLOR_WHITE);
    label.setTextFontSize(14);
    label.setAlignment(alignment);
}

} // namespace

CityBudgetWindow::CityBudgetWindow()
 : Window(centeredCoordinate(getRendererWidth(), kBudgetWindowWidth),
          centeredCoordinate(getRendererHeight(), kBudgetWindowHeight),
          kBudgetWindowWidth, kBudgetWindowHeight) {

    // Non-modal: clicks outside this window dismiss it and pass through to
    // the underlying interface, so the player can still hit build buttons,
    // scroll the starport list, and select units without first finding the
    // Close button.
    setModal(false);

    setWindowWidget(&rootHBox);
    rootHBox.addWidget(HSpacer::create(18));rootHBox.addWidget(&mainVBox);rootHBox.addWidget(HSpacer::create(18));
    mainVBox.addWidget(VSpacer::create(12));
    titleLabel.setText(_("City Budget"));titleLabel.setAlignment(Alignment_HCenter);
    titleLabel.setTextColor(MenuTheme::text,COLOR_TRANSPARENT);titleLabel.setTextFontSize(24);
    mainVBox.addWidget(&titleLabel,30);
    configureValueLabel(yearLabel);configureValueLabel(treasuryLabel,Alignment_Right);
    summaryHBox.addWidget(&yearLabel);summaryHBox.addWidget(&treasuryLabel);
    mainVBox.addWidget(&summaryHBox,26);mainVBox.addWidget(VSpacer::create(8));
    configureSectionHeading(allocationsHeadingLabel,_("Allocations"));
    mainVBox.addWidget(&allocationsHeadingLabel,20);
    auto allocation=[&](HBox& row,Label& label,TextButton& minus,Label& value,TextButton& plus,
                        const char* text,auto decrease,auto increase) {
        label.setText(_(text));configureValueLabel(label);row.addWidget(&label);
        minus.setText("-");minus.setOnClick(decrease);row.addWidget(&minus,28);
        configureValueLabel(value,Alignment_HCenter);row.addWidget(&value,48);
        plus.setText("+");plus.setOnClick(increase);row.addWidget(&plus,28);
    };
    allocation(taxHBox,taxLabel,taxMinus,taxValueLabel,taxPlus,"Tax rate",
        std::bind(&CityBudgetWindow::onTaxDecrease,this),std::bind(&CityBudgetWindow::onTaxIncrease,this));
    allocation(policeHBox,policeLabel,policeMinus,policeValueLabel,policePlus,"Police funding",
        std::bind(&CityBudgetWindow::onPoliceDecrease,this),std::bind(&CityBudgetWindow::onPoliceIncrease,this));
    allocationPairHBox.addWidget(&taxHBox);allocationPairHBox.addWidget(HSpacer::create(16));allocationPairHBox.addWidget(&policeHBox);
    mainVBox.addWidget(&allocationPairHBox,36);mainVBox.addWidget(VSpacer::create(12));
    configureSectionHeading(forecastHeadingLabel,_("Annual forecast"));forecastVBox.addWidget(&forecastHeadingLabel,22);
    for(auto* label : {&incomeLabel,&policeCostLabel,&policeStationCostLabel,&rocketTurretCostLabel,
                       &gunTurretCostLabel,&roadCostLabel,&netLabel,&perSecondLabel}) {
        configureValueLabel(*label);forecastVBox.addWidget(label,22);
    }
    configureSectionHeading(cityStatusHeadingLabel,_("City status"));statusVBox.addWidget(&cityStatusHeadingLabel,22);
    for(auto* label : {&totalPopLabel,&unemploymentLabel,&resPopLabel,&comPopLabel,&indPopLabel,
                       &servicesLabel,&environmentLabel,&crimeTrafficLabel}) {
        configureValueLabel(*label);
        statusVBox.addWidget(label,(label==&servicesLabel || label==&environmentLabel || label==&crimeTrafficLabel) ? 36 : 22);
    }
    detailsHBox.addWidget(&forecastVBox);detailsHBox.addWidget(HSpacer::create(16));detailsHBox.addWidget(&statusVBox);
    mainVBox.addWidget(&detailsHBox,240);mainVBox.addWidget(Spacer::create());
    confirmButton.setText(_("Apply"));confirmButton.setOnClick(std::bind(&CityBudgetWindow::onConfirm,this));
    cancelButton.setText(_("Cancel"));cancelButton.setOnClick(std::bind(&CityBudgetWindow::onCancel,this));
    buttonsHBox.addWidget(&confirmButton);buttonsHBox.addWidget(HSpacer::create(12));buttonsHBox.addWidget(&cancelButton);
    mainVBox.addWidget(&buttonsHBox,40);mainVBox.addWidget(VSpacer::create(12));

    // Snapshot the live tax rate and funding % so the sliders open at the
    // current settings rather than the header defaults. updateDisplay()
    // refreshes the readouts but does NOT clobber pendingTaxRate /
    // pendingPolicePercent on later ticks (the player may already be
    // mid-edit).
    auto* citySim = currentGame ? currentGame->getCitySimulation() : nullptr;
    if (citySim && citySim->isInitialized()) {
        pendingPolicePercent = citySim->getPoliceFundingPercent();
        pendingTaxRate       = citySim->getCityTax();
    }
    updateDisplay();
}

CityBudgetWindow::~CityBudgetWindow() = default;

void CityBudgetWindow::draw(Point position) {
    updateDisplay();
    Window::draw(position);
}

void CityBudgetWindow::onCancel() {
    Window* pParentWindow = dynamic_cast<Window*>(getParent());
    if(pParentWindow != nullptr) {
        pParentWindow->closeChildWindow();
    }
}

void CityBudgetWindow::onPoliceIncrease() {
    if (pendingPolicePercent < 100) {
        pendingPolicePercent += 5;
        if (pendingPolicePercent > 100) pendingPolicePercent = 100;
        updateAllocationLabels();
    }
}

void CityBudgetWindow::onPoliceDecrease() {
    if (pendingPolicePercent > 0) {
        pendingPolicePercent -= 5;
        if (pendingPolicePercent < 0) pendingPolicePercent = 0;
        updateAllocationLabels();
    }
}

void CityBudgetWindow::onTaxIncrease() {
    if (pendingTaxRate < DuneCity::CitySimulation::kMaxTaxRate) {
        ++pendingTaxRate;
        updateAllocationLabels();
    }
}

void CityBudgetWindow::onTaxDecrease() {
    if (pendingTaxRate > DuneCity::CitySimulation::kMinTaxRate) {
        --pendingTaxRate;
        updateAllocationLabels();
    }
}

void CityBudgetWindow::onConfirm() {
    // Route through the command system so multiplayer remains
    // deterministic. p0 reserved (legacy houseID slot for tax),
    // p1 = new tax rate.
    currentGame->getCommandManager().addCommand(
        Command(pLocalPlayer->getPlayerID(), CMD_CITY_SET_TAX_RATE,
                0u, static_cast<uint32_t>(pendingTaxRate), 0u));
    currentGame->getCommandManager().addCommand(
        Command(pLocalPlayer->getPlayerID(), CMD_CITY_SET_BUDGET,
                static_cast<uint32_t>(pendingPolicePercent), 0u, 0u));
    onCancel();
}

void CityBudgetWindow::updateAllocationLabels() {
    taxValueLabel.setText(fmt::sprintf("%d%%", pendingTaxRate));
    policeValueLabel.setText(fmt::sprintf("%d%%", pendingPolicePercent));
}

void CityBudgetWindow::updateDisplay() {
    auto* citySim = currentGame ? currentGame->getCitySimulation() : nullptr;
    if (!citySim || !citySim->isInitialized()) {
        return;
    }

    yearLabel.setText(fmt::sprintf("Year: %d", citySim->getCityYear()));
    treasuryLabel.setText(fmt::sprintf("Treasury: %d credits", citySim->getTotalFunds()));

    // Projected annual revenue using the pending tax slider and land value.
    const int taxBaseEighths = citySim->getTaxBaseEighths();
    const int taxRate   = pendingTaxRate;
    const int avgLV     = citySim->getAvgLandValue();
    const int projected = DuneCity::computeAnnualTaxRevenue(taxBaseEighths, taxRate, avgLV);
    incomeLabel.setText(fmt::sprintf("Projected Tax: +%d/yr", projected));

    // Police: nominal cost is full-funded; actual paid is scaled by the
    // selected funding percentage, including pending slider changes.
    const int stationCount = pLocalHouse ? pLocalHouse->getNumItems(Structure_PoliceStation) : 0;
    const int rocketCount = pLocalHouse ? pLocalHouse->getNumItems(Structure_RocketTurret) : 0;
    const int gunCount = pLocalHouse ? pLocalHouse->getNumItems(Structure_GunTurret) : 0;
    const FixPoint stationPaying = DuneCity::getPoliceAnnualCost(Structure_PoliceStation) * stationCount * pendingPolicePercent / 100;
    const FixPoint rocketPaying = DuneCity::getPoliceAnnualCost(Structure_RocketTurret) * rocketCount * pendingPolicePercent / 100;
    const FixPoint gunPaying = DuneCity::getPoliceAnnualCost(Structure_GunTurret) * gunCount * pendingPolicePercent / 100;
    const FixPoint paying = stationPaying + rocketPaying + gunPaying;
    roadCostLabel.setText("Roads: no upkeep");
    auto credits = [](FixPoint amount) {
        std::string text = fmt::sprintf("%.3f", amount.toDouble());
        while (!text.empty() && text.back() == '0') text.pop_back();
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text;
    };
    policeStationCostLabel.setText(fmt::sprintf("Police stations: %d | -%s/yr", stationCount, credits(stationPaying)));
    rocketTurretCostLabel.setText(fmt::sprintf("Rocket turrets: %d | -%s/yr", rocketCount, credits(rocketPaying)));
    gunTurretCostLabel.setText(fmt::sprintf("Gun turrets: %d | -%s/yr", gunCount, credits(gunPaying)));
    policeCostLabel.setText("Services: -" + credits(paying) + "/yr");
    const FixPoint netAnnual = FixPoint(projected) - paying;
    netLabel.setText(fmt::sprintf("Net Annual: %+.1f/yr", netAnnual.toDouble()));
    perSecondLabel.setText(fmt::sprintf("Cash Flow: %+.1f/sec", (netAnnual / 60).toDouble()));

    // The slider's pending value is seeded once in the constructor so
    // subsequent +/- clicks edit the pending copy without being clobbered.
    updateAllocationLabels();

    resPopLabel.setText(fmt::sprintf("Residential: %d", citySim->getDisplayResPop()));
    comPopLabel.setText(fmt::sprintf("Commercial: %d", citySim->getDisplayComPop()));
    indPopLabel.setText(fmt::sprintf("Industrial: %d", citySim->getDisplayIndPop()));
    totalPopLabel.setText(fmt::sprintf("Population: %d", citySim->getDisplayTotalPop()));

    // Unemployment
    const int unemp = citySim->getUnemploymentRate();
    unemploymentLabel.setText(fmt::sprintf("Unemployment: %d%%", unemp));
    unemploymentLabel.setTextColor(unemp > 20 ? COLOR_RGB(255,80,80) : COLOR_WHITE);

    // Hospital/church count (auto-created by game on residential zones)
    servicesLabel.setText(fmt::sprintf("Hospitals: %d\nChurches: %d",
                                       citySim->getHospitalCount(), citySim->getChurchCount()));

    const auto& environment = citySim->getEnvironmentStatus(
        pLocalHouse ? pLocalHouse->getHouseID() : 0);
    if (environment.sampledStructures == 0) {
        environmentLabel.setText("Land Value: —\nPollution: —");
        crimeTrafficLabel.setText("Crime: —\nTraffic: —");
    } else {
        environmentLabel.setText(std::string("Land Value: ")
            + DuneCity::landValueCategory(environment.averageLandValue)
            + "\nPollution: " + DuneCity::pollutionCategory(environment.averagePollution));
        crimeTrafficLabel.setText(std::string("Crime: ")
            + DuneCity::crimeCategory(environment.averageCrime)
            + "\nTraffic: "
            + (environment.averageTraffic < 64 ? "Light" : environment.averageTraffic < 128 ? "Moderate" : "Heavy"));
    }
}
