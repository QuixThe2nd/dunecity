// Integration-only main: real service, WebRTC, saved game and simulation in three processes.
#include <Game.h>
#include <House.h>
#include <GameInitSettings.h>
#include <Network/NetworkManager.h>
#include <Network/RoomAdmissionClient.h>
#include <Menu/CrossplayMenu.h>
#include <players/HumanPlayer.h>
#include <units/UnitBase.h>
#include <misc/OMemoryStream.h>
#include <misc/IMemoryStream.h>
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

static std::string joinRead(const std::string& path) { std::ifstream f(path,std::ios::binary); return std::string(std::istreambuf_iterator<char>(f),{}); }
static void joinWrite(const std::string& path,const std::string& bytes) { std::ofstream f(path,std::ios::binary); f<<bytes; }
int runLateJoinProbe() {
    const std::string role=std::getenv("JOIN_ROLE"), out=std::getenv("JOIN_OUT"), endpoint=std::getenv("JOIN_ENDPOINT");
    const std::string mode=std::getenv("JOIN_MODE") ? std::getenv("JOIN_MODE") : "replace";
    const bool host=role=="Host", newcomer=role=="Newcomer";
    const bool observer=mode=="spectate" || mode=="reject_spectate";
    const Uint32 verifyCycle=observer ? 1800 : 150;
    settings.general.playerName=role;
    settings.network.metaServer="http://127.0.0.1:1/";
    const Uint32 deadline=SDL_GetTicks()+(std::getenv("JOIN_BROWSER") ? 600000 : 160000);
    auto wait=[&](auto condition) { while(!condition()) { if(SDL_TICKS_PASSED(SDL_GetTicks(),deadline)) throw std::runtime_error("probe wait timeout"); SDL_Delay(5); } };
    AdmissionRequest request;
    request.baseUrl=endpoint; request.allowLoopbackPlaintext=true; request.appVersion=VERSIONSTRING;
    request.gameProtocol=NETWORK_PROTOCOL_VERSION; request.runtime="native"; request.contentHash=CrossplayMenu::contentFingerprint();
    request.hosting=host; request.publicRoom=true; request.maxPeers=4; request.mode="custom"; request.allowLateJoin=true;
    request.modName="vanilla"; request.mapName="Join probe";
    if(!host) { wait([&](){return std::filesystem::exists(out+"/room");}); request.roomCode=joinRead(out+"/room"); }
    if(newcomer) {
        wait([&](){return std::filesystem::exists(out+"/running");});
        request.operation=AdmissionOperation::JoinRequest; request.displayName=role; request.publicOnly=true; request.spectate=mode=="spectate";
    }
    RoomAdmissionClient admission;
    auto ask=[&]() { admission.begin(request); wait([&](){admission.update(); return admission.status()!=RoomAdmissionClient::Status::InProgress;});
        if(admission.status()!=RoomAdmissionClient::Status::Succeeded) throw std::runtime_error(admission.errorMessage()); return admission.response(); };
    auto admitted=ask();
    if(host) joinWrite(out+"/room",admitted.roomCode);
    if(newcomer) {
        request.operation=AdmissionOperation::JoinStatus; request.requestTicket=admitted.requestTicket;
        do { SDL_Delay(3000); admitted=ask(); } while(admitted.requestState=="pending");
        if(admitted.requestState!="approved") throw std::runtime_error("request not approved");
    }
    pNetworkManager=std::make_unique<NetworkManager>(NetworkManager::Transport::DirectP2P);
    auto& network=*pNetworkManager;
    DirectRoomTransport::Config config;
    config.signalingBaseUrl=endpoint; config.allowLoopbackPlaintext=true; config.grant=admitted.grant;
    config.roomCode=admitted.roomCode; config.displayName=role; config.appVersion=VERSIONSTRING;
    config.contentHash=request.contentHash; config.runtime="native"; config.gameProtocolVersion=NETWORK_PROTOCOL_VERSION;
    config.allowLateJoin=host;
    std::string error; if(!network.startDirectSession(config,error)) throw std::runtime_error(error);
    network.setOnPeerDisconnected([&](const std::string&,bool,int){throw std::runtime_error("probe disconnected: "+network.getRelayClient()->statusMessage());});
    bool cancelled=false;
    if(newcomer && (mode=="abort" || std::getenv("JOIN_STALL"))) network.setOnPeerDisconnected([&](const std::string&,bool,int){cancelled=true;});
    bool start=false;
    network.setOnStartGame([&](unsigned){start=true;});
    if(newcomer) network.expectLateJoin();
    GameInitSettings init;
    if(host) {
        auto map=joinRead(PROBE_MAP_PATH);
        if(std::getenv("JOIN_BUSY")) {
            map+="\n";
            for(int i=0;i<100;++i) map+="ID"+std::to_string(100+i)+"=Player"+std::to_string(i<50 ? 1 : 2)
                +",Tank,256,"+std::to_string((i%50)*29+10)+",64,Area Guard\n";
        }
        init=GameInitSettings("join.ini",map,"Host",true,settings.gameOptions);
        GameInitSettings::HouseInfo humans(HOUSE_ATREIDES,1); humans.addPlayerInfo({"Host",HUMANPLAYERCLASS}); if(mode!="share_human") humans.addPlayerInfo({"Partner",HUMANPLAYERCLASS}); init.addHouseInfo(humans);
        GameInitSettings::HouseInfo enemy(HOUSE_HARKONNEN,2); enemy.addPlayerInfo({mode=="share_human" ? "Partner" : "Enemy AI",mode=="share_human" ? HUMANPLAYERCLASS : "qBotEasy"}); init.addHouseInfo(enemy);
        // Retain a real waiting-lobby setup/callback, as the production menu does.
        // Newcomers during a match must never invoke lobby seat assignment.
        network.setGetChangeEventListForNewPlayerCallback([](const std::string& name) {
            if(name=="Newcomer") throw std::runtime_error("late join reopened waiting-lobby assignment");
            return ChangeEventList();
        });
        network.startServer(false,"Join probe",role,&init,2,4);
        OMemoryStream bytes; bytes.open(); init.save(bytes); joinWrite(out+"/init",std::string(bytes.getData(),bytes.getDataLength()));
    } else if(!newcomer) {
        wait([&](){network.update();return std::filesystem::exists(out+"/init");});
        const auto bytes=joinRead(out+"/init"); IMemoryStream in(bytes.data(),bytes.size()); init=GameInitSettings(in);
    }
    if(!newcomer) {
        bool sent=false;
        wait([&](){ network.update(); if(host && !sent && network.getConnectedPeers().size()==1 && network.isMeshReady()) {sent=network.sendStartGame(0); if(!sent) throw std::runtime_error("start refused");} return start; });
    } else {
        wait([&](){ network.update(); return network.lateJoinReady() || cancelled; });
        if(cancelled && mode=="abort") { joinWrite(out+"/Newcomer-cancelled","yes"); network.setOnPeerDisconnected({}); pNetworkManager.reset(); return MENU_QUIT_DEFAULT; }
        if(observer) {
            joinWrite(out+"/observer-delaying",std::to_string(network.observerFrontier()));
            SDL_Delay(std::getenv("JOIN_STALL") ? 40000 : 5000); // Deliberately stop reading: the players must keep simulating.
            if(!std::filesystem::exists(out+"/Host-advanced") || !std::filesystem::exists(out+"/Partner-advanced")) throw std::runtime_error("players waited for spectator loading");
        }
        if(observer && std::getenv("JOIN_STALL")) {
            wait([&](){network.update(); return cancelled || !network.getRelayClient()->isJoined();});
            joinWrite(out+"/Newcomer-left","timed out without blocking players");
            network.setOnPeerDisconnected({}); pNetworkManager.reset();
            SDL_Log("LATE_JOIN_PROBE_PASS: unresponsive spectator isolated"); return MENU_QUIT_DEFAULT;
        }
        init=*network.takeLateJoin();
    }
    auto game=std::make_unique<Game>(); currentGame=game.get(); game->initGame(init); game->initializeGameLoop();
    if(newcomer) joinWrite(out+"/Newcomer-loaded","yes");
    bool requested=false, resumed=newcomer || observer, rejected=false;
    Uint32 lastOrdersCycle=~Uint32(0);
    const bool checkDeparture=observer && !std::getenv("JOIN_BROWSER");
    while(!SDL_TICKS_PASSED(SDL_GetTicks(),deadline)) {
        if(std::filesystem::exists(out+"/done")) break;
        const Uint32 target=checkDeparture && !newcomer && std::filesystem::exists(out+"/Newcomer-left") ? verifyCycle+100 : verifyCycle;
        const bool waiting=game->handleNetworkUpdates();
        if(observer && !newcomer && (network.lateJoinPaused() || network.lateJoinReady())) throw std::runtime_error("spectator paused a player");
        if(observer && !newcomer && std::filesystem::exists(out+"/observer-delaying") && game->getGameCycleCount()>std::stoul(joinRead(out+"/observer-delaying"))+100) joinWrite(out+"/"+role+"-advanced",std::to_string(game->getGameCycleCount()));
        if(network.lateJoinReady()) {
            auto snapshot=network.takeLateJoin(); game.reset(); currentGame=nullptr;
            game=std::make_unique<Game>(); currentGame=game.get(); game->initGame(*snapshot); game->initializeGameLoop(); resumed=true;
            joinWrite(out+"/"+role+"-loaded","yes");
        }
        if(host && !requested && game->getGameCycleCount()>=60) {
            joinWrite(out+"/running","yes");
            const auto& requests=network.getDirectTransport()->joinRequests();
            if(!requests.empty() && observer) {
                if(mode=="reject_spectate" && !rejected) {
                    if(!network.getDirectTransport()->manageJoin("decline",requests[0].id)) throw std::runtime_error("decline failed");
                    rejected=true;
                }
                // Game::handleNetworkUpdates automatically synchronizes spectator requests.
            } else if(!requests.empty()) {
                for(const auto& slot : game->availableJoinSlots()) if(slot.house==(mode=="share_human" ? HOUSE_ATREIDES : HOUSE_HARKONNEN) && slot.controller==((mode=="replace" || mode=="abort") ? 0 : 1)) {
                    if(!game->acceptJoinRequest(requests[0].id,requests[0].name,slot)) throw std::runtime_error("accept failed: "+network.lateJoinStatus());
                    requested=true; break;
                }
            }
        }
        if(host && mode=="abort" && network.joinStage==NetworkManager::JoinStage::Sending) {
            network.abortLateJoin("Test cancellation"); joinWrite(out+"/aborted","yes"); resumed=true;
        }
        if(!host && !newcomer && mode=="abort" && std::filesystem::exists(out+"/aborted") && !network.lateJoinPaused()) resumed=true;
        if(!network.lateJoinPaused()) {
            if(host && std::getenv("JOIN_BUSY") && !waiting && game->getGameCycleCount()!=lastOrdersCycle && game->getGameCycleCount()%150==0) {
                lastOrdersCycle=game->getGameCycleCount();
                const auto player=game->getPlayerByName("Host")->getPlayerID();
                for(auto* unit : unitList) if(unit->getOwner()->getHouseID()==HOUSE_ATREIDES && unit->getItemID()==Unit_Tank)
                    game->cmdManager.addCommand(Command(player,CMD_UNIT_MOVE2POS,unit->getObjectID(),
                        Uint32((game->getGameCycleCount()/150)%2 ? 42 : 8),Uint32(15),Uint32(1)));
            }
            game->cmdManager.update();
            if(!waiting && game->getGameCycleCount()<target && (resumed || game->getGameCycleCount()<65)) game->updateGameState();
        }
        if(checkDeparture && !newcomer && game->getGameCycleCount()==verifyCycle+100) {
            if(!network.getRelayClient()->isJoined() || network.getConnectedPeers().size()!=1) throw std::runtime_error("spectator departure damaged the match");
            if(!std::filesystem::exists(out+"/"+role+"-after-leave")) joinWrite(out+"/"+role+"-after-leave",GameStateDigest::describe(game->computeStateDigest()));
            if(std::filesystem::exists(out+"/done")) break;
        }
        if(resumed && game->getGameCycleCount()==verifyCycle) {
            if(!game->getPlayerByName("Host") || !game->getPlayerByName("Partner") || (mode!="abort" && !observer && !game->getPlayerByName("Newcomer")) || (mode=="replace" && game->getPlayerByName("Enemy AI")) || (mode=="share_ai" && !game->getPlayerByName("Enemy AI"))) throw std::runtime_error("controller roster wrong");
            if(observer) {
                if(game->getPlayerByName("Newcomer") || !game->getPlayerByName("Enemy AI") || (newcomer && !network.isSpectating())) throw std::runtime_error("spectator took a slot");
                if(newcomer) {
                    const auto before=game->cmdManager.timeslot.size();
                    game->cmdManager.addCommand(Command(game->getPlayerByName("Host")->getPlayerID(),CMD_PLAYER_PAUSE));
                    if(game->cmdManager.timeslot.size()!=before) throw std::runtime_error("spectator issued a local command");
                }
            }
            if(mode=="abort" && (!network.getRelayClient()->isJoined() || !game->getPlayerByName("Enemy AI") || game->getPlayerByName("Newcomer"))) throw std::runtime_error("abort changed the original match");
            if(!std::filesystem::exists(out+"/"+role+"-digest")) joinWrite(out+"/"+role+"-digest",GameStateDigest::describe(game->computeStateDigest()));
            if(checkDeparture && newcomer && std::filesystem::exists(out+"/Host-digest") && std::filesystem::exists(out+"/Partner-digest")) {
                network.disconnect(); joinWrite(out+"/Newcomer-left","yes"); break;
            }
            if(std::filesystem::exists(out+"/done")) break;
        }
        SDL_Delay(16);
    }
    if(!std::filesystem::exists(out+"/"+role+"-digest")) throw std::runtime_error("simulation did not resume: "+network.lateJoinStatus());
    network.setOnPeerDisconnected({}); game.reset(); currentGame=nullptr; pNetworkManager.reset();
    SDL_Log("LATE_JOIN_PROBE_PASS: %s",role.c_str()); return MENU_QUIT_DEFAULT;
}
