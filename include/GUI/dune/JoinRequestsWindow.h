#ifndef JOINREQUESTSWINDOW_H
#define JOINREQUESTSWINDOW_H
#include <GUI/Window.h>
#include <GUI/VBox.h>
#include <GUI/Label.h>
#include <GUI/DropDownBox.h>
#include <GUI/TextButton.h>
#include <Game.h>
#include <Network/NetworkManager.h>
#include <globals.h>

class JoinRequestsWindow : public Window {
public:
    static JoinRequestsWindow* create() { auto* w=new JoinRequestsWindow(); w->pAllocated=true; return w; }
private:
    VBox box;
    Label title, help;
    DropDownBox requests, slots;
    TextButton accept, decline, close;
    std::vector<DirectRoomTransport::JoinRequest> pending;
    std::vector<Game::JoinSlot> choices;
    JoinRequestsWindow() : Window(0,0,540,270) {
        setWindowWidget(&box);
        setCurrentPosition((getRendererWidth()-540)/2,(getRendererHeight()-270)/2,540,270);
        title.setText("Join requests"); title.setTextFontSize(22); box.addWidget(&title,36);
        help.setText("Choose a player and the house they will control."); box.addWidget(&help,36);
        if(pNetworkManager && pNetworkManager->getDirectTransport()) pending=pNetworkManager->getDirectTransport()->joinRequests();
        pending.erase(std::remove_if(pending.begin(),pending.end(),[](const auto& r){return r.spectator;}),pending.end());
        choices=currentGame->availableJoinSlots();
        for(const auto& p : pending) requests.addEntry(p.name);
        for(const auto& s : choices) slots.addEntry(s.label);
        if(!pending.empty()) requests.setSelectedItem(0);
        if(!choices.empty()) slots.setSelectedItem(0);
        box.addWidget(&requests,32); box.addWidget(&slots,32);
        accept.setText("Accept and synchronize"); accept.setEnabled(!pending.empty() && !choices.empty());
        accept.setOnClick([this]() {
            const int p=requests.getSelectedIndex(), s=slots.getSelectedIndex();
            if(p<0 || s<0) return;
            if(currentGame->acceptJoinRequest(pending[p].id,pending[p].name,choices[s])) currentGame->resumeGame();
            else help.setText(pNetworkManager->lateJoinStatus());
        });
        decline.setText("Reject join - allow spectating"); decline.setEnabled(!pending.empty());
        decline.setOnClick([this]() {
            const int p=requests.getSelectedIndex(); if(p<0) return;
            pNetworkManager->getDirectTransport()->manageJoin("decline",pending[p].id);
            closeWindow();
        });
        close.setText("Back"); close.setOnClick([this](){closeWindow();});
        box.addWidget(&accept,36); box.addWidget(&decline,32); box.addWidget(&close,32);
        if(pending.empty()) help.setText("No players are requesting to join.");
        else if(choices.empty()) help.setText("No eligible slots remain in this game mode.");
    }
    void closeWindow() { if(auto* parent=dynamic_cast<Window*>(getParent())) parent->closeChildWindow(); }
};
#endif
