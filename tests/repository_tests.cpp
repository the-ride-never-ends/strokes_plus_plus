#include "gestures/gesture_repository.h"
#include "context/profile_repository.h"
#include "test_support.h"

namespace strokes::tests {
namespace {
void gesture_repository_tests(){
    std::vector<gestures::GestureDefinition> values;gestures::GestureRepository repo(values);
    check(repo.create("left","Left"),"gesture is created with unique identity");
    check(!values[0].enabled,"untrained gesture starts disabled");
    check(!repo.create("left","Another"),"duplicate gesture id is rejected");
    check(!repo.create("other","Left"),"duplicate gesture name is rejected");
    check(repo.rename("left","Go Left")&&values[0].id=="left","rename preserves gesture identity");
    check(!repo.set_enabled("left",true),"gesture cannot be enabled before training");
    check(repo.add_template("left",{"sample",{{0,0},{10,0}}}),"valid training template is added");
    check(!repo.add_template("left",{"sample",{{0,0},{10,0}}}),"duplicate template id is rejected");
    check(repo.set_enabled("left",true),"trained gesture can be enabled");
    check(repo.remove_template("left","sample")&&!values[0].enabled,"removing last template disables gesture");
    check(repo.erase("left")&&values.empty(),"gesture is deleted");
}
void profile_repository_tests(){
    std::vector<context::ApplicationProfile> values;context::ProfileRepository repo(values);
    check(repo.create("chrome","Chrome"),"profile is created");
    check(!values[0].enabled,"criterion-free profile starts disabled");
    check(!repo.set_enabled("chrome",true),"profile cannot be enabled without criteria");
    check(repo.add_criterion("chrome",{context::ApplicationProperty::process_name,context::MatchMode::exact,"chrome.exe"}),
          "profile criterion is added");
    check(repo.set_enabled("chrome",true),"profile with criterion can be enabled");
    check(repo.set_action("chrome","left",{actions::ActionType::keyboard_shortcut,"ALT+LEFT"}),"profile action is assigned");
    check(values[0].actions_by_gesture.at("left").value=="ALT+LEFT","assigned action is retained");
    check(repo.rename("chrome","Google Chrome")&&values[0].id=="chrome","profile rename preserves identity");
    check(repo.remove_action("chrome","left"),"profile action is removed");
    check(repo.erase("chrome")&&values.empty(),"profile is deleted");
}
}
void run_repository_tests(){gesture_repository_tests();profile_repository_tests();}
}  // namespace strokes::tests
