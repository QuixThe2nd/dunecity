#ifndef LATEJOINPOLICY_H
#define LATEJOINPOLICY_H
#include <Network/RoomAdmissionClient.h>
#include <sstream>
#include <set>
namespace LateJoinPolicy {
struct Request { std::string id, name; bool spectator = false; };
inline bool parseQueue(const std::string& body,std::vector<Request>& result) {
    if(body.empty() || body.size()>4096) return false;
    bool status=false, protocol=false;
    std::vector<Request> parsed;
    std::set<std::string> ids, names;
    std::istringstream input(body); std::string line;
    unsigned lines=0;
    while(std::getline(input,line)) {
        if(++lines>10 || line.size()>256) return false;
        if(line=="status=ok" && !status) status=true;
        else if(line=="protocol="+std::to_string(RoomRelay::kProtocolVersion) && !protocol) protocol=true;
        else if(line.compare(0,8,"request=")==0) {
            const auto split=line.find('|',8);
            if(split!=72 || parsed.size()>=8) return false;
            Request item{line.substr(8,64),{}};
            const auto roleAt=line.find('|',73);
            const auto role=roleAt==std::string::npos ? "player" : line.substr(roleAt+1);
            if(role!="player" && role!="spectator") return false;
            item.spectator=role=="spectator";
            if(!RoomRelay::isLowercaseHex(item.id) || !RoomAdmission::decodeHexText(line.substr(73,roleAt==std::string::npos ? roleAt : roleAt-73),64,item.name)
               || !RoomRelay::isAcceptableDisplayName(item.name) || !ids.insert(item.id).second || !names.insert(item.name).second) return false;
            parsed.push_back(std::move(item));
        } else return false;
    }
    if(!status || !protocol) return false;
    result=std::move(parsed); return true;
}
}
#endif
