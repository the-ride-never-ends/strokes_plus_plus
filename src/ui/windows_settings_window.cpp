#include "ui/windows_settings_window.h"
#include "actions/keyboard_shortcut.h"
#include "gestures/gesture_repository.h"
#include "context/profile_repository.h"
#include "ui/windows_gesture_trainer.h"
#include <cwchar>
#include <string>
#include <utility>

namespace strokes::ui {
namespace {
constexpr wchar_t class_name[]=L"StrokesPlusPlusSettingsWindow";
enum : int { enabled_id=101,button_id,move_id,distance_id,max_id,threshold_id,overlay_id,width_id,opacity_id,
             shortcut_id,gestures_id,gesture_name_id,gesture_add_id,gesture_rename_id,gesture_delete_id,
             gesture_train_id,gesture_toggle_id,profiles_id,profile_name_id,profile_property_id,profile_mode_id,
             profile_value_id,profile_shortcut_id,
             profile_add_id,profile_update_id,profile_delete_id,profile_assign_id,global_assign_id,save_id,cancel_id };
void text(HWND parent,int id,const wchar_t* value,int x,int y,int w=170,int h=22){
    ::CreateWindowExW(0,L"STATIC",value,WS_CHILD|WS_VISIBLE,x,y,w,h,parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
}
HWND control(HWND parent,const wchar_t* type,const wchar_t* value,DWORD style,int id,int x,int y,int w,int h){
    return ::CreateWindowExW(WS_EX_CLIENTEDGE,type,value,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
}
std::wstring number(double value){wchar_t out[64]{};swprintf_s(out,L"%.6g",value);return out;}
std::wstring integer(std::size_t value){return std::to_wstring(value);}
bool read_double(HWND window,int id,double low,double high,double& output){
    wchar_t value[128]{};::GetDlgItemTextW(window,id,value,128);wchar_t* end=nullptr;const double parsed=std::wcstod(value,&end);
    if(end==value||*end!=L'\0'||parsed<low||parsed>high)return false;output=parsed;return true;
}
bool read_integer(HWND window,int id,long low,long high,long& output){
    wchar_t value[128]{};::GetDlgItemTextW(window,id,value,128);wchar_t* end=nullptr;const long parsed=std::wcstol(value,&end,10);
    if(end==value||*end!=L'\0'||parsed<low||parsed>high)return false;output=parsed;return true;
}
std::string utf8_from_control(HWND window,int id){
    wchar_t value[256]{};::GetDlgItemTextW(window,id,value,256);const int length=static_cast<int>(std::wcslen(value));
    if(length==0)return{};const int needed=::WideCharToMultiByte(CP_UTF8,0,value,length,nullptr,0,nullptr,nullptr);
    std::string result(static_cast<std::size_t>(needed),'\0');::WideCharToMultiByte(CP_UTF8,0,value,length,result.data(),needed,nullptr,nullptr);return result;
}
std::wstring wide(std::string_view value){
    if(value.empty())return{};const int needed=::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    std::wstring result(static_cast<std::size_t>(needed),L'\0');::MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),needed);return result;
}
}

bool WindowsSettingsWindow::show(HINSTANCE instance,config::ConfigurationBundle& configuration){
    instance_=instance;destination_=&configuration;working_=configuration;configuration_=&working_;accepted_=false;finished_=false;
    WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=window_proc;wc.hInstance=instance_;wc.lpszClassName=class_name;
    wc.hCursor=::LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(::RegisterClassExW(&wc)==0&&::GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    window_=::CreateWindowExW(WS_EX_APPWINDOW,class_name,L"Strokes++ Settings",
        WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,680,760,
        nullptr,nullptr,instance_,this);
    if(!window_)return false;
    ::ShowWindow(window_,SW_SHOW);::UpdateWindow(window_);
    MSG message{};
    while(!finished_&&::GetMessageW(&message,nullptr,0,0)>0){::TranslateMessage(&message);::DispatchMessageW(&message);}
    if(message.message==WM_QUIT)::PostQuitMessage(static_cast<int>(message.wParam));
    if(accepted_)*destination_=std::move(working_);
    return accepted_;
}

LRESULT CALLBACK WindowsSettingsWindow::window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    auto* self=reinterpret_cast<WindowsSettingsWindow*>(::GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<WindowsSettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
      self->window_=window;::SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    return self ? self->handle_message(message,wp,lp) : ::DefWindowProcW(window,message,wp,lp);
}

LRESULT WindowsSettingsWindow::handle_message(UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_CREATE){create_controls();load_values();return 0;}
    if(message==WM_COMMAND){
        if(LOWORD(wp)==save_id){if(save_values()){accepted_=true;::DestroyWindow(window_);}return 0;}
        if(LOWORD(wp)==cancel_id){::DestroyWindow(window_);return 0;}
        if(LOWORD(wp)==gesture_add_id){add_gesture();return 0;}
        if(LOWORD(wp)==gesture_rename_id){rename_gesture();return 0;}
        if(LOWORD(wp)==gesture_delete_id){delete_gesture();return 0;}
        if(LOWORD(wp)==gesture_train_id){train_gesture();return 0;}
        if(LOWORD(wp)==gesture_toggle_id){toggle_gesture();return 0;}
        if(LOWORD(wp)==profile_add_id){add_profile();return 0;}
        if(LOWORD(wp)==profile_update_id){update_profile();return 0;}
        if(LOWORD(wp)==profile_delete_id){delete_profile();return 0;}
        if(LOWORD(wp)==profile_assign_id){assign_profile_action();return 0;}
        if(LOWORD(wp)==global_assign_id){assign_global_action();return 0;}
        if(LOWORD(wp)==gestures_id&&HIWORD(wp)==LBN_SELCHANGE){load_selected_gesture();return 0;}
        if(LOWORD(wp)==profiles_id&&HIWORD(wp)==LBN_SELCHANGE){load_selected_profile();return 0;}
    }
    if(message==WM_CLOSE){::DestroyWindow(window_);return 0;}
    if(message==WM_DESTROY){window_=nullptr;finished_=true;return 0;}
    return ::DefWindowProcW(window_,message,wp,lp);
}

void WindowsSettingsWindow::create_controls(){
    text(window_,0,L"Global settings",16,12,220); 
    control(window_,L"BUTTON",L"Gestures enabled",BS_AUTOCHECKBOX,enabled_id,20,42,180,24);
    text(window_,0,L"Activation button",20,76);control(window_,L"COMBOBOX",L"",CBS_DROPDOWNLIST,button_id,200,72,180,180);
    for(auto* value:{L"Right",L"Middle",L"XButton1",L"XButton2"})::SendDlgItemMessageW(window_,button_id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    text(window_,0,L"Movement threshold",20,110);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,move_id,200,106,100,24);
    text(window_,0,L"Point distance",20,144);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,distance_id,200,140,100,24);
    text(window_,0,L"Maximum points",20,178);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,max_id,200,174,100,24);
    text(window_,0,L"Recognition threshold",20,212);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,threshold_id,200,208,100,24);
    control(window_,L"BUTTON",L"Overlay enabled",BS_AUTOCHECKBOX,overlay_id,20,246,180,24);
    text(window_,0,L"Overlay line width",20,280);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,width_id,200,276,100,24);
    text(window_,0,L"Overlay opacity (0-1)",20,314);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,opacity_id,200,310,100,24);
    text(window_,0,L"Selected gesture global shortcut",20,348);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,shortcut_id,230,344,110,24);
    control(window_,L"BUTTON",L"Assign",BS_PUSHBUTTON,global_assign_id,344,344,60,24);
    text(window_,0,L"Gestures",410,12);control(window_,L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL,gestures_id,400,38,220,150);
    control(window_,L"EDIT",L"",ES_AUTOHSCROLL,gesture_name_id,400,194,220,24);
    control(window_,L"BUTTON",L"Add",BS_PUSHBUTTON,gesture_add_id,400,224,50,26);
    control(window_,L"BUTTON",L"Rename",BS_PUSHBUTTON,gesture_rename_id,454,224,62,26);
    control(window_,L"BUTTON",L"Delete",BS_PUSHBUTTON,gesture_delete_id,520,224,50,26);
    control(window_,L"BUTTON",L"Train",BS_PUSHBUTTON,gesture_train_id,574,224,50,26);
    control(window_,L"BUTTON",L"Enable / Disable",BS_PUSHBUTTON,gesture_toggle_id,500,254,124,26);
    text(window_,0,L"Application profiles",410,294);control(window_,L"LISTBOX",L"",LBS_NOTIFY|WS_VSCROLL,profiles_id,400,320,240,100);
    text(window_,0,L"Profile name",400,426,100);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,profile_name_id,500,422,140,24);
    text(window_,0,L"Match field",400,456,100);control(window_,L"COMBOBOX",L"",CBS_DROPDOWNLIST,profile_property_id,500,452,140,120);
    for(auto* value:{L"Process",L"Window title",L"Window class"})::SendDlgItemMessageW(window_,profile_property_id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    ::SendDlgItemMessageW(window_,profile_property_id,CB_SETCURSEL,0,0);
    text(window_,0,L"Match mode",400,486,100);control(window_,L"COMBOBOX",L"",CBS_DROPDOWNLIST,profile_mode_id,500,482,140,120);
    for(auto* value:{L"Exact",L"Contains",L"Regular expression"})::SendDlgItemMessageW(window_,profile_mode_id,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(value));
    ::SendDlgItemMessageW(window_,profile_mode_id,CB_SETCURSEL,0,0);
    text(window_,0,L"Match value",400,516,100);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,profile_value_id,500,512,140,24);
    control(window_,L"BUTTON",L"Add",BS_PUSHBUTTON,profile_add_id,400,542,60,26);
    control(window_,L"BUTTON",L"Update",BS_PUSHBUTTON,profile_update_id,464,542,70,26);
    control(window_,L"BUTTON",L"Delete",BS_PUSHBUTTON,profile_delete_id,538,542,70,26);
    text(window_,0,L"Override shortcut",400,580,100);control(window_,L"EDIT",L"",ES_AUTOHSCROLL,profile_shortcut_id,500,576,140,24);
    control(window_,L"BUTTON",L"Assign override",BS_PUSHBUTTON,profile_assign_id,500,606,140,26);
    control(window_,L"BUTTON",L"Save",BS_DEFPUSHBUTTON,save_id,450,670,90,30);
    control(window_,L"BUTTON",L"Cancel",BS_PUSHBUTTON,cancel_id,550,670,90,30);
}

