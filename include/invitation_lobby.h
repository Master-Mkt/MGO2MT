#pragma once
#include "tournament_invitation.h"
namespace mgo2win::invitations {
constexpr bool asynchronous(uint16_t opcode){return opcode==notification_opcode||opcode==answer_reply_opcode;}
// A malformed asynchronous notification is consumed, never returned as an RPC
// reply. The framed transport has already verified its sequence and MAC.
inline bool dispatch(Session& session,uint64_t scope,uint16_t opcode,std::span<const uint8_t> payload,chat::Encoding encoding,uint64_t now){
 if(opcode==notification_opcode){session.receive_notification(scope,payload,encoding,now);return true;}
 if(opcode==answer_reply_opcode){session.receive_answer(scope,payload,now);return true;}
 return false;
}
}
