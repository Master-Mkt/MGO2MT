#pragma once
#include "notification_wire.h"
#include "server_notifications.h"
#include "tournament_invitation.h"
namespace mgo2mt::notifications {
// Database publication uses server UTC anchored to the receive monotonic clock;
// retail invitation expiry is a separate, already-reviewed monotonic deadline.
inline uint64_t server_now(const notices::State& state,uint64_t now,uint64_t fallback){
 return state.ready&&now>=state.receivedAt?state.serverTime+now-state.receivedAt:fallback;
}
inline View update_session(Presentation& p,const notices::State& state,std::span<const invitations::Entry> entries,uint64_t now,uint64_t fallbackUnix){
 if(!state.connected){p.reset();return p.view();}
 const auto utc=server_now(state,now,fallbackUnix);std::vector<Notice> notices;
 if(state.ready&&state.snapshot.mailCount)notices.push_back({Kind::mail,state.snapshot.latestMailId});
 for(const auto& entry:entries){
  if(entry.scope!=state.scope||entry.notification.state!=1||!entry.notification.id)continue;
  const bool active=(entry.state==invitations::State::pending||entry.state==invitations::State::queued||entry.state==invitations::State::sending)&&entry.expiresAt>now;
  notices.push_back({entry.notification.kind==4?Kind::survival:Kind::tournament,entry.notification.id,active?utc+(entry.expiresAt-now):utc,active});
 }
 std::optional<Alert> alert;if(state.ready&&state.snapshot.alert){const auto&a=*state.snapshot.alert;alert=Alert{a.id,a.version,a.publishedAt,a.expiresAt,a.text};}
 return p.update({state.scope,state.generation,state.character},notices,alert,utc,now,{state.ready,true,true});
}
}
