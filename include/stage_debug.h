#pragma once
namespace mgo2mt::stage {
// Key numbers are Windows virtual keys. A held key never triggers another reset.
struct DebugControls {
 bool enabled=false,reset=false,toggleMusic=false,confirmReset=false,resetYes=false,openMotionBlend=false;int musicStep=0;
 bool key(unsigned key,bool repeat,bool stage){
  if(!stage)cancel_reset();
  if(key==0x7b){if(!repeat){enabled=!enabled;cancel_reset();}return true;} // F12
  if(!enabled)return false;
  if(confirmReset){
   if(!repeat){if(key==0x1b)cancel_reset();else if(key==0x25||key==0x26)resetYes=true;else if(key==0x27||key==0x28)resetYes=false;else if(key==9)resetYes=!resetYes;else if(key==13||key==32){bool yes=resetYes;cancel_reset();reset=yes;}}
   return true; // modal: no underlying room/BGM key handling
  }
  // Global debug action: character selection also has motion transitions.
  if(key==0x73){if(!repeat)openMotionBlend=true;return true;} // F4
  if(!stage)return false;
  if(key!=0x74&&key!=0x76&&key!=0x77&&key!=0x78)return false;
  if(!repeat){if(key==0x74){openMotionBlend=false;confirmReset=true;resetYes=false;}else if(key==0x78)toggleMusic=true;else musicStep+=key==0x76?-1:1;}
  return true;
 }
 void clear_actions(){reset=false;toggleMusic=false;musicStep=0;openMotionBlend=false;}
 void cancel_reset(){confirmReset=false;resetYes=false;reset=false;openMotionBlend=false;}
 void focus_lost(){clear_actions();cancel_reset();}
 void click(int x,int y){if(!confirmReset)return;if(y>=397&&y<442&&((x>=385&&x<605)||(x>=675&&x<895))){bool yes=x<605;cancel_reset();reset=yes;}}
};
}
