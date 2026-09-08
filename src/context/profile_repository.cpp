#include "context/profile_repository.h"
#include <algorithm>
#include <utility>

namespace strokes::context {
ApplicationProfile* ProfileRepository::find(const std::string& id)noexcept{
    auto it=std::ranges::find_if(profiles_,[&](auto&p){return p.id==id;});return it==profiles_.end()?nullptr:&*it;}
bool ProfileRepository::create(std::string id,std::string name){
    if(id.empty()||name.empty()||std::ranges::any_of(profiles_,[&](auto&p){return p.id==id||p.name==name;}))return false;
    profiles_.push_back({std::move(id),std::move(name),false,{},{}});return true;}
bool ProfileRepository::rename(const std::string& id,std::string name){
    auto* p=find(id);if(!p||name.empty()||std::ranges::any_of(profiles_,[&](auto&x){return x.name==name&&x.id!=id;}))return false;
    p->name=std::move(name);return true;}
bool ProfileRepository::set_enabled(const std::string& id,bool enabled){
    auto* p=find(id);if(!p||enabled&&p->criteria.empty())return false;p->enabled=enabled;return true;}
bool ProfileRepository::erase(const std::string& id){return std::erase_if(profiles_,[&](auto&p){return p.id==id;})==1;}
bool ProfileRepository::add_criterion(const std::string& id,MatchCriterion criterion){
    auto* p=find(id);if(!p||criterion.value.empty())return false;p->criteria.push_back(std::move(criterion));return true;}
bool ProfileRepository::set_action(const std::string& id,std::string gesture_id,actions::Action action){
    auto* p=find(id);if(!p||gesture_id.empty()||action.value.empty())return false;p->actions_by_gesture.insert_or_assign(std::move(gesture_id),std::move(action));return true;}
bool ProfileRepository::remove_action(const std::string& id,const std::string& gesture_id){
    auto* p=find(id);return p&&p->actions_by_gesture.erase(gesture_id)==1;}
}  // namespace strokes::context
