#ifndef JOINPROGRESSWINDOW_H
#define JOINPROGRESSWINDOW_H
#include <GUI/Window.h>
#include <GUI/VBox.h>
#include <GUI/Label.h>
#include <GUI/TextButton.h>
#include <Network/NetworkManager.h>
#include <globals.h>
class JoinProgressWindow : public Window {
public:
    JoinProgressWindow() : Window(0,0,500,116) {
        setWindowWidget(&box);
        setCurrentPosition((getRendererWidth()-500)/2,(getRendererHeight()-116)/2,500,116);
        title.setText("Adding a player"); title.setTextFontSize(20); box.addWidget(&title,32);
        box.addWidget(&status,44);
        cancel.setText("Cancel join and continue");
        cancel.setOnClick([](){if(pNetworkManager) pNetworkManager->cancelLateJoin();});
        box.addWidget(&cancel,32); refresh();
    }
    void refresh() {
        if(!pNetworkManager) return;
        status.setText(pNetworkManager->lateJoinStatus());
        cancel.setVisible(pNetworkManager->isServer());
        cancel.setEnabled(pNetworkManager->canCancelLateJoin());
    }
private:
    VBox box; Label title,status; TextButton cancel;
};
#endif
