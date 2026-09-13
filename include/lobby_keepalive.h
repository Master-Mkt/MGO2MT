#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>

namespace mgo2win {
// NomadLobby handles 0005 -> 0005/BE32 zero. The 30-second cadence is native
// policy, safely below ServerHandler's 120-second read timeout, not PS3 timing.
class LobbyKeepalive {
    uint64_t next_, deadline_=0;
    bool pending_=false;
public:
    static constexpr uint64_t interval_ms=30000, response_timeout_ms=8000;
    explicit LobbyKeepalive(uint64_t now):next_(now+interval_ms){}
    bool poll(uint64_t now){
        if(pending_){if(now>=deadline_)throw std::runtime_error("Lobby keepalive reply timeout");return false;}
        if(now<next_)return false;
        pending_=true;deadline_=now+response_timeout_ms;next_=now+interval_ms;return true;
    }
    bool receive(uint16_t command,std::span<const uint8_t> payload,uint64_t now){
        if(command!=0x0005)return false;
        if(!pending_||now>=deadline_||payload.size()!=4||payload[0]||payload[1]||payload[2]||payload[3])
            throw std::runtime_error("Unexpected lobby keepalive reply");
        pending_=false;return true;
    }
    bool pending()const{return pending_;}
};
}
