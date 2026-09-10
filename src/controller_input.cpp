#include "controller_input.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
namespace mgo2win {
bool valid_input_key(unsigned k){return (k>=0x30&&k<=0x39)||(k>=0x41&&k<=0x5a)||(k>=VK_NUMPAD0&&k<=VK_DIVIDE)||(k>=VK_F3&&k<=VK_F12)||(k>=VK_OEM_1&&k<=VK_OEM_3)||(k>=VK_OEM_4&&k<=VK_OEM_8)||k==VK_OEM_102||k==VK_BACK||k==VK_RETURN||k==VK_SPACE||k==VK_SHIFT||k==VK_CONTROL||k==VK_INSERT||k==VK_DELETE||k==VK_HOME||k==VK_END||(k>=VK_LEFT&&k<=VK_DOWN);}
bool valid_input_config(const InputConfig& c){
 if(c.device>1||c.slot>3)return false;
 for(unsigned i=0;i<input_actions;++i){if(!valid_input_key(c.keyboard[i])||c.gamepad[i]>=24)return false;for(unsigned j=0;j<i;++j)if(c.keyboard[i]==c.keyboard[j]||c.gamepad[i]==c.gamepad[j])return false;}return true;
}
bool load_input(const std::filesystem::path& p,InputConfig& c){
 if(!std::filesystem::exists(p))return false;if(std::filesystem::file_size(p)>2048)throw std::runtime_error("Input settings too large");
 std::ifstream f(p);std::string tag,extra;unsigned version;InputConfig draft;
 if(!(f>>tag>>version>>draft.device>>draft.slot)||tag!="MGO2WIN.INPUT"||version!=1)throw std::runtime_error("Invalid input settings");
 for(auto& k:draft.keyboard)if(!(f>>k))throw std::runtime_error("Missing key");
 for(auto& k:draft.gamepad)if(!(f>>k))throw std::runtime_error("Missing pad binding");
 if(f>>extra||!f.eof()||!valid_input_config(draft))throw std::runtime_error("Invalid input mapping");c=draft;return true;
}
void save_input(const std::filesystem::path& p,const InputConfig& c){
 if(!valid_input_config(c))throw std::runtime_error("Invalid input mapping");std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try{{std::ofstream f(temp);f<<"MGO2WIN.INPUT 1\n"<<c.device<<' '<<c.slot<<'\n';for(auto k:c.keyboard)f<<k<<' ';f<<'\n';for(auto k:c.gamepad)f<<k<<' ';f<<'\n';f.close();if(!f)throw std::runtime_error("Input settings write failed");}
 if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Input settings replace failed");
 }catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
void assign_input(InputConfig& c,unsigned action,unsigned code){
 if(action>=input_actions||(c.device?code>=24:!valid_input_key(code)))throw std::runtime_error("Invalid binding");auto& a=c.device?c.gamepad:c.keyboard;
 auto old=a[action];for(unsigned i=0;i<input_actions;++i)if(i!=action&&a[i]==code)a[i]=old;a[action]=code;
}
const wchar_t* action_name(unsigned n){static const wchar_t* names[]={L"方向キー 上",L"方向キー 下",L"方向キー 左",L"方向キー 右",L"決定 / ×",L"戻る / ○",L"□",L"△",L"L1",L"R1",L"L2",L"R2",L"START",L"SELECT",L"L3",L"R3",L"左スティック 上",L"左スティック 下",L"左スティック 左",L"左スティック 右",L"右スティック 上",L"右スティック 下",L"右スティック 左",L"右スティック 右"};return n<24?names[n]:L"?";}
std::wstring input_name(unsigned k,bool pad){
 if(pad){static const wchar_t* names[]={L"D-pad ↑",L"D-pad ↓",L"D-pad ←",L"D-pad →",L"A",L"B",L"X",L"Y",L"LB",L"RB",L"START",L"BACK",L"LS 押込",L"RS 押込",L"LT",L"RT",L"LS ↑",L"LS ↓",L"LS ←",L"LS →",L"RS ↑",L"RS ↓",L"RS ←",L"RS →"};return k<24?names[k]:L"?";}
 wchar_t name[80]{};LONG scan=LONG(MapVirtualKeyW(k,MAPVK_VK_TO_VSC)<<16);if((k>=VK_PRIOR&&k<=VK_DOWN)||k==VK_INSERT||k==VK_DELETE)scan|=1<<24;
 if(GetKeyNameTextW(scan,name,80))return name;return L"Key "+std::to_wstring(k);
}
unsigned menu_key(unsigned a){switch(a){case 0:case 16:return VK_UP;case 1:case 17:return VK_DOWN;case 2:case 18:return VK_LEFT;case 3:case 19:return VK_RIGHT;case 4:case 12:return VK_RETURN;case 5:return VK_ESCAPE;case 8:return VK_F1;case 9:return VK_F2;case 13:return VK_F3;default:return 0;}}
uint32_t pad_buttons(const XINPUT_GAMEPAD& p){
 const WORD buttons[]={XINPUT_GAMEPAD_DPAD_UP,XINPUT_GAMEPAD_DPAD_DOWN,XINPUT_GAMEPAD_DPAD_LEFT,XINPUT_GAMEPAD_DPAD_RIGHT,XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,XINPUT_GAMEPAD_LEFT_SHOULDER,XINPUT_GAMEPAD_RIGHT_SHOULDER,XINPUT_GAMEPAD_START,XINPUT_GAMEPAD_BACK,XINPUT_GAMEPAD_LEFT_THUMB,XINPUT_GAMEPAD_RIGHT_THUMB};uint32_t b=0;for(unsigned i=0;i<14;++i)if(p.wButtons&buttons[i])b|=1u<<i;
 if(p.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)b|=1u<<14;if(p.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD)b|=1u<<15;
 const short axes[]={p.sThumbLY,p.sThumbLX,p.sThumbRY,p.sThumbRX};
 for(unsigned i=0;i<4;++i){int dz=i<2?XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE:XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;unsigned base=16+i*2; // Y: up/down, X: left/right.
 if(axes[i]>dz)b|=1u<<(base+(i%2?1:0));if(axes[i]<-dz)b|=1u<<(base+(i%2?0:1));}return b;
}
ControllerInput::ControllerInput(const std::filesystem::path& p){try{load_input(p,config);}catch(...){config={};}}
PadSample ControllerInput::poll(bool active,unsigned slot){if(slot>3)slot=config.slot;if(slot!=last_slot_){reset();last_slot_=slot;}XINPUT_STATE state{};PadSample s;s.connected=reader(slot,&state)==ERROR_SUCCESS;if(s.connected)s.held=pad_buttons(state.Gamepad);
 if(!active||!s.connected){reset();return s;}if(!ready_){previous_=s.held;if(!s.held)ready_=true;return s;}s.pressed=s.held&~previous_;previous_=s.held;return s;
}
unsigned ControllerInput::keyboard_menu(unsigned key)const{if(config.device)return 0;for(unsigned a=0;a<24;++a)if(config.keyboard[a]==key)return menu_key(a);return 0;}
std::array<bool,input_actions> ControllerInput::actions(const PadSample& s)const{std::array<bool,input_actions> out{};if(config.device&&s.connected)for(unsigned i=0;i<24;++i)out[i]=(s.pressed&(1u<<config.gamepad[i]))!=0;return out;}
}
