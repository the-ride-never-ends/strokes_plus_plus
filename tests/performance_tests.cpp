#include "gestures/recognizer.h"
#include "input/mouse_input_router.h"
#include "test_support.h"
#include <chrono>
#include <string>
#include <utility>

namespace strokes::tests {
namespace {
void recognition_performance(){
    gestures::Stroke stroke;stroke.reserve(128);
    for(int i=0;i<128;++i)stroke.push_back({static_cast<double>(i),static_cast<double>((i*i)%97)});
    gestures::Recognizer recognizer(.75);for(int sample=0;sample<32;++sample){auto varied=stroke;
        for(auto& point:varied)point.y+=sample*.05;(void)recognizer.add_gesture(
            {"gesture-"+std::to_string(sample),"Gesture",true,{{"sample",std::move(varied)}}});}
    constexpr int iterations=1000;const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<iterations;++i)check(recognizer.recognize(stroke).has_value(),"benchmark stroke remains recognized");
    const auto elapsed=std::chrono::steady_clock::now()-start;
    const auto average=std::chrono::duration<double,std::milli>(elapsed).count()/iterations;
    check(average<10.0,"typical recognition remains below 10 ms on average");
}

void input_routing_performance(){
    std::size_t delivered=0;
    input::MouseInputRouter router({input::ActivationButton::right,8.0,{}},
        [&](const input::MouseInputEvent&){++delivered;return true;});
    (void)router.route({input::MouseEventType::button_down,{0,0},input::ActivationButton::right,{}});
    constexpr int iterations=100000;const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<iterations;++i)(void)router.route(
        {input::MouseEventType::pointer_moved,{20.0+static_cast<double>(i%10),0},input::ActivationButton::right,{}});
    const auto elapsed=std::chrono::steady_clock::now()-start;
    const auto average=std::chrono::duration<double,std::milli>(elapsed).count()/iterations;
    check(delivered==static_cast<std::size_t>(iterations+1),"hook-path benchmark routes every event");
    check(average<0.1,"synchronous hook-routing hot path remains substantially below 1 ms per event");
}
}

void run_performance_tests(){recognition_performance();input_routing_performance();}
}  // namespace strokes::tests
