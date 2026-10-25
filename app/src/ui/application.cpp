#include "todo/application.hpp"
#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <algorithm>
#include <fstream>

namespace todo {
Application::Application(HINSTANCE instance,AppOptions options) : instance_(instance),options_(std::move(options)) {
    if(options_.control_directory.empty())options_.control_directory=options_.data_directory;
    if(options_.settings_file.empty())options_.settings_file=options_.data_directory/L"DesktopTodo.ini";
    std::filesystem::create_directories(options_.data_directory);
    auto file=options_.data_directory/L"todo.sqlite";
    bool first=!std::filesystem::exists(file);
    store_=std::make_unique<SqliteStore>(win::path_utf8(file));
    controller_=std::make_unique<Controller>(*store_);
    if(options_.automation) {
        startup_test_key_=L"Software\\DesktopTodo\\Tests\\Automation-"+std::to_wstring(GetCurrentProcessId());
        startup_=std::make_unique<win::Startup>(win::executable_path(),L"DesktopTodo.Test",startup_test_key_);
    } else startup_=std::make_unique<win::Startup>(win::executable_path());
    if(first)controller_->change([](Model& m){add_note(m);},false);
    else {
        try {
            auto backups=options_.data_directory/L"backups";std::filesystem::create_directories(backups);
            auto daily=backups/(L"daily-"+std::to_wstring(ui::now()/86400)+L".sqlite");
            if(!std::filesystem::exists(daily))store_->backup(win::path_utf8(daily));
            std::vector<std::filesystem::path> files;
            for(const auto& entry:std::filesystem::directory_iterator(backups))
                if(entry.is_regular_file()&&entry.path().filename().wstring().starts_with(L"daily-"))files.push_back(entry.path());
            std::sort(files.begin(),files.end());
            while(files.size()>7){std::filesystem::remove(files.front());files.erase(files.begin());}
        } catch(const std::exception& e){last_error_=L"自动备份暂不可用："+win::wide(e.what());}
        controller_->change([](Model& m){restore_startup_policy(m);},false);
    }
    // Clamp stored screen positions against the current display configuration.
    controller_->change([](Model& m){for(auto& note:m.notes)note.bounds=win::visible_bounds(note.bounds);},false);
    create_manager();tray(true);
    if(!options_.startup&&(first||model().notes.empty()))show_manager();
}
Application::~Application() {
    if(wheel_hook_){UnhookWindowsHookEx(wheel_hook_);wheel_hook_=nullptr;wheel_owner_=nullptr;}
    quitting_=true;views_.clear();tray(false);
    if(manager_)DestroyWindow(manager_);
    if(manager_font_)DeleteObject(manager_font_);
    if(!startup_test_key_.empty())RegDeleteTreeW(HKEY_CURRENT_USER,startup_test_key_.c_str());
    if(storage_mutex_)CloseHandle(storage_mutex_);
}
int Application::run() {
    synchronize();
    MSG message{};
    int result;
    while((result=GetMessageW(&message,nullptr,0,0))>0) {
        if(manager_&&IsWindowVisible(manager_)&&IsDialogMessageW(manager_,&message))continue;
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    if(result<0)throw std::runtime_error(win::last_error("Windows message loop"));
    return int(message.wParam);
}
const Note* Application::find_note(Id id) const {
    const auto& notes=model().notes;
    auto found=std::find_if(notes.begin(),notes.end(),[=](const Note& note){return note.id==id;});
    return found==notes.end()?nullptr:&*found;
}
bool Application::change(const std::function<void(Model&)>& action,bool undo) {
    bool changed=controller_->change(action,undo);
    if(changed){++revision_;last_error_.clear();refresh();}
    return changed;
}
void Application::undo(){if(controller_->undo()){++revision_;refresh();}}
void Application::redo(){if(controller_->redo()){++revision_;refresh();}}
void Application::refresh(){if(manager_&&!quitting_)PostMessageW(manager_,message_sync,0,0);}
void Application::synchronize() {
    if(synchronizing_||quitting_)return;
    synchronizing_=true;
    struct Reset{bool& value;~Reset(){value=false;}}reset{synchronizing_};
    for(auto it=views_.begin();it!=views_.end();) {
        if(!find_note(it->first))it=views_.erase(it);else ++it;
    }
    bool missing_shell=false;
    for(const auto& note:model().notes) {
        auto& view=views_[note.id];if(!view)view=std::make_unique<NoteView>(*this,note.id);
        try {view->synchronize();}
        catch(const std::exception& e) {
            if(note.mode!=Mode::desktop)throw;
            missing_shell=true;last_error_=L"桌面连接暂不可用，可在管理窗口切换显示模式。 "+win::wide(e.what());
        }
    }
    if(missing_shell&&shell_attempts_<10){++shell_attempts_;SetTimer(manager_,1,200*shell_attempts_,nullptr);}
    else KillTimer(manager_,1);
    update_wheel_hook();refresh_manager();write_snapshot();
}
void Application::shell_changed() {
    if(quitting_)return;
    shell_attempts_=0;
    SetTimer(manager_,1,200,nullptr);
}
void Application::show_manager() {
    refresh_manager();ui::show(manager_);SetForegroundWindow(manager_);
}
void Application::quit(){quitting_=true;views_.clear();tray(false);PostQuitMessage(0);}
void Application::error(const std::exception& e) {
    last_error_=win::wide(e.what());
    std::ofstream log(options_.data_directory/L"errors.log",std::ios::app);
    log<<e.what()<<'\n';
    if(!options_.automation)MessageBoxW(manager_,last_error_.c_str(),L"DesktopTodo",MB_OK|MB_ICONERROR);
    refresh_manager();
}
void Application::new_note() {
    auto title=ui::prompt(manager_,L"新建便签",L"我的待办",128);if(!title)return;
    change([&](Model& m){Id id=add_note(m,win::utf8(*title));note_by_id(m,id).bounds=win::visible_bounds(note_by_id(m,id).bounds);});
}
void Application::remove_note(Id id) {change([&](Model& m){todo::remove_note(m,id);});}
NoteView* Application::view_at(POINT point) {
    HWND hit=WindowFromPoint(point);
    while(hit) {
        for(auto& [id,view]:views_)if(view->window()==hit)return view.get();
        hit=GetParent(hit);
    }
    return nullptr;
}
}
