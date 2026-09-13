#pragma once
#include "invitation_overlay.h"
#include "tournament_invitation.h"
#include <memory>
namespace mgo2win::invitation_ui {
class Presenter {
 std::shared_ptr<invitations::Session> session_;
 Overlay overlay_;View view_;uint64_t feedbackAt_=0,clock_=0;
 unsigned phase_=0;
public:
 void session(std::shared_ptr<invitations::Session>);
 void update(uint64_t now);
 bool respond(Response,uint64_t now);
 Overlay& overlay(){return overlay_;}
 const View& view()const{return view_;}
};
}
