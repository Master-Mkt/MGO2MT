#pragma once

namespace mgo2win::invitation_ui {
// UI-thread state. Presenter updates do not acknowledge a newly painted screen.
class InputGuard {
 bool paintedModal_=false,leftOwned_=false;
public:
 // Call only after composing the next complete screen, including no-modal frames.
 // Preserve an in-progress mouse gesture across redraw, expiry and scope changes.
 void painted(bool modal){paintedModal_=modal;}
 bool stale(bool currentModal)const{return paintedModal_&&!currentModal;}
 bool blocks_gameplay(bool currentModal)const{return paintedModal_||currentModal;}
 // Call for both left down/up before generic stale-input handling. False leaves
 // routing to the current modal/background; true consumes an obsolete gesture.
 bool left_button(bool down,bool currentModal){
  const bool oldGesture=leftOwned_;
  if(down)leftOwned_=currentModal||paintedModal_;
  else leftOwned_=false;
  return stale(currentModal)||(!down&&oldGesture&&!currentModal);
 }
 // Only destruction/full input reset. A Session scope change alone is not reset.
 void reset(){paintedModal_=leftOwned_=false;}
};
}
