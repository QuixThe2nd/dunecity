// Exercise the actual input/menu command path in an isolated campaign.
#include <GUI/dune/InGameMenu.h>
#include <GUI/QstBox.h>
#include <ScreenBorder.h>
void runControlsProbe(Game& game) {
    auto require=[](bool ok,const char* why) { if(!ok)throw std::runtime_error(why); };
    auto commands=[&](CMDTYPE type) {
        int count=0;
        for(const auto& slot:game.cmdManager.timeslot)for(const auto& command:slot)
            if(command.getCommandID()==type)++count;
        return count;
    };
    Coord origin=Coord::Invalid();
    for(int y=4;y+8<currentGameMap->getSizeY() && origin.isInvalid();++y)
        for(int x=4;x+8<currentGameMap->getSizeX() && origin.isInvalid();++x) {
            bool clear=true;
            for(int dy=0;dy<8;++dy)for(int dx=0;dx<8;++dx)
                if(currentGameMap->getTile(x+dx,y+dy)->hasAGroundObject())clear=false;
            if(clear)origin=Coord(x,y);
        }
    require(origin.isValid(),"Missing input test space");
    auto* unit=pLocalHouse->createUnit(Unit_Tank);unit->deploy(origin);
    unit->setVisible(pLocalHouse->getTeamID(),true);
    for(int dy=0;dy<8;++dy)for(int dx=0;dx<8;++dx)
        currentGameMap->getTile(origin.x+dx,origin.y+dy)->setExplored(pLocalHouse->getTeamID(),true);
    screenborder->setNewScreenCenter(Coord((origin.x+2)*TILESIZE,(origin.y+2)*TILESIZE));
    auto select=[&]() { game.unselectAll(game.selectedList); game.selectedList={unit->getObjectID()};unit->setSelected(true); };
    auto point=[&](Coord tile) { return Coord(screenborder->world2screenX(tile.x*TILESIZE+TILESIZE/2),screenborder->world2screenY(tile.y*TILESIZE+TILESIZE/2)); };
    auto click=[&](Coord tile,int button,int drag=0) {
        const auto p=point(tile);
        require(screenborder->isScreenCoordInsideMap(p.x,p.y),"Input fixture outside map view");
        SDL_Event event{};event.type=SDL_MOUSEBUTTONDOWN;event.button.button=button;event.button.x=p.x;event.button.y=p.y;
        SDL_PushEvent(&event);event.type=SDL_MOUSEBUTTONUP;event.button.x+=drag;SDL_PushEvent(&event);game.doInput();
    };
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    settings.general.leftClickOrders=true; select();
    const int before=commands(CMD_UNIT_MOVE2POS);
    click(origin+Coord(3,2),SDL_BUTTON_LEFT);
    require(commands(CMD_UNIT_MOVE2POS)==before+1,"Left click failed to queue move");
    require(game.selectedList.count(unit->getObjectID()),"Order lost selection");
    click(origin+Coord(3,2),SDL_BUTTON_LEFT,24);
    require(commands(CMD_UNIT_MOVE2POS)==before+1,"Drag selection issued an order");
    select();SDL_SetModState(KMOD_SHIFT);click(origin+Coord(3,2),SDL_BUTTON_LEFT);SDL_SetModState(KMOD_NONE);
    require(commands(CMD_UNIT_MOVE2POS)==before+1,"Shift selection issued an order");
    select();click(origin,SDL_BUTTON_LEFT);
    require(commands(CMD_UNIT_MOVE2POS)==before+1 && game.selectedList.count(unit->getObjectID()),"Friendly click moved instead of selecting");
    click(origin+Coord(3,2),SDL_BUTTON_RIGHT);
    require(game.selectedList.empty() && commands(CMD_UNIT_MOVE2POS)==before+1,"Right click did not cancel selection");
    settings.general.leftClickOrders=false;select();click(origin+Coord(3,2),SDL_BUTTON_RIGHT);
    require(commands(CMD_UNIT_MOVE2POS)==before+2,"Default right-click order regressed");
    House* enemyHouse=nullptr;
    for(int h=0;h<NUM_HOUSES;++h)
        if(auto* candidate=game.getHouse(h); candidate && candidate->getTeamID()!=pLocalHouse->getTeamID()) { enemyHouse=candidate;break; }
    require(enemyHouse,"Missing enemy house fixture");
    auto* enemy=enemyHouse->createUnit(Unit_Tank);
    enemy->deploy(origin+Coord(4,3));enemy->setVisible(pLocalHouse->getTeamID(),true);
    settings.general.leftClickOrders=true;select();
    const auto attacks=commands(CMD_UNIT_ATTACKOBJECT);
    click(origin+Coord(4,3),SDL_BUTTON_LEFT);
    require(commands(CMD_UNIT_ATTACKOBJECT)==attacks+1,"Left click did not attack enemy");
    unit->setDestination(origin+Coord(3,2));
    unit->pathList={origin+Coord(1,0),origin+Coord(2,1),origin+Coord(3,2)};
    const SDL_Rect clip{10,30,200,180};SDL_RenderSetClipRect(renderer,&clip);
    SDL_SetRenderDrawColor(renderer,3,7,11,255);
    game.drawMovementPaths();
    SDL_Rect after{};SDL_RenderGetClipRect(renderer,&after);
    Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer,&r,&g,&b,&a);
    require(SDL_RectEquals(&clip,&after) && r==3 && g==7 && b==11 && a==255,"Paths changed scene render state");
    SDL_RenderSetClipRect(renderer,nullptr);
    settings.general.wasdCamera=true;select();
    SDL_KeyboardEvent key{};key.type=SDL_KEYDOWN;key.keysym.sym=SDLK_a;
    game.handleKeyInput(key);require(game.currentCursorMode==Game::CursorMode_Normal,"WASD A activated attack");
    key.keysym.mod=KMOD_SHIFT;game.handleKeyInput(key);
    require(game.currentCursorMode==Game::CursorMode_Attack,"Shift+A cannot attack with WASD enabled");
    game.setCursorMode(Game::CursorMode_Normal);
    require(game.canSkipMission(),"Fixture cannot skip campaign");
    game.onSkipMission();
    auto* menu=dynamic_cast<InGameMenu*>(game.pInGameMenu.get());require(menu,"Missing pause menu");menu->processChildWindowOpenCloses();
    auto* question=dynamic_cast<QstBox*>(menu->pChildWindow);
    require(question && question->button2.isActive() && commands(CMD_CAMPAIGN_SKIP)==0,"Skip was not safely confirmed");
    question->onButton(QSTBOX_BUTTON2);menu->processChildWindowOpenCloses();
    require(commands(CMD_CAMPAIGN_SKIP)==0,"Cancel skipped mission");
    menu->onSkipMission();menu->processChildWindowOpenCloses();
    question=dynamic_cast<QstBox*>(menu->pChildWindow);require(question,"Missing second confirmation");
    question->onButton(QSTBOX_BUTTON1);menu->processChildWindowOpenCloses();
    require(commands(CMD_CAMPAIGN_SKIP)==1,"Confirm did not queue exactly one skip");
    game.bReplay=true;game.confirmSkipMission();
    require(commands(CMD_CAMPAIGN_SKIP)==1,"Skip bypassed replay restriction");game.bReplay=false;
    SDL_Log("CONTROLS_PASS: left/default orders, drag/shift/friendly selection, cancel, WASD attack binding, confirmed skip");
    SDL_Log("CAMPAIGN_BALANCE_RESULT: probe=controls outcome=passed");
}
