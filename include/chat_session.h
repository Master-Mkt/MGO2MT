#pragma once
#include "chat_wire.h"
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <vector>
namespace mgo2win::chat {
enum class Delivery {none,queued,awaiting_echo,echo_received,unconfirmed};
enum class Submit {accepted,not_joined,busy,unsupported_team,unknown_encoding,invalid_text,command_disabled};
// Explicit deployment knowledge belongs to the authenticated connection. It
// does not infer an encoding from Japanese UI text, room names or a packet.
enum class EndpointProfile {unknown,nomad_jp};
struct Member {uint32_t character=0;std::string name;uint8_t team=0;};
struct Line {uint64_t sequence=0,receivedAt=0;uint32_t character=0;uint8_t mode=0;std::string name,text;bool radio=false;};
struct State {
 uint64_t generation=0;uint32_t room=0,self=0;bool joined=false,teamSupported=false;
 std::optional<Encoding> encoding;std::vector<Line> lines;Delivery delivery=Delivery::none;
 uint64_t serial=0;std::string submittedText;
};
struct Send {uint64_t generation=0,serial=0;uint32_t room=0,self=0;std::string text;uint8_t mode=0;std::vector<uint8_t> payload;};
// One socket worker owns take/receive; UI owns submit. No implicit sends,
// retries, optimistic history rows, or cross-room pending work.
class Session {
 mutable std::mutex mutex_;State state_;std::map<uint32_t,Member> members_;
 std::optional<Send> queued_,flight_;uint64_t deadline_=0,lastSend_=0,lastNow_=0,sequence_=0;
 bool everSent_=false;
 uint64_t capNonce_=0,capDeadline_=0;bool capRequested_=false;
 std::optional<Encoding> endpointEncoding_;
 void expire(uint64_t now);
public:
 void connect(uint32_t self,EndpointProfile profile=EndpointProfile::unknown);
 void capabilities(Encoding,bool teamSupported);
 std::optional<std::vector<uint8_t>> request_capability(uint64_t nonce,uint64_t now);
 bool receive_capability(std::span<const uint8_t>,uint64_t now);
 void enter(uint32_t room);
 void roster(std::vector<Member>,bool joined);
 void display_name(uint32_t character,std::string name);
 void leave();void disconnect();
 State state()const;
 Submit submit(uint64_t generation,std::string text,bool team,uint64_t now);
 std::optional<Send> take(uint64_t now);
 bool receive(const Message&,uint64_t now);
 // A validated HOST radio event is distinct from a lobby chat echo.
 bool receive_radio(uint32_t character,std::string text,uint64_t now);
 void failed(uint64_t serial);void tick(uint64_t now);
};
}
