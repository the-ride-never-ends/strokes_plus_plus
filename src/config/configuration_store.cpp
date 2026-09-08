#include "config/configuration_store.h"
#include "config/configuration_codec.h"
#include "config/json.h"
#include <fstream>
#include <sstream>
#include <utility>

namespace strokes::config {
namespace {
bool recover(const std::filesystem::path& path,std::string& error){
    std::error_code ec;auto backup=path;backup+=".bak";
    if(!std::filesystem::exists(path,ec)&&std::filesystem::exists(backup,ec)){
        std::filesystem::rename(backup,path,ec);if(ec){error="cannot recover "+path.string()+": "+ec.message();return false;}
    }return true;
}
bool atomic_write(const std::filesystem::path& path,const std::string& text,std::string& error){
    auto temp=path;temp+=".tmp";auto backup=path;backup+=".bak";std::error_code ec;
    {std::ofstream out(temp,std::ios::binary|std::ios::trunc);if(!out){error="cannot open "+temp.string();return false;}
     out.write(text.data(),static_cast<std::streamsize>(text.size()));out.flush();if(!out){error="cannot write "+temp.string();return false;}}
    const bool existed=std::filesystem::exists(path,ec);
    if(existed){std::filesystem::remove(backup,ec);ec.clear();std::filesystem::rename(path,backup,ec);
      if(ec){std::filesystem::remove(temp);error="cannot stage "+path.string()+": "+ec.message();return false;}}
    std::filesystem::rename(temp,path,ec);
    if(ec){if(existed){std::error_code ignored;std::filesystem::rename(backup,path,ignored);}std::filesystem::remove(temp);
      error="cannot replace "+path.string()+": "+ec.message();return false;}
    std::filesystem::remove(backup,ec);return true;
}
std::optional<json::Value> read_json(const std::filesystem::path& path,std::string& error){
    if(!recover(path,error))return{};std::ifstream in(path,std::ios::binary);if(!in){error="cannot open "+path.string();return{};}
    std::ostringstream data;data<<in.rdbuf();if(!in.good()&&!in.eof()){error="cannot read "+path.string();return{};}
    auto parsed=json::parse(data.str());if(!parsed){error=path.string()+":"+std::to_string(parsed.error->offset)+": "+parsed.error->message;return{};}
    return std::move(*parsed.value);
}
template<class T> bool write_encoded(const std::filesystem::path& path,const T& value,std::string& error){
    return atomic_write(path,json::serialize(encode(value)),error);
}
}

ConfigurationStore::ConfigurationStore(std::filesystem::path directory):directory_(std::move(directory)){}

ConfigurationBundle ConfigurationStore::defaults(){
    ConfigurationBundle result;
    result.gestures.gestures.push_back({"right","Right",true,{{"default-right",{{0,0},{30,0},{60,0},{100,0}}}}});
    result.profiles.global_actions.emplace("right",actions::Action{actions::ActionType::keyboard_shortcut,"ALT+RIGHT"});
    return result;
}

ConfigurationLoadResult ConfigurationStore::load_or_create() const{
    std::error_code ec;std::filesystem::create_directories(directory_,ec);
    if(ec)return{{},"cannot create configuration directory: "+ec.message()};
    auto config_path=directory_/"config.json",gesture_path=directory_/"gestures.json",profile_path=directory_/"profiles.json";
    auto initial=defaults();std::string error;
    if(!std::filesystem::exists(config_path)&&!write_encoded(config_path,initial.global,error))return{{},error};
    if(!std::filesystem::exists(gesture_path)&&!write_encoded(gesture_path,initial.gestures,error))return{{},error};
    if(!std::filesystem::exists(profile_path)&&!write_encoded(profile_path,initial.profiles,error))return{{},error};
    auto config_json=read_json(config_path,error);if(!config_json)return{{},error};auto global=decode_global_options(*config_json);
    if(!global)return{{},config_path.string()+": "+global.error};
    auto gesture_json=read_json(gesture_path,error);if(!gesture_json)return{{},error};auto gestures=decode_gesture_file(*gesture_json);
    if(!gestures)return{{},gesture_path.string()+": "+gestures.error};
    auto profile_json=read_json(profile_path,error);if(!profile_json)return{{},error};auto profiles=decode_profile_file(*profile_json);
    if(!profiles)return{{},profile_path.string()+": "+profiles.error};
    std::string warnings;
    if(!gestures.error.empty())warnings=gesture_path.string()+": "+gestures.error;
    if(!profiles.error.empty()){if(!warnings.empty())warnings+="; ";warnings+=profile_path.string()+": "+profiles.error;}
    return{ConfigurationBundle{std::move(*global.value),std::move(*gestures.value),std::move(*profiles.value)},std::move(warnings)};
}

bool ConfigurationStore::save(const ConfigurationBundle& value,std::string& error) const{
    std::error_code ec;std::filesystem::create_directories(directory_,ec);if(ec){error=ec.message();return false;}
    return write_encoded(directory_/"config.json",value.global,error)&&
           write_encoded(directory_/"gestures.json",value.gestures,error)&&
           write_encoded(directory_/"profiles.json",value.profiles,error);
}
}  // namespace strokes::config
