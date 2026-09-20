#ifndef JOINPROGRESSWINDOW_H
#define JOINPROGRESSWINDOW_H
#include <GUI/Window.h>
#include <GUI/VBox.h>
#include <GUI/Label.h>
#include <GUI/ProgressBar.h>
#include <GUI/TextButton.h>
#include <Network/NetworkManager.h>
#include <globals.h>
class JoinProgressWindow : public Window {
public:
    JoinProgressWindow() : Window(0,0,580,196) {
        setWindowWidget(&box);
        const int width=std::min(580,getRendererWidth()-24);
        setCurrentPosition((getRendererWidth()-width)/2,(getRendererHeight()-196)/2,width,196);
        title.setText("Joining player"); title.setTextFontSize(18); box.addWidget(&title,28);
        player.setTextFontSize(22); player.setTextColor(COLOR_RGB(255,210,64));
        player.setAlignment(static_cast<Alignment_Enum>(Alignment_HCenter | Alignment_VCenter));
        box.addWidget(&player,84);
        box.addWidget(&status,44);
        cancel.setText("Cancel join and continue");
        cancel.setOnClick([](){if(pNetworkManager) pNetworkManager->cancelLateJoin();});
        box.addWidget(&cancel,32); refresh();
    }
    void refresh() {
        if(!pNetworkManager) return;
        title.setText(pNetworkManager->isSpectating() ? "Refreshing game state" : "Joining player");
        player.setText(pNetworkManager->lateJoinPlayerName().empty()
            ? settings.general.playerName : pNetworkManager->lateJoinPlayerName());
        status.setText(pNetworkManager->lateJoinProgressText());
        status.setProgress(pNetworkManager->lateJoinPercent());
        cancel.setVisible(pNetworkManager->isServer());
        cancel.setEnabled(pNetworkManager->canCancelLateJoin());
    }
private:
    VBox box; Label title, player; TextProgressBar status; TextButton cancel;
};
#endif
