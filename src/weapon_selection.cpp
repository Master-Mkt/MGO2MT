#include "weapon_selection.h"
namespace mgo2win::weapons {
void Selection::context(std::optional<SelectionContext> next){
 context_=next;
 // Re-check the complete draft when balance or restrictions change. Keeping an
 // unaffordable ID selected would make the confirmation state misleading.
 if(!context_){selected_={};return;}
 for(unsigned i=0;i<3;++i){auto e=selected(Category(i));if(e&&access_to(*e)!=Access::allowed)selected_[i].reset();}
 if(quote().access==Access::insufficient_dp)selected_={};
}
const Entry* Selection::selected(Category category)const{
 auto i=unsigned(category);return catalog_&&i<3&&selected_[i]?catalog_->find(category,*selected_[i]):nullptr;
}
std::vector<const Entry*> Selection::choices(Category category)const{
 if(!catalog_||unsigned(category)>=3)return {};
 return catalog_->choices(category,context_.value_or(SelectionContext{}),true);
}
Access Selection::access_to(const Entry&e)const{return context_?weapons::access(e,*context_):Access::unverified;}
Quote Selection::quote()const{
 if(!context_)return {Access::unverified,0};
 std::vector<const Entry*> entries;for(unsigned i=0;i<3;++i)if(auto e=selected(Category(i)))entries.push_back(e);
 return weapons::quote(entries,*context_);
}
Access Selection::choose(Category category,uint16_t id){
 auto i=unsigned(category);if(!catalog_||i>=3)return Access::unverified;
 auto e=catalog_->find(category,id);if(!e)return Access::unverified;
 auto status=access_to(*e);if(status!=Access::allowed)return status;
 auto old=selected_[i];selected_[i]=id;auto checked=quote();
 if(checked.access!=Access::allowed)selected_[i]=old;
 return checked.access;
}
bool Selection::complete()const{
 for(unsigned i=0;i<3;++i)if(!selected(Category(i)))return false;
 return quote().access==Access::allowed;
}
}
