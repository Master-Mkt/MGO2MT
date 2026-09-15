#include "menu_font.h"
#include "server_notifications.h"
#include <windows.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace mgo2win::notifications {
namespace {
bool valid(Scope s){return s.connection&&s.generation&&s.character;}
bool valid(Kind k){return unsigned(k)<3;}
bool live(uint64_t expiry,uint64_t now){return !expiry||now<expiry;}
bool rect_valid(Rect r){return r.left>=0&&r.top>=0&&r.right>=r.left&&r.bottom>=r.top&&r.right<=1280&&r.bottom<=720;}
void over(uint32_t& d,uint32_t rgb,unsigned a){
 if(!a)return;const unsigned da=d>>24,oa=a+(da*(255-a)+127)/255;uint32_t out=oa<<24;
 for(unsigned shift:{0u,8u,16u})out|=((((rgb>>shift)&255)*a+((d>>shift)&255)*da*(255-a)/255+oa/2)/oa)<<shift;
 d=out;
}
}
std::optional<std::wstring> news_text(std::string_view bytes){
 if(bytes.empty()||bytes.size()>4096)return {};
 const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0);if(count<=0)return {};
 std::wstring out(size_t(count),L'\0');if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),out.data(),count)!=count)return {};
 for(auto& c:out){if(c==L'\r'||c==L'\n'||c==L'\t'){c=L' ';continue;}
  if(c<32||(c>=127&&c<=159)||(c>=0x2028&&c<=0x202e)||(c>=0x2066&&c<=0x2069)||c==0xfeff)return {};
 }return out;
}
void Presentation::reset(){scope_.reset();seen_.clear();acknowledged_.clear();pingPending_.clear();active_.clear();view_={};alert_.reset();now_=0;initializedKinds_={};mailHighwater_=0;soundSaturated_=false;}
bool Presentation::acknowledge(Scope s,Kind kind,uint64_t id){
 const Key key{kind,id};if(!scope_||s!=*scope_||!valid(kind)||!id||!seen_.contains(key)||acknowledged_.size()>=maximumSeen)return false;
 acknowledged_.insert(key);active_.erase(key);pingPending_.erase(key);view_.badges[unsigned(kind)]=std::any_of(active_.begin(),active_.end(),[&](const auto& x){return x.first.first==kind;});return true;
}
View Presentation::update(Scope s,std::span<const Notice> notices,std::optional<Alert> next,uint64_t unixNow,uint64_t now,std::array<bool,3> ready){
 if(!valid(s)||notices.size()>maximumNotices){reset();return view_;}
 std::set<Key> unique;for(const auto& n:notices)if(!valid(n.kind)||!n.id||!unique.insert({n.kind,n.id}).second){reset();return view_;}
 if(!scope_||s!=*scope_||now<now_){reset();scope_=s;}
 now_=now;view_.scope=s;view_.bright=(now%1000)<500;
 std::erase_if(active_,[&](const auto& row){return ready[unsigned(row.first.first)]||!live(row.second,unixNow);});
 for(const auto& n:notices){const auto kind=unsigned(n.kind);if(!ready[kind])continue;const Key key{n.kind,n.id};
  const bool arrived=!seen_.contains(key),active=n.active&&live(n.expiresUnixMs,unixNow);
  const bool newerMail=n.kind!=Kind::mail||n.id>mailHighwater_;
  // Native aggregate-mail policy: falling back to an older unread ID after a
  // read is a badge update, not a new arrival. No server mutation is implied.
  if(n.kind==Kind::mail)mailHighwater_=(std::max)(mailHighwater_,n.id);
  if(arrived){if(seen_.size()<maximumSeen){seen_.insert(key);if(active&&newerMail&&initializedKinds_[kind]&&!soundSaturated_)pingPending_.insert(key);}else soundSaturated_=true;}
  if(active&&!acknowledged_.contains(key)&&active_.size()<maximumNotices)active_[key]=n.expiresUnixMs;
 }
 for(unsigned kind=0;kind<3;++kind)if(ready[kind])initializedKinds_[kind]=true;
 view_.badges={};for(const auto& row:active_)view_.badges[unsigned(row.first.first)]=true;
 std::erase_if(pingPending_,[&](const auto& key){return !active_.contains(key);});
 if(!next){alert_.reset();view_.news.reset();}
 else if(auto text=news_text(next->text);text&&next->id&&next->version&&next->publishedUnixMs<=unixNow&&live(next->expiresUnixMs,unixNow)&&(!next->expiresUnixMs||next->expiresUnixMs>next->publishedUnixMs)){
  const bool older=alert_&&(next->publishedUnixMs<alert_->publishedUnixMs||(next->id==alert_->id&&next->version<alert_->version));
  const bool rewrite=alert_&&next->id==alert_->id&&next->version==alert_->version&&*next!=*alert_;
  if(!older&&!rewrite){if(!alert_||next->id!=alert_->id||next->version!=alert_->version){view_.news=News{next->id,next->version,now,std::move(*text)};}alert_=std::move(next);}
 }else{alert_.reset();view_.news.reset();}
 if(alert_&&!live(alert_->expiresUnixMs,unixNow)){alert_.reset();view_.news.reset();}
 return view_;
}
struct Renderer::Impl {
 HDC dc=nullptr;HBITMAP bitmap=nullptr;HFONT font=nullptr;HGDIOBJ oldBitmap=nullptr,oldFont=nullptr;uint32_t* pixels=nullptr;
 Impl(){dc=CreateCompatibleDC(nullptr);BITMAPINFO b{};b.bmiHeader.biSize=40;b.bmiHeader.biWidth=1280;b.bmiHeader.biHeight=-720;b.bmiHeader.biPlanes=1;b.bmiHeader.biBitCount=32;
  if(dc)bitmap=CreateDIBSection(dc,&b,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
  font=create_menu_font(21,FW_NORMAL);
  if(!dc||!bitmap||!font||!pixels){release();throw std::runtime_error("Notification GDI allocation");}oldBitmap=SelectObject(dc,bitmap);oldFont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
 }
 void release(){if(dc&&oldFont)SelectObject(dc,oldFont);if(dc&&oldBitmap)SelectObject(dc,oldBitmap);if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);}
 ~Impl(){release();}
};
Renderer::Renderer()=default;Renderer::~Renderer()=default;
bool Renderer::paint(std::span<uint32_t> dst,int width,int height,const View& view,uint64_t now,PaintLayout layout){
 if(width!=1280||height!=720||dst.size()!=size_t(width)*height||!valid(view.scope)||layout.iconRight<116||layout.iconRight>1280||layout.iconTop<0||layout.iconTop>688||layout.newsLeft<0||layout.newsRight>1280||layout.newsRight-layout.newsLeft<80||layout.newsTop<0||layout.newsTop>692||(layout.excluded&&!rect_valid(*layout.excluded)))return false;
 bool painted=false;auto pixel=[&](int x,int y,uint32_t color,unsigned a=255){if(x<0||y<0||x>=width||y>=height)return;if(layout.excluded&&x>=layout.excluded->left&&x<layout.excluded->right&&y>=layout.excluded->top&&y<layout.excluded->bottom)return;over(dst[size_t(y)*width+x],color,a);painted=true;};
 auto rect=[&](int x,int y,int w,int h,uint32_t color){for(int row=y;row<y+h;++row)for(int col=x;col<x+w;++col)pixel(col,row,color);};
 auto line=[&](int x,int y,int x2,int y2,uint32_t c){const int n=(std::max)(std::abs(x2-x),std::abs(y2-y));for(int i=0;i<=n;++i)rect(n?x+(x2-x)*i/n:x,n?y+(y2-y)*i/n:y,2,2,c);};
 const auto color=view.bright?orange:dimOrange;
 for(unsigned kind=0;kind<3;++kind)if(view.badges[kind]){const int x=layout.iconRight-116+int(kind)*40,y=layout.iconTop;
  if(kind==0){rect(x,y+5,28,2,color);rect(x,y+23,28,2,color);rect(x,y+5,2,20,color);rect(x+26,y+5,2,20,color);line(x+2,y+7,x+13,y+16,color);line(x+13,y+16,x+25,y+7,color);}
  else if(kind==1){rect(x+5,y+3,18,2,color);rect(x+5,y+3,2,14,color);rect(x+21,y+3,2,14,color);line(x+6,y+16,x+13,y+21,color);line(x+13,y+21,x+22,y+16,color);rect(x+13,y+21,2,6,color);rect(x+7,y+27,14,2,color);line(x+4,y+6,x,y+7,color);line(x,y+7,x+4,y+15,color);line(x+24,y+6,x+28,y+7,color);line(x+28,y+7,x+24,y+15,color);}
  else{rect(x+11,y+2,7,7,color);line(x+14,y+11,x+14,y+24,color);line(x+14,y+13,x+7,y+17,color);line(x+14,y+13,x+21,y+17,color);rect(x+1,y+8,6,6,color);rect(x+23,y+8,6,6,color);line(x+4,y+16,x+4,y+25,color);line(x+26,y+16,x+26,y+25,color);line(x+14,y+24,x+9,y+29,color);line(x+14,y+24,x+19,y+29,color);}
 }
 if(view.news&&now>=view.news->beganMs&&!view.news->text.empty()&&view.news->text.size()<=4096){try{
  if(!impl_)impl_=std::make_unique<Impl>();auto& p=*impl_;std::wstring text=L"ALERT  "+view.news->text;SIZE extent{};if(!GetTextExtentPoint32W(p.dc,text.data(),int(text.size()),&extent))return painted;
  const double travel=double(now-view.news->beganMs)*.072;const int area=layout.newsRight-layout.newsLeft;
  if(travel<double(area)+extent.cx+8){GdiFlush();std::fill_n(p.pixels,1280*720,0u);const int x=layout.newsRight-int(travel);auto saved=SaveDC(p.dc);IntersectClipRect(p.dc,layout.newsLeft,layout.newsTop,layout.newsRight,layout.newsTop+28);SetTextColor(p.dc,RGB(255,255,255));RECT r{x,layout.newsTop,x+extent.cx+8,layout.newsTop+28};DrawTextW(p.dc,text.data(),int(text.size()),&r,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);RestoreDC(p.dc,saved);GdiFlush();
   for(int y=layout.newsTop;y<layout.newsTop+28;++y)for(int col=layout.newsLeft;col<layout.newsRight;++col){const auto raw=p.pixels[size_t(y)*1280+col];const auto alpha=(std::max)({raw&255,(raw>>8)&255,(raw>>16)&255});if(alpha)pixel(col,y,0xffffff,alpha);}
  }
 }catch(const std::exception&){return painted;}}
 return painted;
}
}
