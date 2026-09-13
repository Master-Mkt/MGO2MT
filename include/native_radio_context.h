#pragma once
#include "native_radio.h"
#include "host_session.h"
#include "combat_cycle.h"
namespace mgo2win::radio {
// Current native closed subset, not a reconstruction of all original radio
// exceptions: map20/rule1/flags0, active/alive/deployed, authoritative team1/2.
namespace detail { Context build(uint64_t,const host::LoadRequest*,const combat::Snapshot*,const combat::wire::Preparation*,std::span<const Identity>,bool); }
Context client_context(const host::Result&);
Context host_context(combat::Cycle&,std::span<const Identity> admitted,uint64_t now);
bool same_team(const Member& sender,const Member& recipient,uint8_t preset) noexcept;
}
