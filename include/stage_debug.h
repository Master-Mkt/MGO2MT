#pragma once
namespace mgo2win::stage {
// Key numbers are Windows virtual keys. A held key never triggers another reset.
struct DebugControls {
 bool enabled=false,reset=false,toggleMusic=false,confirmReset=false,resetYes=false;int musicStep=0;
 bool key(unsigned key,bool repeat,bool stage){
  if(!stage)cancel_reset();
  if(key==0x7b){if(!repeat){enabled=!enabled;cancel_reset();}return true;} // F12
  if(!enabled||!stage)return false;
  if(confirmReset){
   if(!repeat){if(key==0x1b)cancel_reset();else if(key==0x25||key==0x26)resetYes=true;else if(key==0x27||key==0x28)resetYes=false;else if(key==9)resetYes=!resetYes;else if(key==13||key==32){bool yes=resetYes;cancel_reset();reset=yes;}}
   return true; // modal: no underlying room/BGM key handling
  }
  if(key!=0x74&&key!=0x76&&key!=0x77&&key!=0x78)return false;
  if(!repeat){if(key==0x74){confirmReset=true;resetYes=false;}else if(key==0x78)toggleMusic=true;else musicStep+=key==0x76?-1:1;}
  return true;
 }
 void clear_actions(){reset=false;toggleMusic=false;musicStep=0;}
 void cancel_reset(){confirmReset=false;resetYes=false;reset=false;}
 void focus_lost(){clear_actions();cancel_reset();}
 void click(int x,int y){if(!confirmReset)return;if(y>=397&&y<442&&((x>=385&&x<605)||(x>=675&&x<895))){bool yes=x<605;cancel_reset();reset=yes;}}
};
}