void WindowsSettingsWindow::load_values(){
    auto& c=configuration_->global;::CheckDlgButton(window_,enabled_id,c.gestures_enabled?BST_CHECKED:BST_UNCHECKED);
    ::SendDlgItemMessageW(window_,button_id,CB_SETCURSEL,static_cast<WPARAM>(c.gesture_button),0);
    ::SetDlgItemTextW(window_,move_id,number(c.movement_threshold).c_str());::SetDlgItemTextW(window_,distance_id,number(c.minimum_point_distance).c_str());
    ::SetDlgItemTextW(window_,max_id,integer(c.maximum_points).c_str());::SetDlgItemTextW(window_,threshold_id,number(c.recognition_threshold).c_str());
    ::CheckDlgButton(window_,overlay_id,c.overlay.enabled?BST_CHECKED:BST_UNCHECKED);::SetDlgItemTextW(window_,width_id,integer(c.overlay.line_width).c_str());
    ::SetDlgItemTextW(window_,opacity_id,number(c.overlay.opacity).c_str());
    refresh_gestures();
    for(auto& p:configuration_->profiles.profiles){std::wstring name(p.name.begin(),p.name.end());::SendDlgItemMessageW(window_,profiles_id,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(name.c_str()));}
    if(!configuration_->gestures.gestures.empty()){::SendDlgItemMessageW(window_,gestures_id,LB_SETCURSEL,0,0);load_selected_gesture();}
}

