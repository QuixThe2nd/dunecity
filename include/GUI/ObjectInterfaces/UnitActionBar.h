#ifndef DUNECITY_UNIT_ACTION_BAR_H
#define DUNECITY_UNIT_ACTION_BAR_H

#include <GUI/StaticContainer.h>
#include <initializer_list>

// Keep available unit actions together without reserving blank icon slots.
class UnitActionBar : public StaticContainer {
public:
    UnitActionBar() { enableResizing(true, false); }

    void setButtons(std::initializer_list<Widget*> buttons) {
        for(auto* button : buttons) addWidget(button, Point(0,0), Point(26,26));
        refresh();
    }

    Point getMinimumSize() const override {
        int count=0;
        for(const auto& item : containedWidgets) if(item.pWidget->isVisible()) ++count;
        return Point(110, std::max(1,(count+3)/4)*28-2);
    }

    void refresh() {
        unsigned visible=0, mask=0, bit=1;
        for(auto& item : containedWidgets) {
            if(item.pWidget->isVisible()) {
                mask |= bit;
                item.position=Point((visible%4)*28,(visible/4)*28);
                ++visible;
            }
            bit <<= 1;
        }
        if(mask != visibleMask) {
            visibleMask=mask;
            resize(std::max(110,getSize().x),getMinimumSize().y);
            resizeAll();
        }
    }
private:
    unsigned visibleMask=0;
};
#endif
