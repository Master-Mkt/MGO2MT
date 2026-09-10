#pragma once
#include <windows.h>
#include <Xinput.h>
#include <array>
#include <filesystem>
#include <string>
#include <functional>
namespace mgo2win {
constexpr unsigned input_actions=24;
struct InputConfig {
 unsigned device=0,slot=0;
 std::array<unsigned,input_actions> keyboard={VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,VK_RETURN,VK_BACK,'Z','X','Q','E','1','3','P','O','C','V','W','S','A','D','I','K','J','L'};
 std::array<unsigned,input_actions> gamepad={0,1,2,3,4,5,6,7,8,9,14,15,10,11,12,13,16,17,18,19,20,21,22,23};
};
bool valid_input_key(unsigned);
bool valid_input_config(const InputConfig&);
bool load_input(const std::filesystem::path&,InputConfig&);
void save_input(const std::filesystem::path&,const InputConfig&);
void assign_input(InputConfig&,unsigned action,unsigned code); // Swap duplicates within that device.
std::wstring input_name(unsigned code,bool pad);
const wchar_t* action_name(unsigned);
unsigned menu_key(unsigned action);
uint32_t pad_buttons(const XINPUT_GAMEPAD&);
struct PadSample {bool connected=false;uint32_t held=0,pressed=0;};
class ControllerInput {
 uint32_t previous_=0;bool ready_=false;unsigned last_slot_=4;
public:
 InputConfig config;
 using Reader=std::function<DWORD(DWORD,XINPUT_STATE*)>;
 Reader reader=XInputGetState;
 explicit ControllerInput(const std::filesystem::path&);
 PadSample poll(bool active,unsigned slot=4);
 unsigned keyboard_menu(unsigned key)const;
 std::array<bool,input_actions> actions(const PadSample&)const;
 void reset(){ready_=false;previous_=0;}
};
}