void WindowsSettingsWindow::refresh_gestures(){
    ::SendDlgItemMessageW(window_,gestures_id,LB_RESETCONTENT,0,0);
    for(const auto& gesture:configuration_->gestures.gestures){
        std::wstring label=wide(gesture.name);
        label+=gesture.enabled?L" [enabled]":L" [disabled]";
        label+=L" - "+std::to_wstring(gesture.templates.size())+L" sample(s)";
        ::SendDlgItemMessageW(window_,gestures_id,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
    }
}

int WindowsSettingsWindow::selected_gesture() const noexcept {
    const LRESULT selected=::SendDlgItemMessageW(window_,gestures_id,LB_GETCURSEL,0,0);
    return selected==LB_ERR?-1:static_cast<int>(selected);
}

int WindowsSettingsWindow::selected_profile() const noexcept {
    const LRESULT selected=::SendDlgItemMessageW(window_,profiles_id,LB_GETCURSEL,0,0);
    return selected==LB_ERR?-1:static_cast<int>(selected);
}

void WindowsSettingsWindow::load_selected_gesture(){
    const int selected=selected_gesture();if(selected<0)return;const auto& gesture=configuration_->gestures.gestures[static_cast<std::size_t>(selected)];
    ::SetDlgItemTextW(window_,gesture_name_id,wide(gesture.name).c_str());auto action=configuration_->profiles.global_actions.find(gesture.id);
    ::SetDlgItemTextW(window_,shortcut_id,action==configuration_->profiles.global_actions.end()?L"":wide(action->second.value).c_str());
    load_selected_profile();
}

void WindowsSettingsWindow::load_selected_profile(){
    const int selected=selected_profile();if(selected<0)return;const auto& profile=configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
    ::SetDlgItemTextW(window_,profile_name_id,wide(profile.name).c_str());
    const context::MatchCriterion criterion=profile.criteria.empty()?context::MatchCriterion{}:profile.criteria.front();
    ::SendDlgItemMessageW(window_,profile_property_id,CB_SETCURSEL,static_cast<WPARAM>(criterion.property),0);
    ::SendDlgItemMessageW(window_,profile_mode_id,CB_SETCURSEL,static_cast<WPARAM>(criterion.mode),0);
    ::SetDlgItemTextW(window_,profile_value_id,wide(criterion.value).c_str());const int gesture=selected_gesture();
    if(gesture>=0){const auto& id=configuration_->gestures.gestures[static_cast<std::size_t>(gesture)].id;auto action=profile.actions_by_gesture.find(id);
      ::SetDlgItemTextW(window_,profile_shortcut_id,action==profile.actions_by_gesture.end()?L"":wide(action->second.value).c_str());}
}

void WindowsSettingsWindow::add_gesture(){
    const std::string name=utf8_from_control(window_,gesture_name_id);
    if(name.empty()){::MessageBoxW(window_,L"Enter a gesture name first.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    gestures::GestureRepository repository(configuration_->gestures.gestures);
    std::string id="gesture-"+std::to_string(::GetTickCount64());
    unsigned suffix=1;while(!repository.create(id,name)){if(repository.find(id))id="gesture-"+std::to_string(::GetTickCount64())+"-"+std::to_string(suffix++);
        else{::MessageBoxW(window_,L"Gesture names must be unique.",L"Strokes++",MB_OK|MB_ICONERROR);return;}}
    refresh_gestures();::SendDlgItemMessageW(window_,gestures_id,LB_SETCURSEL,configuration_->gestures.gestures.size()-1,0);
}

void WindowsSettingsWindow::rename_gesture(){
    const int selected=selected_gesture();if(selected<0)return;const std::string name=utf8_from_control(window_,gesture_name_id);
    gestures::GestureRepository repository(configuration_->gestures.gestures);
    if(!repository.rename(configuration_->gestures.gestures[static_cast<std::size_t>(selected)].id,name))
        ::MessageBoxW(window_,L"Enter a unique gesture name.",L"Strokes++",MB_OK|MB_ICONERROR);
    refresh_gestures();
}

void WindowsSettingsWindow::delete_gesture(){
    const int selected=selected_gesture();if(selected<0)return;
    if(::MessageBoxW(window_,L"Delete the selected gesture and all of its mappings?",L"Strokes++",MB_YESNO|MB_ICONWARNING)!=IDYES)return;
    const std::string id=configuration_->gestures.gestures[static_cast<std::size_t>(selected)].id;
    gestures::GestureRepository repository(configuration_->gestures.gestures);(void)repository.erase(id);
    configuration_->profiles.global_actions.erase(id);
    for(auto& profile:configuration_->profiles.profiles)profile.actions_by_gesture.erase(id);
    refresh_gestures();
}

void WindowsSettingsWindow::train_gesture(){
    const int selected=selected_gesture();if(selected<0)return;
    ::EnableWindow(window_,FALSE);WindowsGestureTrainer trainer;auto stroke=trainer.capture(instance_);::EnableWindow(window_,TRUE);::SetForegroundWindow(window_);
    if(!stroke)return;auto& definition=configuration_->gestures.gestures[static_cast<std::size_t>(selected)];
    gestures::GestureRepository repository(configuration_->gestures.gestures);
    const std::string id="template-"+std::to_string(::GetTickCount64());
    if(repository.add_template(definition.id,{id,std::move(*stroke)}))(void)repository.set_enabled(definition.id,true);
    refresh_gestures();::SendDlgItemMessageW(window_,gestures_id,LB_SETCURSEL,selected,0);
}

void WindowsSettingsWindow::toggle_gesture(){
    const int selected=selected_gesture();if(selected<0)return;auto& gesture=configuration_->gestures.gestures[static_cast<std::size_t>(selected)];
    gestures::GestureRepository repository(configuration_->gestures.gestures);
    if(!repository.set_enabled(gesture.id,!gesture.enabled)){
        ::MessageBoxW(window_,L"A gesture needs at least one training sample before it can be enabled.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    refresh_gestures();::SendDlgItemMessageW(window_,gestures_id,LB_SETCURSEL,selected,0);
}

void WindowsSettingsWindow::add_profile(){
    const std::string name=utf8_from_control(window_,profile_name_id);const std::string value=utf8_from_control(window_,profile_value_id);
    const auto property=static_cast<context::ApplicationProperty>(::SendDlgItemMessageW(window_,profile_property_id,CB_GETCURSEL,0,0));
    const auto mode=static_cast<context::MatchMode>(::SendDlgItemMessageW(window_,profile_mode_id,CB_GETCURSEL,0,0));
    if(name.empty()||value.empty()){::MessageBoxW(window_,L"Enter a unique profile name and match value.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    context::ProfileRepository repository(configuration_->profiles.profiles);std::string id="profile-"+std::to_string(::GetTickCount64());
    if(!repository.create(id,name)||!repository.add_criterion(id,{property,mode,value})||
       !repository.set_enabled(id,true)){::MessageBoxW(window_,L"Profile name must be unique.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    ::SendDlgItemMessageW(window_,profiles_id,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(name).c_str()));
    ::SendDlgItemMessageW(window_,profiles_id,LB_SETCURSEL,configuration_->profiles.profiles.size()-1,0);
}

void WindowsSettingsWindow::update_profile(){
    const int selected=selected_profile();if(selected<0)return;const std::string name=utf8_from_control(window_,profile_name_id);
    const std::string value=utf8_from_control(window_,profile_value_id);
    const auto property=static_cast<context::ApplicationProperty>(::SendDlgItemMessageW(window_,profile_property_id,CB_GETCURSEL,0,0));
    const auto mode=static_cast<context::MatchMode>(::SendDlgItemMessageW(window_,profile_mode_id,CB_GETCURSEL,0,0));
    if(name.empty()||value.empty()){::MessageBoxW(window_,L"Enter a profile name and match value.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    auto& profile=configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
    context::ProfileRepository repository(configuration_->profiles.profiles);
    if(!repository.rename(profile.id,name)){::MessageBoxW(window_,L"Profile names must be unique.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    if(profile.criteria.empty())profile.criteria.push_back({property,mode,value});
    else profile.criteria.front()={property,mode,value};
    ::SendDlgItemMessageW(window_,profiles_id,LB_DELETESTRING,selected,0);
    ::SendDlgItemMessageW(window_,profiles_id,LB_INSERTSTRING,selected,reinterpret_cast<LPARAM>(wide(name).c_str()));
    ::SendDlgItemMessageW(window_,profiles_id,LB_SETCURSEL,selected,0);
}

void WindowsSettingsWindow::delete_profile(){
    const int selected=selected_profile();if(selected<0)return;if(::MessageBoxW(window_,L"Delete the selected profile?",L"Strokes++",MB_YESNO|MB_ICONWARNING)!=IDYES)return;
    context::ProfileRepository repository(configuration_->profiles.profiles);(void)repository.erase(configuration_->profiles.profiles[static_cast<std::size_t>(selected)].id);
    ::SendDlgItemMessageW(window_,profiles_id,LB_DELETESTRING,selected,0);
}

void WindowsSettingsWindow::assign_global_action(){
    const int selected=selected_gesture();if(selected<0)return;const std::string shortcut=utf8_from_control(window_,shortcut_id);
    if(!actions::parse_keyboard_shortcut(shortcut)){::MessageBoxW(window_,L"The shortcut is invalid.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    configuration_->profiles.global_actions.insert_or_assign(configuration_->gestures.gestures[static_cast<std::size_t>(selected)].id,
        actions::Action{actions::ActionType::keyboard_shortcut,shortcut});
}

void WindowsSettingsWindow::assign_profile_action(){
    const int profile=selected_profile(),gesture=selected_gesture();if(profile<0||gesture<0)return;
    const std::string shortcut=utf8_from_control(window_,profile_shortcut_id);
    if(!actions::parse_keyboard_shortcut(shortcut)){::MessageBoxW(window_,L"The override shortcut is invalid.",L"Strokes++",MB_OK|MB_ICONERROR);return;}
    context::ProfileRepository repository(configuration_->profiles.profiles);
    (void)repository.set_action(configuration_->profiles.profiles[static_cast<std::size_t>(profile)].id,
        configuration_->gestures.gestures[static_cast<std::size_t>(gesture)].id,
        {actions::ActionType::keyboard_shortcut,shortcut});
}

bool WindowsSettingsWindow::save_values(){
    auto copy=*configuration_;double movement=0,distance=0,threshold=0,opacity=0;long maximum=0,width=0;
    if(!read_double(window_,move_id,0,1000,movement)||!read_double(window_,distance_id,0,1000,distance)||
       !read_integer(window_,max_id,2,1000000,maximum)||!read_double(window_,threshold_id,0,1,threshold)||
       !read_integer(window_,width_id,1,100,width)||!read_double(window_,opacity_id,0,1,opacity)){
        ::MessageBoxW(window_,L"One or more numeric settings are invalid.",L"Strokes++",MB_OK|MB_ICONERROR);return false;}
    wchar_t shortcut[128]{};::GetDlgItemTextW(window_,shortcut_id,shortcut,128);std::string shortcut_text;
    for(const wchar_t* p=shortcut;*p;++p){if(*p>127){::MessageBoxW(window_,L"Shortcut must use ASCII key names.",L"Strokes++",MB_OK|MB_ICONERROR);return false;}shortcut_text.push_back(static_cast<char>(*p));}
    const int gesture_selected=selected_gesture();
    if(!shortcut_text.empty()&&!actions::parse_keyboard_shortcut(shortcut_text)){::MessageBoxW(window_,L"The global shortcut is invalid.",L"Strokes++",MB_OK|MB_ICONERROR);return false;}
    const LRESULT button_selected=::SendDlgItemMessageW(window_,button_id,CB_GETCURSEL,0,0);
    if(button_selected<0||button_selected>3){::MessageBoxW(window_,L"Select an activation button.",L"Strokes++",MB_OK|MB_ICONERROR);return false;}
    copy.global.gestures_enabled=::IsDlgButtonChecked(window_,enabled_id)==BST_CHECKED;
    copy.global.gesture_button=static_cast<input::ActivationButton>(button_selected);
    copy.global.movement_threshold=movement;copy.global.minimum_point_distance=distance;copy.global.maximum_points=static_cast<std::size_t>(maximum);
    copy.global.recognition_threshold=threshold;copy.global.overlay.enabled=::IsDlgButtonChecked(window_,overlay_id)==BST_CHECKED;
    copy.global.overlay.line_width=static_cast<int>(width);copy.global.overlay.opacity=opacity;
    if(gesture_selected>=0&&!shortcut_text.empty())copy.profiles.global_actions.insert_or_assign(
        copy.gestures.gestures[static_cast<std::size_t>(gesture_selected)].id,
        actions::Action{actions::ActionType::keyboard_shortcut,std::move(shortcut_text)});
    *configuration_=std::move(copy);return true;
}
}  // namespace strokes::ui
