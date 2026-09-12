#include "weapon_restrictions.h"

namespace mgo2win::restrictions {
namespace {
constexpr Bits bit(unsigned index){Bits b{};b[index/8]=uint8_t(1u<<(index%8));return b;}
constexpr Bits suppressors(){Bits b{};b[9]=0x20;b[10]=0x0e;return b;}
using enum Category;
// HostGameEnvFactory.buildWeaponRestrictions, current candidate. Category and
// mask are HOST policy controls, not the ordinary-player equipment catalog.
const std::array rows{
 Entry{"knife",primary,"ST KNIFE",uint16_t(1),bit(1),false},
 Entry{"mp5",primary,"MP5",uint16_t(18),bit(18),true},
 Entry{"p90",primary,"P90",uint16_t(20),bit(20),false},
 Entry{"patriot",primary,"PATRIOT",uint16_t(22),bit(22),true},
 Entry{"vz",primary,"SKORPION",uint16_t(23),bit(23),false},
 Entry{"m4",primary,"M4",uint16_t(24),bit(24),false},
 Entry{"ak",primary,"AK102",uint16_t(25),bit(25),false},
 Entry{"g3a3",primary,"G3A3",uint16_t(26),bit(26),true},
 Entry{"mk17",primary,"MK17",uint16_t(30),bit(30),true},
 Entry{"xm8",primary,"XM8",uint16_t(31),bit(31),true},
 Entry{"m60",primary,"M60E4",uint16_t(35),bit(35),true},
 Entry{"m870",primary,"M870",uint16_t(37),bit(37),false},
 Entry{"saiga",primary,"SAIGA",uint16_t(38),bit(38),true},
 Entry{"vss",primary,"VSS",uint16_t(39),bit(39),true},
 Entry{"dsr",primary,"DSR1",uint16_t(41),bit(41),true},
 Entry{"m14",primary,"M14EBR",uint16_t(42),bit(42),true},
 Entry{"mosin",primary,"MOSIN N",uint16_t(43),bit(43),false},
 Entry{"svd",primary,"DRAGNOV",uint16_t(44),bit(44),false},
 Entry{"rpg",primary,"RPG7",uint16_t(50),bit(50),true},
 Entry{"shield",primary,"SHIELD",uint16_t(73),bit(73),false},
 Entry{"mk2",secondary,"RUGER",uint16_t(2),bit(2),false},
 Entry{"operator",secondary,"OPERATOR",uint16_t(3),bit(3),true},
 Entry{"mk23",secondary,"SOCOM",uint16_t(4),bit(4),true},
 Entry{"gsr",secondary,"SIG GSR",uint16_t(7),bit(7),false},
 Entry{"de",secondary,"D EAGLE",uint16_t(8),bit(8),true},
 Entry{"g18",secondary,"GLOCK",uint16_t(15),bit(15),true},
 Entry{"grenade",support,"GRENADE",uint16_t(52),bit(52),false},
 Entry{"wp",support,"WP G",uint16_t(53),bit(53),true},
 Entry{"stun",support,"STUN G",uint16_t(54),bit(54),false},
 Entry{"chaff",support,"CHAFF G",uint16_t(55),bit(55),false},
 Entry{"smoke",support,"SMOKE G",uint16_t(56),bit(56),false},
 Entry{"smoke_r",support,"SMOKE G.(R)",uint16_t(57),bit(57),true},
 Entry{"smoke_g",support,"SMOKE G.(G)",uint16_t(58),bit(58),true},
 Entry{"smoke_y",support,"SMOKE G.(Y)",uint16_t(59),bit(59),true},
 Entry{"eloc",support,"EMP GRENADE",uint16_t(63),bit(63),true},
 Entry{"claymore",support,"CLAYMORE",uint16_t(64),bit(64),false},
 Entry{"sgmine",support,"SLEEP CLAYMORE",uint16_t(65),bit(65),true},
 Entry{"c4",support,"C4",uint16_t(66),bit(66),true},
 Entry{"sgsatchel",support,"SLEEP C4",uint16_t(67),bit(67),true},
 Entry{"magazine",support,"BOOK",uint16_t(69),bit(69),false},
 Entry{"masterkey",custom,"MASTERKEY",std::nullopt,bit(74),std::nullopt},
 Entry{"xm320",custom,"XM320",std::nullopt,bit(75),std::nullopt},
 Entry{"gp30",custom,"GP30",std::nullopt,bit(76),std::nullopt},
 Entry{"suppressor",custom,"SUPPRESSOR",std::nullopt,suppressors(),std::nullopt},
 Entry{"scope",custom,"SCOPE",std::nullopt,bit(92),std::nullopt},
 Entry{"sight",custom,"SIGHT",std::nullopt,bit(93),std::nullopt},
 Entry{"lightlg",custom,"LIGHT (LONG GUN)",std::nullopt,bit(95),std::nullopt},
 Entry{"laser",custom,"LASER",std::nullopt,bit(96),std::nullopt},
 Entry{"lighthg",custom,"LIGHT (HANDGUN)",std::nullopt,bit(97),std::nullopt},
 Entry{"grip",custom,"GRIP",std::nullopt,bit(98),std::nullopt},
 Entry{"drum",items,"DRUM",std::nullopt,bit(106),std::nullopt},
 Entry{"envg",items,"ENVG",std::nullopt,bit(118),std::nullopt},
};
}
std::span<const Entry> catalog(){return rows;}
const Entry* find(std::string_view key){for(const auto& e:rows)if(e.key==key)return &e;return nullptr;}
const char* category_name(Category c){switch(c){case primary:return "PRIMARY";case secondary:return "SECONDARY";case support:return "SUPPORT";case custom:return "CUSTOM";case items:return "ITEMS";}return "UNKNOWN";}
bool enabled(const Bits& b){return (b[0]&1)!=0;}
void set_enabled(Bits& b,bool value){b[0]=uint8_t((b[0]&~1u)|(value?1u:0u));}
LockState state(const Bits& b,const Entry& e){
 bool any=false,all=true,has_mask=false;
 for(size_t i=0;i<b.size();++i){has_mask|=e.mask[i]!=0;any|=(b[i]&e.mask[i])!=0;all&=(b[i]&e.mask[i])==e.mask[i];}
 if(!any||!has_mask)return LockState::unlocked;
 return all?LockState::locked:LockState::mixed;
}
bool locked(const Bits& b,const Entry& e){return state(b,e)!=LockState::unlocked;}
bool effective_locked(const Bits& b,const Entry& e){return enabled(b)&&locked(b,e);}
void set_locked(Bits& b,const Entry& e,bool value){
 for(size_t i=0;i<b.size();++i){auto mask=uint8_t(e.mask[i]&(i==0?0xfe:0xff));b[i]=value?uint8_t(b[i]|mask):uint8_t(b[i]&~mask);}
}
Bits category_mask(Category category){Bits result{};for(const auto& e:rows)if(e.category==category)for(size_t i=0;i<result.size();++i)result[i]|=e.mask[i];return result;}
void set_category_locked(Bits& b,Category category,bool value){for(const auto& e:rows)if(e.category==category)set_locked(b,e,value);}
void set_all_locked(Bits& b,bool value){for(const auto& e:rows)set_locked(b,e,value);}
}
