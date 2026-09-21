#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace mgo2mt::notices {
inline constexpr uint16_t request_opcode=0x44f0,reply_opcode=0x44f1;
inline constexpr uint64_t poll_ms=15000,reply_timeout_ms=8000;
struct Media {std::string path,sha256;uint32_t size=0;bool operator==(const Media&)const=default;};
struct Alert {uint32_t id=0,version=0;uint64_t publishedAt=0,expiresAt=0;std::string text;std::optional<Media> audio,video;bool operator==(const Alert&)const=default;};
struct Snapshot {std::optional<Alert> alert;uint32_t mailCount=0,latestMailId=0;bool operator==(const Snapshot&)const=default;};
struct Reply {uint64_t nonce=0,serverTime=0;uint32_t character=0,status=0;std::string version;bool changed=false;std::optional<Snapshot> snapshot;};
bool valid_media(const Media&,bool video);
std::vector<uint8_t> request(uint64_t nonce,uint32_t character,std::optional<uint32_t> ping,std::string_view version);
Reply parse(std::span<const uint8_t>);
struct State {uint64_t scope=0,generation=0,serial=0,serverTime=0,receivedAt=0;uint32_t character=0;bool connected=false,ready=false,supported=false;std::string version;Snapshot snapshot;};
class Session {
 mutable std::mutex mutex_;State state_;uint64_t next_=0,deadline_=0,nonce_=0,sequence_=0,clock_=0;bool pending_=false,disabled_=false;
public:
 void connect(uint64_t scope,uint32_t character,uint64_t now);
 void disconnect();
 std::optional<std::vector<uint8_t>> take(uint64_t now,std::optional<uint32_t> pingMs);
 bool receive(std::span<const uint8_t>,uint64_t now);
 State state()const{std::lock_guard lock(mutex_);return state_;}
};
}
