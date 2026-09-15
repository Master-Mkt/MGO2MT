#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "motion_blend_dialog.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace mgo2win::motion_blend;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::wstring text(HWND dialog,int id){std::array<wchar_t,512> value{};GetDlgItemTextW(dialog,id,value.data(),int(value.size()));return value.data();}
static std::string bytes(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
static void click(HWND dialog,int id){SendMessageW(dialog,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),LPARAM(GetDlgItem(dialog,id)));}
static void rate(HWND dialog,const wchar_t* value){check(SetDlgItemTextW(dialog,100,value)!=FALSE,"set draft rate");}
struct OwnerInput {unsigned count=0;WPARAM key=0;LPARAM flags=0;};
static LRESULT CALLBACK owner_procedure(HWND window,UINT message,WPARAM wp,LPARAM lp){
 auto* input=reinterpret_cast<OwnerInput*>(GetWindowLongPtrW(window,GWLP_USERDATA));
 if(message==WM_NCCREATE){input=static_cast<OwnerInput*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(window,GWLP_USERDATA,LONG_PTR(input));}
 if(message==WM_KEYDOWN&&input){++input->count;input->key=wp;input->flags=lp;return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
static void layout(HWND dialog){
 RECT client{};check(GetClientRect(dialog,&client)!=FALSE,"dialog client bounds");
 for(int id:{100,101,102,103,IDOK,IDCANCEL}){
  HWND child=GetDlgItem(dialog,id);check(child&&IsWindow(child)&&GetParent(child)==dialog,"real child control exists");
  RECT bounds{};check(GetWindowRect(child,&bounds)!=FALSE,"control bounds available");
  MapWindowPoints(nullptr,dialog,reinterpret_cast<POINT*>(&bounds),2);
  check(bounds.left>=0&&bounds.top>=0&&bounds.right<=client.right&&bounds.bottom<=client.bottom&&bounds.right>bounds.left&&bounds.bottom>bounds.top,"control fits dialog client area");
  check((GetWindowLongPtrW(child,GWL_STYLE)&WS_CHILD)!=0,"control is a child window");
  check(SendMessageW(child,WM_GETFONT,0,0)!=0,"dialog font propagated to controls");
 }
 check((GetWindowLongPtrW(GetDlgItem(dialog,100),GWL_STYLE)&ES_NUMBER)!=0,"rate edit uses numeric input style");
 check(SendDlgItemMessageW(dialog,100,EM_GETLIMITTEXT,0,0)==4,"manual numeric input bounded to four digits");
}

int main(){try{
 const auto folder=std::filesystem::temp_directory_path()/(L"mgo2win-blend-dialog-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 check(std::filesystem::create_directory(folder),"unique isolated UI fixture directory");
 const auto path=folder/L"motion_blend.cfg";
 OwnerInput ownerInput;WNDCLASSW ownerClass{};ownerClass.lpfnWndProc=owner_procedure;ownerClass.hInstance=GetModuleHandleW(nullptr);ownerClass.lpszClassName=L"MGO2WIN.MotionBlend.TestOwner";
 check(RegisterClassW(&ownerClass)!=0,"register hidden test owner class");
 HWND owner=CreateWindowExW(0,ownerClass.lpszClassName,L"Hidden motion blend test owner",WS_OVERLAPPEDWINDOW,100,100,900,700,nullptr,nullptr,GetModuleHandleW(nullptr),&ownerInput);
 check(owner&&IsWindow(owner)&&!IsWindowVisible(owner),"non-visible native owner created");
 Settings active;check(save(path,active),"create isolated initial cfg");
 Dialog dialog;dialog.open(owner,active,path,false);
 check(dialog.visible()&&dialog.handle()&&IsWindow(dialog.handle())&&!IsWindowVisible(dialog.handle()),"modeless dialog created without showing it");
 layout(dialog.handle());
 for(bool repeat:{false,true}){
  MSG message{};message.hwnd=GetDlgItem(dialog.handle(),100);message.message=WM_KEYDOWN;message.wParam=VK_F12;
  message.lParam=LPARAM(1)|(LPARAM(0x58)<<16)|(repeat?(LPARAM(1)<<30):0);
  const auto count=ownerInput.count;
  check(dialog.message(message),"F12 from child EDIT is consumed by modeless dialog adapter");
  check(ownerInput.count==count+1&&ownerInput.key==VK_F12&&ownerInput.flags==(message.lParam|(LPARAM(1)<<25)),"owner receives F12 once with trusted bit25 and exact original repeat/scancode bits");
  check(bool(ownerInput.flags&(LPARAM(1)<<30))==repeat,"forwarded F12 retains repeat state");
 }
 {MSG message{};message.hwnd=dialog.handle();message.message=WM_KEYDOWN;message.wParam=VK_F12;message.lParam=1;const auto count=ownerInput.count;
  check(dialog.message(message)&&ownerInput.count==count+1,"F12 from dialog itself forwards to owner");
  message.hwnd=owner;const auto after=ownerInput.count;dialog.message(message);check(ownerInput.count==after,"unrelated owner message cannot be reflected into trusted child forwarding");}
 check(text(dialog.handle(),100)==L"500"&&text(dialog.handle(),101).find(L"0.200")!=std::wstring::npos,"initial rate and duration explanation");
 auto originalHandle=dialog.handle();dialog.open(owner,active,path,false);check(dialog.handle()==originalHandle,"open is idempotent while the dialog exists");
 for(auto value:{L"10",L"2000",L"500"}){
  rate(dialog.handle(),value);click(dialog.handle(),IDOK);
  check(active.percentPerSecond==unsigned(std::stoul(value))&&load(path)==active,"apply updates live settings and durable cfg");
  check(!text(dialog.handle(),102).empty()&&dialog.visible(),"apply has feedback and retains modeless dialog");
 }
 const auto saved=bytes(path);
 for(auto value:{L"0",L"9",L"2001",L"",L"abc",L"+500",L"500.0",L"999999999999999"}){
  rate(dialog.handle(),value);click(dialog.handle(),IDOK);
  check(active.percentPerSecond==500&&bytes(path)==saved,"invalid draft does not alter active or saved settings");
  check(!text(dialog.handle(),102).empty(),"invalid input has feedback");
 }
 rate(dialog.handle(),L"1700");click(dialog.handle(),IDCANCEL);
 check(!dialog.visible()&&!dialog.handle()&&!IsWindow(originalHandle)&&active.percentPerSecond==500&&bytes(path)==saved,"close discards uncommitted draft and destroys dialog");
 active=load(path);dialog.open(owner,active,path,false);check(text(dialog.handle(),100)==L"500","reopen uses persisted setting");
 rate(dialog.handle(),L"1100");click(dialog.handle(),IDOK);check(active.percentPerSecond==1100,"apply nondefault before default-button test");
 const auto nondefault=bytes(path);
 click(dialog.handle(),103);check(text(dialog.handle(),100)==L"500"&&active.percentPerSecond==1100&&bytes(path)==nondefault,"default button edits draft only");
 click(dialog.handle(),IDCANCEL);dialog.open(owner,active,path,false);check(text(dialog.handle(),100)==L"1100","cancel after default button preserves old setting");
 click(dialog.handle(),103);click(dialog.handle(),IDOK);check(active.percentPerSecond==500&&load(path)==active,"explicit apply commits default button choice");
 const auto beforeFailure=bytes(path);
 HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 check(locked!=INVALID_HANDLE_VALUE,"lock destination against replacement");
 rate(dialog.handle(),L"1500");click(dialog.handle(),IDOK);CloseHandle(locked);
 check(active.percentPerSecond==500&&bytes(path)==beforeFailure&&dialog.visible(),"failed atomic save retains old live/file settings");
 check(text(dialog.handle(),102).find(L"保持")!=std::wstring::npos,"save failure communicates old setting retention");
 rate(dialog.handle(),L"1800");SendMessageW(dialog.handle(),WM_CLOSE,0,0);check(!dialog.visible()&&active.percentPerSecond==500,"window close discards draft");
 // Warmed-up repeated create/destroy detects leaked dialog/control GDI objects.
 const auto beforeGdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
 const auto beforeUser=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
 for(unsigned i=0;i<12;++i){dialog.open(owner,active,path,false);layout(dialog.handle());auto child=GetDlgItem(dialog.handle(),100);dialog.close();check(!IsWindow(child)&&!dialog.visible(),"close destroys edit and dialog windows");}
 check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)<=beforeGdi+1,"repeated dialogs do not leak GDI objects");
 check(GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS)<=beforeUser+1,"repeated dialogs do not leak windows or controls");
 {MSG message{};message.hwnd=owner;message.message=WM_KEYDOWN;message.wParam=VK_F12;check(!dialog.message(message),"closed dialog never routes a key");}
 DestroyWindow(owner);UnregisterClassW(ownerClass.lpszClassName,ownerClass.hInstance);std::filesystem::remove(path);std::filesystem::remove(folder);
 std::cout<<"hidden modeless motion blend controls, apply/cancel/default/save failure and lifecycle passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
