#include "invitation_presenter.h"
#include <windows.h>
#include <algorithm>
#include <limits>

namespace mgo2win::invitation_ui {
namespace {
std::wstring wide(const std::string& s){
 if(s.empty()||s.size()>2048)return {};const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);if(n<=0)return {};
 std::wstring result(n,0);if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),result.data(),n)!=n)return {};return result;
}
unsigned phase(invitations::State s){using S=invitations::State;switch(s){case S::pending:return 1;case S::queued:case S::sending:return 2;case S::accepted:return 3;case S::declined:return 4;case S::rejected:return 5;case S::outcome_unknown:return 6;case S::expired:return 7;}return 0;}
uint64_t deadline(uint64_t now){return now>UINT64_MAX-45000?UINT64_MAX:now+45000;}
std::wstring message(const invitations::Entry&e,unsigned state){
 if(state==1){auto text=wide(invitations::marquee_utf8(e.notification));if(auto at=text.find(L"START");at!=text.npos)text.resize(at);
  text+=e.respondable?L"START メニューで Y / F6 を押して返答してください。":L"現在、この招待には返答できません。";return text;}
 auto name=wide(e.notification.name);if(name.empty())return {};
 switch(state){case 2:return name+L"さんへの招待の返答を送信しています。結果を確認するまでお待ちください。";
 case 3:return name+L"さんの招待を承諾しました。サーバーから返答を確認しました。";
 case 4:return name+L"さんの招待を辞退しました。サーバーから返答を確認しました。";
 case 5:return name+L"さんの招待への返答がサーバーで受け付けられませんでした。";
 case 6:return name+L"さんの招待への返答結果を確認できませんでした。自動では再送していません。";
 case 7:return name+L"さんの招待は期限切れ、または別の返答により終了しました。";default:return {};}
}
}
void Presenter::session(std::shared_ptr<invitations::Session> session){if(session_==session)return;session_=std::move(session);overlay_.clear();view_={};phase_=0;feedbackAt_=clock_=0;}
void Presenter::update(uint64_t now){
 clock_=(std::max)(clock_,now);now=clock_;
 if(!session_){overlay_.clear();view_={};phase_=0;return;}
 auto entries=session_->view(now);const invitations::Entry* selected=nullptr;
 // Preserve the single in-flight answer; otherwise expose the newest pending invitation.
 for(auto it=entries.rbegin();it!=entries.rend();++it)if(phase(it->state)==2){selected=&*it;break;}
 if(!selected)for(auto it=entries.rbegin();it!=entries.rend();++it)if(it->state==invitations::State::pending){selected=&*it;break;}
 if(!selected)for(auto&e:entries)if(e.scope==view_.scope&&e.notification.id==view_.id){selected=&e;break;}
 if(!selected&&!entries.empty())selected=&entries.back();
 if(!selected){overlay_.clear();view_={};phase_=0;return;}
 const unsigned nextPhase=phase(selected->state);const bool change=selected->scope!=view_.scope||selected->notification.id!=view_.id||nextPhase!=phase_;
 if(change){feedbackAt_=now;overlay_.clear();}phase_=nextPhase;
 view_.scope=selected->scope;view_.id=selected->notification.id;view_.marquee=message(*selected,nextPhase);
 view_.respondable=nextPhase==1&&selected->respondable;
 view_.receivedAt=nextPhase==1?selected->receivedAt:feedbackAt_;
 view_.expiresAt=nextPhase==1?selected->expiresAt:deadline(feedbackAt_);
 view_.available=!view_.marquee.empty()&&now<view_.expiresAt;
 overlay_.sync(view_,now);
}
bool Presenter::respond(Response response,uint64_t now){
 if(!session_||response.scope!=view_.scope||response.id!=view_.id||response.id>UINT32_MAX||!view_.respondable)return false;
 const bool submitted=session_->submit(response.scope,uint32_t(response.id),response.accept,now);
 if(!submitted)overlay_.clear();update(now);return submitted;
}
}
