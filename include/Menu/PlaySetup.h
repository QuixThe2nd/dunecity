#ifndef PLAYSETUP_H
#define PLAYSETUP_H

#include <GameInitSettings.h>
#include <Network/ChangeEventList.h>
#include <mod/ModInfo.h>
#include <string>
#include <vector>

// Local setup only: no connection is opened until the player chooses Create Lobby.
struct CustomPlaySetup {
    std::vector<std::string> maps;
    std::vector<ModInfo> mods;
    int map = 0;
    int mapCategory = 4;
    int mod = 0;
    bool online = false;
    bool publicGame = false;
    bool sharedHouse = false;
    SettingsClass::GameOptionsClass rules;
    ChangeEventList players;
};

constexpr int MENU_SETUP_CHANGED = -20;
constexpr int MENU_SETUP_HOST = -21;
constexpr int MENU_SETUP_MAP = -22;
constexpr int MENU_SETUP_PLAYERS = -23;
void playCustomGame(bool online = false);
void showGameLibrary(bool replays = false);
bool hasRecentGame();
void continueRecentGame();

#endif
