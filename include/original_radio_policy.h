#pragma once
#include <array>
#include <cstdint>

namespace mgo2::original_radio_policy {
// Current ELF B55368 + BB19A8, default catalogue IDs and third=2 only.
// Raw fields retain their addresses until their wider semantics are established.
struct Skill { uint8_t id=0, level=0; };
struct Peer {
 bool exists=false;
 uint8_t team=0, type12=0;
 uint32_t flags8=0;
};
struct DefaultReceive {
 uint8_t senderSlot=0, id=0, third=2, rule=1;
 Peer local, sender;
 bool gameExists=false;
 uint32_t suppressionMask168=0, gameFlags110=0;
 uint8_t localProfile294=0;
 std::array<Skill,5> senderSkills{};
 bool rule10Host=false;
 uint8_t rule10BypassSenderSlot=255; // Exact 26D568 result; not a native PC ID.
 bool senderActorPresent=false, senderVoiceKnown=false;
};
enum class Reason { allowed, unsupported, missing_peer, suppressed, spectator, different_team };
struct Decision {
 bool textAllowed=false;
 // Sound dispatch is eligible, not proof that its referenced asset exists.
 bool voiceCandidate=false;
 Reason reason=Reason::unsupported;
};
constexpr bool default_id(uint8_t id) { return id<=7 || (id>=9 && id<=16); }
constexpr uint8_t instructor_level(const DefaultReceive& s) {
 // B1CDA8: first matching slot wins, including a level-zero match.
 if(s.rule==10 || s.sender.type12<=6 || s.sender.type12==9) return 0;
 for(const auto& skill:s.senderSkills) if(skill.id==17) return skill.level;
 return 0;
}
constexpr Decision receive_default(const DefaultReceive& s) {
 if(s.senderSlot>=24 || !default_id(s.id) || s.third!=2) return {};
 if(!s.sender.exists) return {false,false,Reason::missing_peer};
 // B27C40 returns -1 without Game; suppression precedes every bypass.
 if(!s.gameExists || (s.suppressionMask168 & (uint32_t{1}<<s.senderSlot)))
  return {false,false,Reason::suppressed};
 const bool spectator=(s.local.exists && s.local.team==254) || (s.gameFlags110&0x40000);
 const bool profileBypass=(s.localProfile294&15)==3;
 bool bypass=false;
 if(s.rule==10) {
  if(s.rule10Host) bypass=true;
  else {
   if(spectator) return {false,false,Reason::spectator};
   bypass=profileBypass || s.senderSlot==s.rule10BypassSenderSlot;
  }
 } else if(s.local.exists && (s.local.flags8&0x200)) bypass=true;
 else {
  if(spectator) return {false,false,Reason::spectator};
  bypass=profileBypass || s.rule==0 || s.rule==12 || s.rule==15 || instructor_level(s)!=0;
 }
 if(!bypass && (!s.local.exists || s.local.team!=s.sender.team))
  return {false,false,Reason::different_team};
 // BB19A8 third2 requires both peers; absent actor/unknown voice returns 0,
 // so B55368 still queues the text. No health or round-phase filter occurs here.
 if(!s.local.exists) return {false,false,Reason::missing_peer};
 return {true,s.senderActorPresent&&s.senderVoiceKnown,Reason::allowed};
} 
} // namespace mgo2::original_radio_policy
