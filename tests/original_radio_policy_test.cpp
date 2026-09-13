#include "original_radio_policy.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace mgo2::original_radio_policy;
int main(){
 DefaultReceive s; s.senderSlot=4;s.gameExists=true;
 s.local={true,0,7,0};s.sender={true,0,7,0};
 for(unsigned i=0;i<256;++i){s.id=uint8_t(i);assert(receive_default(s).textAllowed==default_id(s.id));}
 s.id=0;
 assert(receive_default(s).textAllowed&&!receive_default(s).voiceCandidate);
 s.senderActorPresent=true;assert(!receive_default(s).voiceCandidate);
 s.senderVoiceKnown=true;assert(receive_default(s).voiceCandidate);
 s.sender.team=1;assert(receive_default(s).reason==Reason::different_team);
 for(auto rule:{0,12,15}){s.rule=uint8_t(rule);assert(receive_default(s).textAllowed);}
 s.rule=1;s.localProfile294=0xA3;assert(receive_default(s).textAllowed);
 s.localProfile294=0;s.senderSkills[2]={17,1};assert(receive_default(s).textAllowed);
 for(auto type:{0,1,2,3,4,5,6,9}){s.sender.type12=uint8_t(type);assert(!receive_default(s).textAllowed);}
 s.sender.type12=7;s.senderSkills[0]={17,0};assert(!receive_default(s).textAllowed);
 s.senderSkills[0]={0,0};s.local.team=254;assert(receive_default(s).reason==Reason::spectator);
 s.local.flags8=0x200;assert(receive_default(s).textAllowed);
 s.suppressionMask168=1u<<4;assert(receive_default(s).reason==Reason::suppressed);
 s.suppressionMask168=1u<<5;assert(receive_default(s).textAllowed);
 s.gameExists=false;assert(receive_default(s).reason==Reason::suppressed);s.gameExists=true;
 s.local.flags8=0;s.local.team=0;s.gameFlags110=0x40000;
 assert(receive_default(s).reason==Reason::spectator);s.gameFlags110=0;
 s.rule=10;assert(receive_default(s).reason==Reason::different_team);
 s.rule10BypassSenderSlot=4;assert(receive_default(s).textAllowed);
 s.local.team=254;assert(!receive_default(s).textAllowed);
 s.rule10Host=true;assert(receive_default(s).textAllowed);
 s.local.exists=false;assert(receive_default(s).reason==Reason::missing_peer);
 s.local.exists=true;s.sender.exists=false;assert(receive_default(s).reason==Reason::missing_peer);
 s.sender.exists=true;s.senderSlot=24;assert(receive_default(s).reason==Reason::unsupported);
 s.senderSlot=4;s.third=1;assert(receive_default(s).reason==Reason::unsupported);
 s.third=3;assert(receive_default(s).reason==Reason::unsupported);
 std::cout<<"Original default radio receive policy: PASS\n";
}
