#include "native_radio_context.h"
namespace mgo2win::radio {
Context host_context(combat::Cycle& cycle,std::span<const Identity> admitted,uint64_t now){
 auto snapshot=cycle.service().authority().snapshot();
 std::optional<combat::wire::Preparation> prep;
 if(!admitted.empty())prep=cycle.service().preparation(admitted.front(),now);
 return detail::build(cycle.epoch(),&cycle.request(),&snapshot,prep?&*prep:nullptr,admitted,
              cycle.service().current_status()==combat::wire::Status::active&&cycle.service().authority().active());
}
}
