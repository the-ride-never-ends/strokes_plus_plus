#include "gestures/gesture_repository.h"
#include <algorithm>
#include <utility>

namespace strokes::gestures {
bool GestureRepository::unique_id(const std::string& id)const{return !id.empty()&&std::ranges::none_of(gestures_,[&](auto&g){return g.id==id;});}
bool GestureRepository::unique_name(const std::string& name,const std::string* except)const{
    return !name.empty()&&std::ranges::none_of(gestures_,[&](auto&g){return g.name==name&&(!except||g.id!=*except);});}
GestureDefinition* GestureRepository::find(const std::string& id)noexcept{
    auto it=std::ranges::find_if(gestures_,[&](auto&g){return g.id==id;});return it==gestures_.end()?nullptr:&*it;}
bool GestureRepository::create(std::string id,std::string name){
    if(!unique_id(id)||!unique_name(name))return false;gestures_.push_back({std::move(id),std::move(name),false,{}});return true;}
bool GestureRepository::rename(const std::string& id,std::string name){
    auto* g=find(id);if(!g||!unique_name(name,&id))return false;g->name=std::move(name);return true;}
bool GestureRepository::set_enabled(const std::string& id,bool enabled){
    auto* g=find(id);if(!g||enabled&&g->templates.empty())return false;g->enabled=enabled;return true;}
bool GestureRepository::erase(const std::string& id){
    return std::erase_if(gestures_,[&](auto&g){return g.id==id;})==1;}
bool GestureRepository::add_template(const std::string& id,GestureTemplate value){
    auto* g=find(id);if(!g||value.id.empty()||value.points.size()<2||
      std::ranges::any_of(g->templates,[&](auto&t){return t.id==value.id;}))return false;
    g->templates.push_back(std::move(value));return true;}
bool GestureRepository::remove_template(const std::string& id,const std::string& template_id){
    auto* g=find(id);if(!g)return false;const bool removed=std::erase_if(g->templates,[&](auto&t){return t.id==template_id;})==1;
    if(removed&&g->templates.empty())g->enabled=false;return removed;}
}  // namespace strokes::gestures
