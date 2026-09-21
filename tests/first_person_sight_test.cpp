#include "first_person_sight.h"
#include "weapon_hand_renderer.h"
#include "original_first_person_lens.h"
#include "source_coordinates.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mgo2mt;
namespace fs=mgo2mt::first_person_sight;
namespace {
void check(bool v,const char*m){if(!v)throw std::runtime_error(m);}
std::vector<char> read(const std::filesystem::path&p){std::ifstream in(p,std::ios::binary);check(bool(in),"fixture missing");return {std::istreambuf_iterator<char>(in),{}};}
float length(fs::Vec3 p){return std::sqrt(p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);}
fs::Vec3 direction(fs::Vec3 a,fs::Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];auto n=length(a);for(auto&v:a)v/=n;return a;}
void collinear(const fs::Result&r,const fs::Axis&a){auto f=direction(a.front,r.view.eye),b=direction(a.rear,r.view.eye);for(unsigned i=0;i<3;++i)check(std::abs(f[i]-b[i])<.0001f,"front and rear CNP points have same camera projection");}
void synthetic(){
 WorldView view{{0,100,0},{0,0,1},16.f/9,.6f};fs::Controller c;fs::Axis line{{120,100,200},{120,100,600}};auto r=c.update(view,24,line);check(r.calibrated&&!r.held&&r.view.direction==view.direction&&r.view.aspect==view.aspect&&r.view.verticalFov==view.verticalFov,"only eye changed");check(std::abs(r.view.eye[0]-120)<.01f&&r.view.eye[2]==0,"minimum lateral translation");collinear(r,line);
 view.eye={10,100,20};view.direction={1,0,0};r=c.update(view,24,{});check(r.held&&std::abs(r.view.eye[0]-10)<.01f&&std::abs(r.view.eye[2]+100)<.01f,"reload offset follows actor turn and movement");r=c.update(view,25,{});check(!r.calibrated&&r.view.eye==view.eye,"weapon switch clears old calibration");c.reset();check(!c.update(view,24,{}).calibrated,"life reset clears old offset");
 fs::Axis zero{{0,0,0},{0,0,0}};check(!c.update(view,24,zero).calibrated,"degenerate pair rejected");line.front[0]=std::numeric_limits<float>::quiet_NaN();check(!c.update(view,24,line).calibrated,"nonfinite pair rejected");
 view={{0,100,500},{0,0,1},16.f/9,.6f};line={{120,100,200},{120,100,600}};r=c.update(view,24,line);check(r.calibrated&&std::abs(r.view.eye[2]-100)<.01f,"rear point remains ahead of the eye");collinear(r,line);
 line={{120,100,600},{120,100,200}};r=c.update(view,24,line);check(!r.calibrated&&r.view.eye==view.eye,"backward stable pose clears stale stance offset");check(!c.update(view,24,{}).calibrated,"invalid stance cannot seed a reload offset");
}
}
int main(int argc,char**argv){try{
 check(argc==5,"weapons data root, character data root, CSV, external connection resource");synthetic();std::filesystem::path weapons=argv[1],characters=argv[2];weapon_hand::Models models(weapons/"weapons");std::string connectionError;check(models.load_connections(argv[4],connectionError),connectionError.c_str());weapon_hand::Bank hands(read(weapons/"weapons/hands.gwh"));CharacterCatalog catalog(read(characters/"character/appearance.gwc"));PlayerMotionBank motions(read(characters/"character/player.gwmot"));
 std::ofstream csv;if(argc>=4){csv.open(argv[3]);check(bool(csv),"CSV open");csv<<"weapon,gender,posture,pitch,yaw,shift,axis_degrees,eye_x,eye_y,eye_z,center_dx_1280,center_dy_720\n";}
 constexpr unsigned ids[]={2,3,4,7,8,15,18,20,23,24,25,26,30,31,35,37,38,39,41,42,43,44,50};unsigned cases=0,rejected=0,fallbacks=0;float maximumShift=0,maximumAngle=0;
 for(unsigned gender=0;gender<2;++gender){std::array<uint8_t,28>appearance{};appearance[0]=uint8_t(gender);appearance[2]=11;appearance[3]=22;appearance[15]=46;appearance[17]=57;auto body=catalog.assemble(appearance);check(body.ready(),"original rig ready");
  for(unsigned id:ids){auto model=models.find(id);check(model!=nullptr,"original gun model");auto local=fs::local_axis(id,*model,models.connection_points());check(bool(local),"all 23 firearms have SHA-bound original CNP pair");auto bad=*model;bad.parts[0].original.mdnSha256="wrong";check(!fs::local_axis(id,bad,models.connection_points()),"foreign model does not inherit CNP pair");
   for(unsigned posture=0;posture<3;++posture)for(float pitch:{-.4f,0.f,.4f})for(float yaw:{0.f,1.1f}){
    auto base=motions.sample(posture==0?PlayerMotion::Idle:posture==1?PlayerMotion::CrouchIdle:PlayerMotion::ProneIdle,0);auto selected=hands.select(id,PlayerMotion::Aim,0,true,-1,-1,int(posture));check(base&&selected,"original motion samples");weapon_hand::upper_body(*base,*selected,catalog.skeleton(gender));catalog.pose(body,*base);check(weapon_hand::aim_pitch(body,catalog.skeleton(gender),pitch),"native pitch applied before attachment");auto frame=weapon_hand::frame(body,selected->point);check(bool(frame),"current posed MTP frame");const fs::Vec3 origin{3000,2,-4000};auto axis=fs::world_axis(*local,*frame,yaw,origin);check(bool(axis),"finite transformed original pair");
    const float height=posture==0?1700.f:posture==1?1100.f:560.f;WorldView view{{origin[0],origin[1]+height*.65f,origin[2]},{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)},16.f/9,.6f};view.verticalFov=original_first_person::vertical_fov_degrees(uint16_t(id),0,view.aspect,original_first_person::scope_capable(uint16_t(id)),false)*.0174532925199433f;fs::Controller camera;auto aligned=camera.update(view,id,axis);
    if(id==50){check(!aligned.calibrated&&aligned.view.eye==view.eye&&aligned.view.direction==view.direction&&aligned.view.verticalFov==view.verticalFov,"RPG family adapter remains baseline when original CNP axis falls outside lens");++fallbacks;continue;}
    if(!aligned.calibrated){auto v=direction(axis->front,axis->rear);auto dot=[](fs::Vec3 a,fs::Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};fs::Vec3 from{view.eye[0]-axis->rear[0],view.eye[1]-axis->rear[1],view.eye[2]-axis->rear[2]};float along=std::min(-100.f,dot(from,v));fs::Vec3 delta{};for(unsigned i=0;i<3;++i)delta[i]=axis->rear[i]+along*v[i]-view.eye[i];auto sourceBody=body;catalog.pose(sourceBody,selected->pose);weapon_hand::aim_pitch(sourceBody,catalog.skeleton(gender),pitch);auto sourceFrame=weapon_hand::frame(sourceBody,selected->point);auto sourceAxis=fs::world_axis(*local,*sourceFrame,yaw,origin);auto sourceDir=direction(sourceAxis->front,sourceAxis->rear);std::cerr<<"REJECT fullcos="<<dot(sourceDir,view.direction)<<" weapon="<<id<<" gender="<<gender<<" posture="<<posture<<" pitch="<<pitch<<" yaw="<<yaw<<" cosine="<<dot(v,view.direction)<<" shift="<<length(delta)<<" axis="<<v[0]<<','<<v[1]<<','<<v[2]<<" rear="<<axis->rear[0]<<','<<axis->rear[1]<<','<<axis->rear[2]<<'\n';++rejected;continue;}
    check(aligned.view.direction==view.direction&&aligned.view.verticalFov==view.verticalFov&&aligned.view.aspect==view.aspect,"navigation direction and original lens preserved across catalog");collinear(aligned,*axis);maximumShift=std::max(maximumShift,aligned.displacement);maximumAngle=std::max(maximumAngle,aligned.axisAngleRadians);++cases;
    {auto aim=direction(axis->front,axis->rear);const fs::Vec3 right{std::cos(yaw),0,-std::sin(yaw)},up{-std::sin(yaw)*std::sin(pitch),std::cos(pitch),-std::cos(yaw)*std::sin(pitch)};auto dot=[](fs::Vec3 a,fs::Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};const float scale=360.f/(dot(aim,view.direction)*std::tan(view.verticalFov*.5f));check(std::abs(dot(aim,right)*scale)<20.f&&std::abs(dot(aim,up)*scale)<20.f,"ordinary firearm CNP sights remain within20 pixels of screen center at1280x720");if(csv)csv<<id<<','<<gender<<','<<posture<<','<<pitch<<','<<yaw<<','<<aligned.displacement<<','<<aligned.axisAngleRadians*57.2957795f<<','<<aligned.view.eye[0]<<','<<aligned.view.eye[1]<<','<<aligned.view.eye[2]<<','<<source_screen_x*dot(aim,right)*scale<<','<<-dot(aim,up)*scale<<'\n';}
   }
  }
 }
 for(unsigned id:{1u,52u,64u,73u})if(auto model=models.find(id))check(!fs::local_axis(id,*model,models.connection_points()),"support/melee weapon has no invented camera pair");
 if(rejected)throw std::runtime_error(std::to_string(rejected)+" of "+std::to_string(cases+rejected)+" original pose cases rejected; see complete diagnostics");
 std::cout<<"PASS CNP view alignment: "<<cases<<" aligned + "<<fallbacks<<" explicit RPG baseline fallbacks, max shift "<<maximumShift<<", max residual axis angle "<<maximumAngle*57.2957795f<<" degrees; forward/lens preserved\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
