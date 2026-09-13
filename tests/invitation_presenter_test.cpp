#include "invitation_presenter.h"
#include "invitation_input_guard.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace mgo2win;
namespace {
void check(bool b,const char*why){if(!b)throw std::runtime_error(why);}
std::array<uint8_t,32> notification(uint32_t id,uint8_t kind=4){std::array<uint8_t,32>b{};b[1]=7;for(unsigned i=0;i<4;++i)b[2+i]=uint8_t(id>>(24-i*8));b[10]=1;b[11]=kind;const std::string name="日本の隊長";std::copy(name.begin(),name.end(),b.begin()+16);return b;}
std::array<uint8_t,9> ack(uint32_t id,uint8_t state,uint32_t error=0){std::array<uint8_t,9>b{};for(unsigned i=0;i<4;++i){b[i]=uint8_t(error>>(24-i*8));b[4+i]=uint8_t(id>>(24-i*8));}b[8]=state;return b;}
}
int main(){try{
 invitation_ui::InputGuard guard;
 check(!guard.stale(false)&&!guard.blocks_gameplay(false),"no invitation never blocks normal input");
 guard.painted(true);
 check(guard.blocks_gameplay(true)&&!guard.stale(true),"live modal owns gameplay but keeps its own decision route");
 check(!guard.left_button(true,true),"mouse down belongs to live modal");
 check(guard.stale(false)&&guard.stale(false)&&guard.blocks_gameplay(false),"non-input update cannot acknowledge expired modal still on screen");
 guard.painted(false);
 check(!guard.stale(false)&&!guard.blocks_gameplay(false),"actual closed screen paint releases generic input guard");
 check(guard.left_button(false,false),"modal mouse down then expiry and repaint still consumes late mouse up");
 check(!guard.left_button(true,false)&&!guard.left_button(false,false),"next fresh background click works");
 guard.painted(true);check(guard.left_button(true,false),"click on stale drawn dialog is consumed");
 guard.painted(false);check(guard.left_button(false,false),"stale dialog click remains owned across redraw");
 guard.painted(true);check(!guard.left_button(true,true)&&!guard.left_button(false,true),"normal live modal click reaches confirm once");
 guard.reset();check(!guard.stale(false)&&!guard.left_button(false,false),"full input reset clears ownership");
 auto session=std::make_shared<invitations::Session>(invitations::Policy{90000,5000,{4}});session->bind(10,100);invitation_ui::Presenter ui;ui.session(session);ui.update(100);check(!ui.overlay().available(),"empty real session has no fixture UI");
 check(session->receive_notification(10,notification(77),chat::Encoding::utf8,100),"offline notification accepted");ui.update(100);check(ui.overlay().available()&&!ui.overlay().visible()&&ui.view().marquee.find(L"日本の隊長")!=std::wstring::npos&&ui.view().marquee.find(L"Y / F6")!=std::wstring::npos,"authenticated notification becomes Japanese actionable view, no auto modal");
 check(ui.overlay().open(101),"explicit reply dialog");ui.overlay().choose(true);auto response=ui.overlay().confirm(101);check(response&&ui.respond(*response,101),"explicit choice submits through session queue");check(!ui.view().respondable&&ui.view().marquee.find(L"承諾しました")==std::wstring::npos,"queueing does not claim acceptance");
 auto payload=session->take(102);check(payload&&(*payload)[4]==2,"wire request waits for transport take");const auto began=ui.view().receivedAt;ui.update(102);check(ui.view().receivedAt==began&&ui.view().marquee.find(L"承諾しました")==std::wstring::npos,"sending does not restart feedback or claim ACK");
 check(!ui.respond(*response,102)&&!session->take(102),"no duplicate submit/take");check(!session->receive_answer(11,ack(77,2),103),"wrong connection ACK rejected");ui.update(103);check(ui.view().marquee.find(L"承諾しました")==std::wstring::npos,"wrong-scope ACK never becomes success text");
 check(session->receive_answer(10,ack(77,2),104),"matching offline ACK");ui.update(104);check(ui.view().marquee.find(L"承諾しました")!=std::wstring::npos&&ui.overlay().available()&&ui.view().receivedAt==104&&!ui.overlay().open(104),"acceptance text visible only after matching ACK despite prior confirmation, no further response");
 std::vector<uint32_t> pixels(1280*720);check(ui.overlay().paint(pixels,1280,720,2104)&&std::any_of(pixels.begin(),pixels.end(),[](auto p){return p!=0;}),"actual ACK feedback paints after same invitation was confirmed");ui.update(45104);check(!ui.overlay().available(),"terminal feedback has bounded lifetime");
 session->bind(11,100);ui.update(45105);check(!ui.overlay().available()&&ui.view().id==0,"same Session connection rebinding clears old scope UI");
 check(session->receive_notification(11,notification(88),chat::Encoding::utf8,45106),"second scope invite");ui.update(45106);check(!ui.respond({10,88,true},45106),"old scope response cannot enter current session");check(ui.overlay().open(45106),"new invitation explicit dialog");auto decline=ui.overlay().confirm(45106);check(decline&&!decline->accept&&ui.respond(*decline,45106),"default decline uses exact current scope");check(session->take(45107).has_value(),"decline reaches transport queue");ui.update(50107);check(ui.view().marquee.find(L"確認できませんでした")!=std::wstring::npos&&!ui.view().respondable,"timeout shows outcome unknown rather than accepted/declined");
 ui.session({});ui.update(50108);check(!ui.overlay().available(),"disconnect removes feedback");
 auto unsupported=std::make_shared<invitations::Session>(invitations::Policy{90000,5000,{4}});unsupported->bind(12,100);check(unsupported->receive_notification(12,notification(99,3),chat::Encoding::utf8,50109),"known-shape unsupported mode message");ui.session(unsupported);ui.update(50109);check(ui.overlay().available()&&!ui.overlay().open(50109)&&!ui.view().respondable,"unsupported mode announcement does not enable action");
 std::cout<<"Invitation Presenter notification -> Japanese UI -> explicit response -> queue/take -> ACK/unknown/scope PASS\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
