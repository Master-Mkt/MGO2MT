#include "character_client.h"
#include "invitation_lobby.h"
#include <iostream>
#include <stdexcept>
using namespace mgo2mt;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
 NetworkKeys keys;for(unsigned i=0;i<1042;++i)keys.packet[i]=uint32_t(i*0x1234567u);keys.hmac.fill(0x3a);keys.wire={1,3,5,7};
 RoomRequests rooms;auto& session=*rooms.invitationSession;session.bind(11,7);
 std::vector<uint8_t> notice(32);notice[1]=6;notice[5]=99;notice[10]=1;notice[11]=5;
 const std::string japanese="日本語招待";std::copy(japanese.begin(),japanese.end(),notice.begin()+16);
 std::vector<LobbyPacket> packets{{0x49c1,1,notice},{0x49c1,2,{1,2}},{0x4301,3,{0,0,0,0}}};
 unsigned rpc=0;for(const auto& p:packets){auto encoded=encode_lobby(keys,p.command,p.sequence,p.payload);auto decoded=decode_lobby(keys,encoded,p.sequence);
  if(!invitations::asynchronous(decoded.command)){++rpc;check(decoded.command==0x4301,"async packet leaked into RPC response");continue;}
  check(invitations::dispatch(session,11,decoded.command,decoded.payload,chat::Encoding::utf8,100),"known async packet not consumed");
 }
 check(rpc==1&&session.view(100).size()==1&&session.view(100)[0].notification.name==japanese,"authenticated interleaved notification");
 check(!invitations::dispatch(session,11,0x4301,{},chat::Encoding::utf8,100),"unrelated RPC consumed");
 // Enter/leave a room must not erase a lobby invitation or emit a reply.
 RoomAction join;join.event=RoomEvent::join;join.id=5;check(rooms.submit(join),"room action");rooms.clear();rooms.clear_combat();
 check(session.view(101).size()==1&&!session.take(101),"room lifecycle changed invitation");
 check(session.submit(11,99,true,102),"explicit response not queued");auto answer=session.take(102);check(answer.has_value(),"network reply not drained");
 auto wire=encode_lobby(keys,invitations::answer_opcode,4,*answer);auto decoded=decode_lobby(keys,wire,4);
 check(decoded.command==0x49c2&&decoded.payload==std::vector<uint8_t>({0,0,0,99,2}),"exact original affirmative reply");
 std::vector<uint8_t> ack{0,0,0,0,0,0,0,99,2};decoded=decode_lobby(keys,encode_lobby(keys,0x49c3,5,ack),5);
 check(invitations::dispatch(session,11,decoded.command,decoded.payload,chat::Encoding::utf8,103)&&session.view(103)[0].state==invitations::State::accepted,"correlated ACK missing");
 auto corrupted=encode_lobby(keys,0x49c1,6,notice);corrupted[9]^=1;bool rejected=false;try{decode_lobby(keys,corrupted,6);}catch(...){rejected=true;}check(rejected,"modified MAC admitted");
 session.bind(0,0);check(invitations::dispatch(session,11,0x49c1,notice,chat::Encoding::utf8,104)&&session.view(104).empty(),"stale connection resurrected invitation");
 std::cout<<"Offline framed lobby invite/RPC interleave, malformed consumption, room persistence, ACK and disconnect passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
