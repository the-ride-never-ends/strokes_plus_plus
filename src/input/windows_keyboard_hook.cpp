#include "input/windows_keyboard_hook.h"

namespace strokes::input {
std::atomic<WindowsKeyboardHook*> WindowsKeyboardHook::active_hook_{nullptr};
WindowsKeyboardHook::~WindowsKeyboardHook(){stop();}

bool WindowsKeyboardHook::start(EscapeHandler handler,void* context) noexcept {
    if(running()||handler==nullptr)return false;
    WindowsKeyboardHook* expected=nullptr;
    if(!active_hook_.compare_exchange_strong(expected,this,std::memory_order_acq_rel))return false;
    handler_=handler;context_=context;
    hook_=::SetWindowsHookExW(WH_KEYBOARD_LL,hook_callback,::GetModuleHandleW(nullptr),0);
    if(hook_==nullptr){handler_=nullptr;context_=nullptr;active_hook_.store(nullptr,std::memory_order_release);return false;}
    return true;
}

void WindowsKeyboardHook::stop() noexcept {
    if(hook_!=nullptr){::UnhookWindowsHookEx(hook_);hook_=nullptr;}
    WindowsKeyboardHook* expected=this;
    active_hook_.compare_exchange_strong(expected,nullptr,std::memory_order_acq_rel);
    handler_=nullptr;context_=nullptr;
    suppress_escape_up_=false;
}

LRESULT CALLBACK WindowsKeyboardHook::hook_callback(int code,WPARAM message,LPARAM data) noexcept {
    if(code==HC_ACTION&&data!=0){
        const auto& native=*reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if(auto* hook=active_hook_.load(std::memory_order_acquire);hook){
            const bool is_down=message==WM_KEYDOWN||message==WM_SYSKEYDOWN;
            const bool eligible=native.vkCode==VK_ESCAPE&&
                (native.flags&(LLKHF_INJECTED|LLKHF_LOWER_IL_INJECTED))==0;
            const bool handled=eligible&&is_down&&!hook->suppress_escape_up_&&hook->handler_&&
                hook->handler_(hook->context_);
            if(filter_escape_event(message,native,handled,hook->suppress_escape_up_))return 1;
        }
    }
    return ::CallNextHookEx(nullptr,code,message,data);
}

bool WindowsKeyboardHook::filter_escape_event(WPARAM message,const KBDLLHOOKSTRUCT& native,
                                               bool handler_result,bool& suppress_key_up) noexcept {
    if(native.vkCode!=VK_ESCAPE||(native.flags&(LLKHF_INJECTED|LLKHF_LOWER_IL_INJECTED))!=0)return false;
    if((message==WM_KEYUP||message==WM_SYSKEYUP)&&suppress_key_up){suppress_key_up=false;return true;}
    if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN){
        if(suppress_key_up)return true;
        if(handler_result){suppress_key_up=true;return true;}
    }
    return false;
}
}  // namespace strokes::input
