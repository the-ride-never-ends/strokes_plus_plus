#include "context/windows_application_context.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "overlay/windows_gesture_overlay.h"

#define PSAPI_VERSION 1
#include <Psapi.h>
#include <Windows.h>
#include <cstdint>
#include <iostream>

namespace {
int failures=0;
void check(bool condition,const char* message){if(!condition){std::cerr<<"FAIL: "<<message<<'\n';++failures;}}
std::uint64_t ticks(const FILETIME& value){return(static_cast<std::uint64_t>(value.dwHighDateTime)<<32)|value.dwLowDateTime;}
bool pass_mouse(const strokes::input::MouseInputEvent&,void*) noexcept{return false;}
bool pass_escape(void*) noexcept{return false;}
}

int main(){
    using namespace strokes;
    const auto right=input::WindowsMouseClick::make_input_sequence(input::ActivationButton::right);
    check(right[0].type==INPUT_MOUSE&&right[0].mi.dwFlags==MOUSEEVENTF_RIGHTDOWN,
          "right click begins with native right-button down");
    check(right[1].type==INPUT_MOUSE&&right[1].mi.dwFlags==MOUSEEVENTF_RIGHTUP,
          "right click ends with native right-button up");

    bool suppress_up=false;KBDLLHOOKSTRUCT escape{};escape.vkCode=VK_ESCAPE;
    check(input::WindowsKeyboardHook::filter_escape_event(WM_KEYDOWN,escape,true,suppress_up)&&suppress_up,
          "handled Escape down is suppressed");
    check(input::WindowsKeyboardHook::filter_escape_event(WM_KEYDOWN,escape,false,suppress_up)&&suppress_up,
          "repeated Escape down remains suppressed without repeating cancellation");
    check(input::WindowsKeyboardHook::filter_escape_event(WM_KEYUP,escape,false,suppress_up)&&!suppress_up,
          "matching Escape up is suppressed and balanced");
    escape.flags=LLKHF_INJECTED;
    check(!input::WindowsKeyboardHook::filter_escape_event(WM_KEYDOWN,escape,true,suppress_up),
          "injected Escape is ignored");

    overlay::WindowsGestureOverlay overlay;
    check(overlay.create(::GetModuleHandleW(nullptr),{false,4,217,RGB(0,160,255)}),
          "disabled Win32 overlay can be created without showing UI");
    overlay.destroy();
    context::WindowsApplicationContextProvider context;
    check(context.foreground_application().has_value(),"foreground Win32 application context can be captured");

    input::WindowsMouseHook mouse_hook;input::WindowsKeyboardHook keyboard_hook;
    check(mouse_hook.start(pass_mouse,nullptr),"low-level mouse hook installs");
    check(keyboard_hook.start(pass_escape,nullptr),"low-level keyboard hook installs");
    keyboard_hook.stop();mouse_hook.stop();
    check(!mouse_hook.running()&&!keyboard_hook.running(),"low-level hooks uninstall cleanly");

    FILETIME created{},exited{},kernel_before{},user_before{},kernel_after{},user_after{};
    check(::GetProcessTimes(::GetCurrentProcess(),&created,&exited,&kernel_before,&user_before)!=FALSE,
          "initial process CPU time is readable");
    ::Sleep(500);
    check(::GetProcessTimes(::GetCurrentProcess(),&created,&exited,&kernel_after,&user_after)!=FALSE,
          "final process CPU time is readable");
    const double cpu_ms=static_cast<double>((ticks(kernel_after)-ticks(kernel_before))+
                                            (ticks(user_after)-ticks(user_before)))/10000.0;
    PROCESS_MEMORY_COUNTERS counters{};counters.cb=sizeof(counters);
    check(::GetProcessMemoryInfo(::GetCurrentProcess(),&counters,sizeof(counters))!=FALSE,
          "working-set usage is readable");
    check(cpu_ms<25.0,"idle CPU remains effectively zero over the sampling interval");
    check(counters.WorkingSetSize<50ull*1024ull*1024ull,"idle working set remains below 50 MB");
    return failures==0?0:1;
}
