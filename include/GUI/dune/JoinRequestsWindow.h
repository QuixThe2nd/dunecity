#ifndef JOINREQUESTSWINDOW_H
#define JOINREQUESTSWINDOW_H
#include <GUI/Window.h>
#include <GUI/VBox.h>
#include <GUI/Label.h>
#include <GUI/DropDownBox.h>
#include <GUI/TextButton.h>
#include <Game.h>
#include <House.h>
#include <Network/NetworkManager.h>
#include <globals.h>

class JoinRequestsWindow : public Window {
public:
    static JoinRequestsWindow* create() { auto* w=new JoinRequestsWindow(); w->pAllocated=true; return w; }
private:
    VBox box;
    Label title, player, help;
    DropDownBox requests, slots;
    TextButton accept, decline, close;
    std::vector<DirectRoomTransport::JoinRequest> pending;
    std::vector<Game::JoinSlot> choices;
public:
    JoinRequestsWindow() : Window(0,0,580,350) {
        setWindowWidget(&box);
        const int width=std::min(580,getRendererWidth()-24);
        setCurrentPosition((getRendererWidth()-width)/2,(getRendererHeight()-350)/2,width,350);
        title.setText("Request to play"); title.setTextFontSize(20); box.addWidget(&title,32);
        player.setTextFontSize(22); player.setTextColor(COLOR_RGB(255,210,64));
        player.setAlignment(static_cast<Alignment_Enum>(Alignment_HCenter | Alignment_VCenter));
        box.addWidget(&player,84);
        help.setText("Share keeps the current player. Replace removes the AI."); box.addWidget(&help,36);
        if(pNetworkManager && pNetworkManager->getDirectTransport()) pending=pNetworkManager->getDirectTransport()->joinRequests();
        pending.erase(std::remove_if(pending.begin(),pending.end(),[](const auto& r){return r.spectator;}),pending.end());
        choices=currentGame->availableJoinSlots();
        for(const auto& p : pending) requests.addEntry(p.name);
        requests.setOnSelectionChange([this](bool) {
            const int selected=requests.getSelectedIndex();
            player.setText(selected >= 0 ? pending[selected].name : "No pending requests");
        });
        for(const auto& s : choices) slots.addEntry(s.label);
        if(!pending.empty()) requests.setSelectedItem(0);
        player.setText(pending.empty() ? "No pending requests" : pending.front().name);
        if(!choices.empty()) {
            // Prefer adding a second controller over removing an existing AI.
            auto shared=std::find_if(choices.begin(),choices.end(),[](const auto& slot) {
                return slot.controller==static_cast<int>(currentGame->getHouse(slot.house)->getPlayerList().size());
            });
            slots.setSelectedItem(shared==choices.end() ? 0 : static_cast<int>(shared-choices.begin()));
        }
        box.addWidget(&requests,32); box.addWidget(&slots,32);
        accept.setText("Allow player to join"); accept.setEnabled(!pending.empty() && !choices.empty());
        accept.setOnClick([this]() {
            const int p=requests.getSelectedIndex(), s=slots.getSelectedIndex();
            if(p<0 || s<0) return;
            if(currentGame->acceptJoinRequest(pending[p].id,pending[p].name,choices[s])) currentGame->resumeGame();
            else help.setText(pNetworkManager->lateJoinStatus());
        });
        decline.setText("Decline - keep spectating"); decline.setEnabled(!pending.empty());
        decline.setOnClick([this]() {
            const int p=requests.getSelectedIndex(); if(p<0) return;
            pNetworkManager->getDirectTransport()->manageJoin("decline",pending[p].id);
            closeWindow();
        });
        close.setText("Later"); close.setOnClick([this](){closeWindow();});
        box.addWidget(&accept,36); box.addWidget(&decline,32); box.addWidget(&close,32);
        if(pending.empty()) help.setText("No players are requesting to join.");
        else if(choices.empty()) help.setText("No eligible slots remain in this game mode.");
    }
    void closeWindow() { if(auto* parent=dynamic_cast<Window*>(getParent())) parent->closeChildWindow(); else currentGame->resumeGame(); }
};
#endif
