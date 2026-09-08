#include "config/configuration_store.h"
#include "config/configuration_codec.h"
#include "config/json.h"
#include "test_support.h"
#include <filesystem>
#include <fstream>

namespace strokes::tests {
namespace {
struct TemporaryDirectory {
    std::filesystem::path path=std::filesystem::temp_directory_path()/"strokes-plus-plus-store-tests";
    TemporaryDirectory(){std::error_code ec;std::filesystem::remove_all(path,ec);}
    ~TemporaryDirectory(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
void creation_and_persistence(){
    TemporaryDirectory temp;config::ConfigurationStore store(temp.path);
    auto loaded=store.load_or_create();check(static_cast<bool>(loaded),"missing configuration files are created");
    check(std::filesystem::exists(temp.path/"config.json")&&std::filesystem::exists(temp.path/"gestures.json")&&
          std::filesystem::exists(temp.path/"profiles.json"),"all three configuration files are created");
    loaded.value->global.gestures_enabled=false;loaded.value->global.movement_threshold=17;
    loaded.value->profiles.global_actions.at("right").value="CTRL+W";std::string error;
    loaded.value->gestures.gestures.push_back({"down","Down",true,{{"down-1",{{4,2},{4,20}}}}});
    loaded.value->profiles.profiles.push_back({"notes","Notes",true,
        {{context::ApplicationProperty::process_name,context::MatchMode::contains,"notepad"}},
        {{"down",{actions::ActionType::keyboard_shortcut,"CTRL+S"}}}});
    check(store.save(*loaded.value,error),"configuration bundle saves atomically");
    auto reloaded=store.load_or_create();
    check(reloaded&& !reloaded.value->global.gestures_enabled&&reloaded.value->global.movement_threshold==17,
          "global options persist across reload");
    check(reloaded&&reloaded.value->profiles.global_actions.at("right").value=="CTRL+W",
          "action mappings persist across reload");
    check(reloaded&&reloaded.value->gestures.gestures.size()==2&&
          reloaded.value->gestures.gestures[1].templates[0].points.size()==2,
          "gesture definitions and templates persist across reload");
    check(reloaded&&reloaded.value->profiles.profiles.size()==1&&
          reloaded.value->profiles.profiles[0].actions_by_gesture.at("down").value=="CTRL+S",
          "profiles, criteria, and overrides persist across reload");
}
void malformed_and_recovery(){
    TemporaryDirectory temp;config::ConfigurationStore store(temp.path);auto loaded=store.load_or_create();
    {std::ofstream out(temp.path/"config.json",std::ios::trunc);out<<"{bad";}auto bad=store.load_or_create();
    check(!bad&&bad.error.find("config.json")!=std::string::npos,"malformed configuration reports its file");
    std::filesystem::remove(temp.path/"config.json");
    {std::ofstream out(temp.path/"config.json.bak",std::ios::trunc);out<<config::json::serialize(config::encode(loaded.value->global));}
    auto recovered=store.load_or_create();check(static_cast<bool>(recovered),"interrupted-save backup is recovered");
}
}
void run_configuration_store_tests(){creation_and_persistence();malformed_and_recovery();}
}  // namespace strokes::tests
