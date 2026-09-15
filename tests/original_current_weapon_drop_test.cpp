#include "original_current_weapon_drop.h"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace mgo2win::original_current_weapon_drop;
static void check(bool value,const char* why){if(!value){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 unsigned accepted=0,rejected=0;
 for(uint32_t i=0;i<=72;++i){const auto gate=id_gate(i);check(gate.has_value(),"complete reviewed subset");*gate?++accepted:++rejected;}
 check(accepted==56&&rejected==17,"two original jump tables: 56/17");
 for(const auto id:{0U,1U,16U,29U,36U,46U,72U})check(id_gate(id)==false,"original excluded ID anchors");
 for(const auto id:{2U,5U,25U,52U,64U,69U,70U})check(id_gate(id)==true,"original eligible ID anchors");
 for(const auto id:{73U,94U,140U,180U,512U,std::numeric_limits<uint32_t>::max()})check(!id_gate(id).has_value(),"internal/unreviewed IDs unknown");
 Context c;c.currentWeaponPresent=true;c.weaponIndex=25;c.groundManagerPresent=true;c.menuContainsWeapon=true;c.onlineFlag=true;
 auto p=plan(c);check(p.path==Path::local_current&&p.requestsGroundRegistration&&p.registrationManagerAvailable&&p.callsMenuRemoval&&p.menuRemovalPermitted&&!p.clearsProxyFields,"AK current weapon path");
 c.groundManagerPresent=false;p=plan(c);
 check(p.requestsGroundRegistration&&!p.registrationManagerAvailable&&p.callsMenuRemoval&&p.menuRemovalPermitted,"raw caller ignores registration failure; not a native transaction");
 c.status110=true;check(!plan(c).menuRemovalPermitted&&plan(c).requestsGroundRegistration,"110 blocks menu removal only");
 c.status110=false;c.status139=true;check(!plan(c).menuRemovalPermitted,"139 blocks menu removal");
 c.status139=false;c.menuContainsWeapon=false;check(!plan(c).menuRemovalPermitted,"missing menu entry");
 c.menuContainsWeapon=true;c.weaponIndex=70;p=plan(c);
 check(p.requestsGroundRegistration&&p.callsMenuRemoval&&!p.menuRemovalPermitted,"pseudo-book has different ID and online removal gates");
 c.onlineFlag=false;check(plan(c).menuRemovalPermitted,"online protection is conditional");
 c.weaponIndex=1;p=plan(c);check(p.path==Path::no_operation&&!p.requestsGroundRegistration&&!p.callsMenuRemoval,"knife rejected before any mutation");
 c.weaponIndex=25;c.currentWeaponPresent=false;check(plan(c).path==Path::no_operation,"missing current weapon");
 c.currentWeaponPresent=true;c.actorRoleAt88=2;check(plan(c).path==Path::no_operation,"role2 suppresses local branch");
 c.actorTypeAt8c=8;c.isHost=false;check(plan(c).path==Path::no_operation,"proxy needs host");
 c.isHost=true;c.currentWeaponPresent=false;c.status110=true;p=plan(c);
 check(p.path==Path::host_proxy&&p.requestsGroundRegistration&&!p.callsMenuRemoval&&p.clearsProxyFields,"host proxy uses own ID and clears own fields, not local menu");
 c.weaponIndex=16;check(!plan(c).clearsProxyFields,"same rejection table for proxy");
 c.weaponIndex=73;check(plan(c).path==Path::unknown&&!plan(c).requestsGroundRegistration,"unreviewed default branch not activated");
 std::cout<<"original current weapon drop scoped contract PASS\n";
}
