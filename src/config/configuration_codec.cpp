#include "config/configuration_codec.h"
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_set>

namespace strokes::config {
namespace {
using json::Array; using json::Object; using json::Value;
const Value* field(const Object& o, std::string_view n) { auto i=o.find(n); return i==o.end()?nullptr:&i->second; }
template<class T> const T* field_as(const Object& o, std::string_view n) { auto* v=field(o,n); return v?v->get_if<T>():nullptr; }
std::optional<int> integer(const Object& o, std::string_view n) {
    auto* v=field_as<double>(o,n);
    if(!v || std::floor(*v)!=*v || *v<std::numeric_limits<int>::min() || *v>std::numeric_limits<int>::max()) return {};
    return static_cast<int>(*v);
}
std::string button(input::ActivationButton v) {
    switch(v){case input::ActivationButton::right:return "right";case input::ActivationButton::middle:return "middle";
    case input::ActivationButton::x_button_1:return "xbutton1";case input::ActivationButton::x_button_2:return "xbutton2";} return {};
}
std::optional<input::ActivationButton> button(std::string_view v) {
    if(v=="right")return input::ActivationButton::right;if(v=="middle")return input::ActivationButton::middle;
    if(v=="xbutton1")return input::ActivationButton::x_button_1;if(v=="xbutton2")return input::ActivationButton::x_button_2;return {};
}
std::string property(context::ApplicationProperty v) {
    switch(v){case context::ApplicationProperty::process_name:return "process";case context::ApplicationProperty::window_title:return "title";
    case context::ApplicationProperty::window_class:return "class";} return {};
}
std::optional<context::ApplicationProperty> property(std::string_view v) {
    if(v=="process")return context::ApplicationProperty::process_name;if(v=="title")return context::ApplicationProperty::window_title;
    if(v=="class")return context::ApplicationProperty::window_class;return {};
}
std::string mode(context::MatchMode v) {
    switch(v){case context::MatchMode::exact:return "exact";case context::MatchMode::contains:return "contains";case context::MatchMode::regex:return "regex";} return {};
}
std::optional<context::MatchMode> mode(std::string_view v) {
    if(v=="exact")return context::MatchMode::exact;if(v=="contains")return context::MatchMode::contains;if(v=="regex")return context::MatchMode::regex;return {};
}
Value action(const actions::Action& a){return Object{{"type","keyboard"},{"shortcut",a.value}};}
std::optional<actions::Action> action(const Value& v){
    auto* o=v.get_if<Object>(); if(!o)return{}; auto* t=field_as<std::string>(*o,"type");auto* s=field_as<std::string>(*o,"shortcut");
    if(!t||*t!="keyboard"||!s||s->empty())return{};return actions::Action{actions::ActionType::keyboard_shortcut,*s};
}
Object actions_out(const actions::ActionResolver::GlobalActions& m){Object o;for(auto&[k,v]:m)o.emplace(k,action(v));return o;}
bool actions_in(const Value& v,actions::ActionResolver::GlobalActions& m,std::size_t& skipped){
    auto* o=v.get_if<Object>();if(!o)return false;for(auto&[k,x]:*o){auto a=action(x);if(k.empty()||!a){++skipped;continue;}m.emplace(k,std::move(*a));}return true;
}
}

Value encode(const GlobalOptions& c){
    return Object{{"version",double(c.version)},{"gestures_enabled",c.gestures_enabled},{"gesture_button",button(c.gesture_button)},
      {"movement_threshold",c.movement_threshold},{"minimum_point_distance",c.minimum_point_distance},{"maximum_points",double(c.maximum_points)},
      {"recognition_threshold",c.recognition_threshold},{"overlay",Object{{"enabled",c.overlay.enabled},{"line_width",double(c.overlay.line_width)},
      {"opacity",c.overlay.opacity},{"color",double(c.overlay.color)}}}};
}

DecodeResult<GlobalOptions> decode_global_options(const Value& v){
    auto* o=v.get_if<Object>();if(!o)return{{},"global configuration must be an object"};
    auto ver=integer(*o,"version"),max=integer(*o,"maximum_points");auto* en=field_as<bool>(*o,"gestures_enabled");
    auto* b=field_as<std::string>(*o,"gesture_button");auto parsed_button=b?button(*b):std::nullopt;
    auto* move=field_as<double>(*o,"movement_threshold");auto* dist=field_as<double>(*o,"minimum_point_distance");
    auto* threshold=field_as<double>(*o,"recognition_threshold");auto* ov=field(*o,"overlay");auto* oo=ov?ov->get_if<Object>():nullptr;
    if(!ver||*ver!=1||!max||*max<2||!en||!parsed_button||!move||*move<0||!dist||*dist<0||!threshold||*threshold<0||*threshold>1||!oo)
        return{{},"global configuration has missing or invalid fields"};
    auto* oe=field_as<bool>(*oo,"enabled");auto width=integer(*oo,"line_width");auto* opacity=field_as<double>(*oo,"opacity");auto color=integer(*oo,"color");
    if(!oe||!width||*width<=0||!opacity||*opacity<0||*opacity>1||!color||*color<0||*color>0xFFFFFF)return{{},"overlay configuration is invalid"};
    GlobalOptions r; r.version=*ver;r.gestures_enabled=*en;r.gesture_button=*parsed_button;r.movement_threshold=*move;
    r.minimum_point_distance=*dist;r.maximum_points=static_cast<std::size_t>(*max);r.recognition_threshold=*threshold;
    r.overlay={*oe,*width,*opacity,static_cast<std::uint32_t>(*color)};return{r,{}};
}

Value encode(const GestureFile& f){
    Array gs;for(auto&g:f.gestures){Array ts;for(auto&t:g.templates){Array ps;for(auto&p:t.points)ps.emplace_back(Object{{"x",p.x},{"y",p.y}});
    ts.emplace_back(Object{{"id",t.id},{"points",std::move(ps)}});}gs.emplace_back(Object{{"id",g.id},{"name",g.name},{"enabled",g.enabled},{"templates",std::move(ts)}});}
    return Object{{"version",double(f.version)},{"gestures",std::move(gs)}};
}

DecodeResult<GestureFile> decode_gesture_file(const Value& v){
    auto* o=v.get_if<Object>();if(!o)return{{},"gesture file must be an object"};auto ver=integer(*o,"version");auto* x=field(*o,"gestures");auto* gs=x?x->get_if<Array>():nullptr;
    if(!ver||*ver!=1||!gs)return{{},"gesture file has missing or invalid fields"};GestureFile r;std::size_t skipped=0;std::unordered_set<std::string> ids;
    for(auto&gv:*gs){auto* g=gv.get_if<Object>();if(!g){++skipped;continue;}auto* id=field_as<std::string>(*g,"id");auto* name=field_as<std::string>(*g,"name");
      auto* en=field_as<bool>(*g,"enabled");auto* tx=field(*g,"templates");auto* ts=tx?tx->get_if<Array>():nullptr;
      if(!id||id->empty()||!name||name->empty()||!en||!ts||ids.contains(*id)){++skipped;continue;}gestures::GestureDefinition gd{*id,*name,*en,{}};
      for(auto&tv:*ts){auto* t=tv.get_if<Object>();auto* tid=t?field_as<std::string>(*t,"id"):nullptr;auto* px=t?field(*t,"points"):nullptr;auto* ps=px?px->get_if<Array>():nullptr;
        if(!tid||tid->empty()||!ps||ps->size()<2){++skipped;continue;}gestures::Stroke stroke;bool valid=true;
        for(auto&pv:*ps){auto* p=pv.get_if<Object>();auto* xx=p?field_as<double>(*p,"x"):nullptr;auto* yy=p?field_as<double>(*p,"y"):nullptr;
          if(!xx||!yy||!std::isfinite(*xx)||!std::isfinite(*yy)){valid=false;break;}stroke.push_back({*xx,*yy});}
        if(valid)gd.templates.push_back({*tid,std::move(stroke)});else ++skipped;}
      if(*en&&gd.templates.empty()){++skipped;continue;}ids.insert(*id);r.gestures.push_back(std::move(gd));}
    return{std::move(r),skipped?"skipped "+std::to_string(skipped)+" invalid gesture or template entries":std::string{}};
}

Value encode(const ProfileFile& f){
    Array ps;for(auto&p:f.profiles){Array cs;for(auto&c:p.criteria)cs.emplace_back(Object{{"property",property(c.property)},{"mode",mode(c.mode)},{"value",c.value}});
      ps.emplace_back(Object{{"id",p.id},{"name",p.name},{"enabled",p.enabled},{"criteria",std::move(cs)},{"actions",actions_out(p.actions_by_gesture)}});}
    return Object{{"version",double(f.version)},{"profiles",std::move(ps)},{"global_actions",actions_out(f.global_actions)}};
}

DecodeResult<ProfileFile> decode_profile_file(const Value& v){
    auto* o=v.get_if<Object>();if(!o)return{{},"profile file must be an object"};auto ver=integer(*o,"version");auto* px=field(*o,"profiles");auto* ps=px?px->get_if<Array>():nullptr;auto* ga=field(*o,"global_actions");
    if(!ver||*ver!=1||!ps||!ga)return{{},"profile file has missing or invalid fields"};ProfileFile r;std::size_t skipped=0;std::unordered_set<std::string> ids;if(!actions_in(*ga,r.global_actions,skipped))return{{},"global actions are invalid"};
    for(auto&pv:*ps){auto* p=pv.get_if<Object>();auto* id=p?field_as<std::string>(*p,"id"):nullptr;auto* name=p?field_as<std::string>(*p,"name"):nullptr;
      auto* en=p?field_as<bool>(*p,"enabled"):nullptr;auto* cx=p?field(*p,"criteria"):nullptr;auto* cs=cx?cx->get_if<Array>():nullptr;auto* ax=p?field(*p,"actions"):nullptr;
      if(!id||id->empty()||!name||name->empty()||!en||!cs||cs->empty()||!ax||ids.contains(*id)){++skipped;continue;}context::ApplicationProfile ap{*id,*name,*en,{},{}};bool valid=true;
      for(auto&cv:*cs){auto* c=cv.get_if<Object>();auto* pp=c?field_as<std::string>(*c,"property"):nullptr;auto* mm=c?field_as<std::string>(*c,"mode"):nullptr;auto* vv=c?field_as<std::string>(*c,"value"):nullptr;
        auto pval=pp?property(*pp):std::nullopt;auto mval=mm?mode(*mm):std::nullopt;if(!pval||!mval||!vv||vv->empty()){valid=false;break;}ap.criteria.push_back({*pval,*mval,*vv});}
      if(!valid){++skipped;continue;}if(!actions_in(*ax,ap.actions_by_gesture,skipped)){++skipped;continue;}ids.insert(*id);r.profiles.push_back(std::move(ap));}
    return{std::move(r),skipped?"skipped "+std::to_string(skipped)+" invalid profile, criterion, or action entries":std::string{}};
}
}  // namespace strokes::config
