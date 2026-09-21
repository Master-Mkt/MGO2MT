#pragma once
#include "chat_wire.h"
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <vector>
namespace mgo2mt::invitations {
inline constexpr uint16_t notification_opcode=0x49c1,answer_opcode=0x49c2,answer_reply_opcode=0x49c3;
struct Notification {
 uint16_t lobby=0;uint32_t id=0,serverTime=0,opaque=0;uint8_t state=0,kind=0;
 std::string name; // UTF-8 display text; never an authenticated character ID.
 bool operator==(const Notification&)const=default;
};
struct AnswerReply {uint32_t result=0,id=0;uint8_t state=0;};
// Candidate names are fixed 16 wire bytes. Do not widen the retail packet for
// Unicode extensions. The identifier is invitation ID only when state == 1;
// other notifications identify outgoing targets and must not cancel incoming IDs.
Notification parse_notification(std::span<const uint8_t>,chat::Encoding);
AnswerReply parse_answer_reply(std::span<const uint8_t>);
std::array<uint8_t,5> answer_payload(uint32_t id,bool accept);
std::string marquee_utf8(const Notification&);
enum class State {pending,queued,sending,accepted,declined,rejected,outcome_unknown,expired};
struct Entry {
 Notification notification;uint64_t scope=0,receivedAt=0,expiresAt=0;
 State state=State::pending;bool respondable=false;uint32_t result=0;
};
struct Policy {
 // Explicit native monotonic deadlines. serverTime is retained as an opaque
 // original timestamp/age field; it is not assumed to be local Unix time.
 uint64_t ttlMs=90000,answerTimeoutMs=5000;
 std::vector<uint8_t> respondableKinds;
 // Reply carries no kind. A caller may explicitly enable answering every
 // well-formed, authenticated incoming state1 notification. This does not
 // authorize generating invitations; this server currently generates kind4 only.
 bool allowReceivedKinds=false;
};
class Session {
 mutable std::mutex mutex_;Policy policy_;uint64_t scope_=0,clock_=0,pendingAt_=0;
 uint32_t self_=0,pendingId_=0;bool pendingAccept_=false;
 std::vector<Entry> entries_;std::set<uint32_t> seen_;
 void tick_locked(uint64_t now);
 std::optional<std::array<uint8_t,5>> prepare_locked(uint64_t scope,uint32_t id,bool accept,uint64_t now,bool queued);
public:
 explicit Session(Policy);
 void bind(uint64_t connectionScope,uint32_t selfCharacter);
 bool receive_notification(uint64_t scope,std::span<const uint8_t>,chat::Encoding,uint64_t nowMs);
 bool receive_answer(uint64_t scope,std::span<const uint8_t>,uint64_t nowMs);
 std::optional<std::array<uint8_t,5>> answer(uint64_t scope,uint32_t id,bool accept,uint64_t nowMs);
 bool submit(uint64_t scope,uint32_t id,bool accept,uint64_t nowMs);
 std::optional<std::array<uint8_t,5>> take(uint64_t nowMs);
 std::vector<Entry> view(uint64_t nowMs);
 void tick(uint64_t nowMs);
 // Socket/queue uncertainty is terminal for this attempt. No automatic retry,
 // navigation or join operation is emitted, including after a successful ACK.
 void submission_unknown(uint64_t scope,uint32_t id);
};
}
