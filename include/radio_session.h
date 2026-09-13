#pragma once
#include "native_radio.h"
#include <mutex>
namespace mgo2win::radio {
enum class DeliveryState {none,queued,awaiting_echo,confirmed,unconfirmed};
struct SessionState {
 uint64_t generation=0,epoch=0; Identity self; uint32_t life=0;
 Status status=Status::unavailable; bool eligible=false;
 DeliveryState delivery=DeliveryState::none;
};
// UI selection is scoped to the current admitted identity AND life. Only the
// room worker pumps the client; no UI action writes a socket or retries a send.
class Session {
 mutable std::mutex mutex_; Client client_; SessionState state_;
 std::optional<uint8_t> queued_; uint32_t flight_=0; uint64_t deadline_=0,lastSend_=0,lastNow_=0;
 bool everSent_=false; std::vector<Event> events_;
 void reset_locked();
public:
 void disconnect();
 SessionState state()const;
 bool submit(uint64_t generation,uint8_t preset,uint64_t now);
 std::vector<Body> pump(const Context&,Identity self,uint64_t now,uint64_t nonce,bool writable,
                       const std::vector<Body>& incoming,const Filter&);
 std::vector<Event> drain();
};
}
