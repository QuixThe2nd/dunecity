#ifndef JOINMODEWINDOW_H
#define JOINMODEWINDOW_H
#include <GUI/Window.h>
#include <GUI/VBox.h>
#include <GUI/Label.h>
#include <GUI/TextButton.h>

class JoinModeWindow : public Window {
public:
    enum class Choice { Cancel, Play, Spectate };
    Choice choice=Choice::Cancel;
    static JoinModeWindow* create() { auto* w=new JoinModeWindow(); w->pAllocated=true; return w; }
private:
    VBox box;
    Label title, help;
    TextButton play, spectate, cancel;
    JoinModeWindow() : Window(0,0,500,212) {
        setWindowWidget(&box);
        setCurrentPosition((getRendererWidth()-500)/2,(getRendererHeight()-212)/2,500,212);
        title.setText("Join this running game"); title.setTextFontSize(22); box.addWidget(&title,40);
        help.setText("Request a player slot, or watch as a spectator."); box.addWidget(&help,40);
        play.setText("Request to play"); spectate.setText("Spectate"); cancel.setText("Cancel");
        play.setOnClick([this](){finish(Choice::Play);});
        spectate.setOnClick([this](){finish(Choice::Spectate);});
        cancel.setOnClick([this](){finish(Choice::Cancel);});
        box.addWidget(&play,44); box.addWidget(&spectate,44); box.addWidget(&cancel,44);
    }
    void finish(Choice result) { choice=result; if(auto* parent=dynamic_cast<Window*>(getParent())) parent->closeChildWindow(); }
};
#endif
