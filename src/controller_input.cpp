#include "controller_input.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
namespace mgo2win {
bool valid_input_key(unsigned k){return (k>=0x30&&k<=0x39)||(k>=0x41&&k<=0x5a)||(k>=VK_NUMPAD0&&k<=VK_DIVIDE)||(k>=VK_F3&&k<=VK_F12)||(k>=VK_OEM_1&&k<=VK_OEM_3)||(k>=VK_OEM_4&&k<=VK_OEM_8)||k==VK_OEM_102||k==VK_BACK||k==VK_RETURN||k==VK_SPACE||k==VK_SHIFT||k==VK_CONTROL||k==VK_INSERT||k==VK_DELETE||k==VK_HOME||k==VK_END||(k>=VK_LEFT&&k<=VK_DOWN);}
bool valid_input_config(const InputConfig& c){
 if(c.device>1||c.slot>3||c.left_deadzone>90||c.right_deadzone>90||c.run_threshold<10||c.run_threshold>100||c.run_hysteresis>30||c.run_hysteresis>=c.run_threshold)return false;
 for(unsigned i=0;i<input_actions;++i){if(!valid_input_key(c.keyboard[i])||c.gamepad[i]>=24)return false;for(unsigned j=0;j<i;++j)if(c.keyboard[i]==c.keyboard[j]||c.gamepad[i]==c.gamepad[j])return false;}return true;
}
bool load_input(const std::filesystem::path& p,InputConfig& c){
 if(!std::filesystem::exists(p))return false;if(std::filesystem::file_size(p)>2048)throw std::runtime_error("Input settings too large");
 std::ifstream f(p);std::string tag,extra;unsigned version;InputConfig draft;
 if(!(f>>tag>>version>>draft.device>>draft.slot)||tag!="MGO2WIN.INPUT"||(version!=1&&version!=2))throw std::runtime_error("Invalid input settings");
 for(auto& k:draft.keyboard)if(!(f>>k))throw std::runtime_error("Missing key");
 for(auto& k:draft.gamepad)if(!(f>>k))throw std::runtime_error("Missing pad binding");
 if(version==2){if(!(f>>draft.left_deadzone>>draft.right_deadzone>>draft.run_threshold>>draft.run_hysteresis))throw std::runtime_error("Missing analog settings");}
 // These are complete historical presets, not a guess from individual buttons.
 // Even one customized pad binding prevents migration; keyboard/tuning stay intact.
 constexpr std::array<unsigned,input_actions> oldPreset={0,1,2,3,5,4,6,7,8,9,14,15,10,11,12,13,16,17,18,19,20,21,22,23};
 auto firstPreset=oldPreset;std::swap(firstPreset[4],firstPreset[5]);
 if(draft.gamepad==oldPreset||(version==1&&draft.gamepad==firstPreset))draft.gamepad=InputConfig{}.gamepad;

 if(f>>extra||!f.eof()||!valid_input_config(draft))throw std::runtime_error("Invalid input mapping");c=draft;return true;
}
void save_input(const std::filesystem::path& p,const InputConfig& c){
 if(!valid_input_config(c))throw std::runtime_error("Invalid input mapping");std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L"."+std::to_wstring(GetCurrentProcessId())+L".tmp";
 try{{std::ofstream f(temp);f<<"MGO2WIN.INPUT 2\n"<<c.device<<' '<<c.slot<<'\n';for(auto k:c.keyboard)f<<k<<' ';f<<'\n';for(auto k:c.gamepad)f<<k<<' ';f<<'\n';f<<c.left_deadzone<<' '<<c.right_deadzone<<' '<<c.run_threshold<<' '<<c.run_hysteresis<<'\n';f.close();if(!f)throw std::runtime_error("Input settings write failed");}
 if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Input settings replace failed");
 }catch(...){std::error_code ec;std::filesystem::remove(temp,ec);throw;}
}
void assign_input(InputConfig& c,unsigned action,unsigned code){
 if(action>=input_actions||(c.device?code>=24:!valid_input_key(code)))throw std::runtime_error("Invalid binding");auto& a=c.device?c.gamepad:c.keyboard;
 auto old=a[action];for(unsigned i=0;i<input_actions;++i)if(i!=action&&a[i]==code)a[i]=old;a[action]=code;
}
const wchar_t* action_name(unsigned n){static const wchar_t* names[]={L"選択肢 上",L"選択肢 下",L"選択肢 左 / 主観リーン左",L"選択肢 右 / 主観リーン右",L"決定 / リロード",L"キャンセル / 姿勢・回避",L"AUTO AIM 切替",L"壁アクション / 敬礼 / 伏せ姿勢方向",L"主観切替（補助） / 前のタブ",L"視線を戻す / 次のタブ",L"武器を発射",L"武器を構える",L"設定メニュー",L"チャットメニュー",L"武器一覧（長押し・離して決定）",L"装備一覧（長押し・離して決定）",L"移動 前",L"移動 後",L"移動 左",L"移動 右",L"視線 上",L"視線 下",L"視線 左",L"視線 右"};return n<24?names[n]:L"?";}
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
namespace {
float stick_axis(short value){return value<0?float(value)/32768.f:float(value)/32767.f;}
void stick_values(short x,short y,unsigned deadzone,float& ox,float& oy,float& raw){
 float fx=stick_axis(x),fy=stick_axis(y),length=std::hypot(fx,fy);raw=std::min(length,1.f);float dz=float(std::min(deadzone,90u))*.01f;
 if(length<=dz){ox=oy=0;return;}float adjusted=(raw-dz)/(1.f-dz);ox=fx/length*adjusted;oy=fy/length*adjusted;
}
float trigger_value(BYTE raw){return raw<=XINPUT_GAMEPAD_TRIGGER_THRESHOLD?0.f:float(raw-XINPUT_GAMEPAD_TRIGGER_THRESHOLD)/float(255-XINPUT_GAMEPAD_TRIGGER_THRESHOLD);}
}
bool input_running(float magnitude,bool was_running,const InputConfig& c){
 if(!std::isfinite(magnitude)||magnitude<=0||!valid_input_config(c))return false;
 float threshold=float(c.run_threshold-(was_running?c.run_hysteresis:0))*.01f;
 return std::min(magnitude,1.f)>=threshold;
}
ControllerInput::ControllerInput(const std::filesystem::path& p){try{load_input(p,config);}catch(...){config={};}}
PadSample ControllerInput::poll(bool active,unsigned slot){
 if(slot>3)slot=config.slot;if(slot!=last_slot_){reset();last_slot_=slot;}XINPUT_STATE state{};PadSample s;s.connected=reader(slot,&state)==ERROR_SUCCESS;
 if(!s.connected){reset();return s;}
 const auto& p=state.Gamepad;
 stick_values(p.sThumbLX,p.sThumbLY,config.left_deadzone,s.left_x,s.left_y,s.raw_left_magnitude);
 stick_values(p.sThumbRX,p.sThumbRY,config.right_deadzone,s.right_x,s.right_y,s.raw_right_magnitude);
 s.left_trigger=trigger_value(p.bLeftTrigger);s.right_trigger=trigger_value(p.bRightTrigger);
 s.raw_held=pad_buttons(p)&0xffffu;
 const float axes[]={s.left_y,s.left_x,s.right_y,s.right_x};
 for(unsigned i=0;i<4;++i){unsigned base=16+i*2;if(axes[i]>0)s.raw_held|=1u<<(base+(i%2?1:0));if(axes[i]<0)s.raw_held|=1u<<(base+(i%2?0:1));}
 if(!active)reset();
 else if(!ready_&&!s.raw_held)ready_=true;
 else if(ready_){s.armed=true;s.held=s.raw_held;s.pressed=s.held&~previous_;s.released=previous_&~s.held;previous_=s.held;return s;}
 // Raw diagnostics are safe to display, but cannot become movement or an edge.
 s.left_x=s.left_y=s.right_x=s.right_y=s.left_trigger=s.right_trigger=0;return s;
}
unsigned ControllerInput::keyboard_menu(unsigned key)const{if(config.device)return 0;for(unsigned a=0;a<24;++a)if(config.keyboard[a]==key)return menu_key(a);return 0;}
std::array<bool,input_actions> ControllerInput::actions(const PadSample& s)const{std::array<bool,input_actions> out{};if(config.device&&s.connected)for(unsigned i=0;i<24;++i)out[i]=(s.pressed&(1u<<config.gamepad[i]))!=0;return out;}
std::array<float,input_actions> ControllerInput::action_values(const PadSample& s)const{
 std::array<float,input_actions> out{};if(!config.device||!s.connected||!s.armed)return out;
 std::array<float,24> values{};for(unsigned i=0;i<14;++i)values[i]=(s.held&(1u<<i))?1.f:0.f;
 values[14]=s.left_trigger;values[15]=s.right_trigger;
 values[16]=s.left_y;values[17]=-s.left_y;values[18]=-s.left_x;values[19]=s.left_x;
 values[20]=s.right_y;values[21]=-s.right_y;values[22]=-s.right_x;values[23]=s.right_x;
 for(unsigned i=0;i<input_actions;++i)if(config.gamepad[i]<values.size()){float value=values[config.gamepad[i]];out[i]=std::isfinite(value)?std::clamp(value,0.f,1.f):0.f;}
 return out;
}
}
