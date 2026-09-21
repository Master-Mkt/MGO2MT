#pragma once
#include "weapon_icons.h"
namespace mgo2mt {
// Reviewed direct archive+texture binding from playeroption.LA2. Shared-bank
// candidates are deliberately absent from this runtime bundle.
inline weapons::Icons& system_ui_icons(){static weapons::Icons icons;return icons;}
}
