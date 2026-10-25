#include "todo/application.hpp"
#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <charconv>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace todo {
namespace {
std::string quote(const std::string& text) {
    std::ostringstream out;out<<'"';
    for(unsigned char c:text) {
        if(c=='"'||c=='\\')out<<'\\'<<c;
        else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
        else out<<c;
    }
    out<<'"';return out.str();
}
std::string decode(const std::string& text) {
    std::string result;
    for(std::size_t i=0;i<text.size();++i) {
        if(text[i]=='%') {
            if(i+2>=text.size())throw std::invalid_argument("Invalid percent encoding");
            unsigned value=0;auto parsed=std::from_chars(text.data()+i+1,text.data()+i+3,value,16);
            if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+i+3)throw std::invalid_argument("Invalid percent encoding");
            result.push_back(char(value));i+=2;
        }else result.push_back(text[i]);
    }
    if(!valid_utf8(result))throw std::invalid_argument("Invalid UTF-8 command");
    return result;
}
std::vector<std::string> parts(const std::string& text) {
    std::vector<std::string> result;std::size_t begin=0;
    for(;;){auto end=text.find('\t',begin);result.push_back(decode(text.substr(begin,end-begin)));
        if(end==std::string::npos)break;begin=end+1;}
    return result;
}
std::int64_t number(const std::string& text,int base=10) {
    std::int64_t value=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,base);
    if(parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size())throw std::invalid_argument("Invalid command number");
    return value;
}
std::string native_window(HWND window) {
    if(!IsWindow(window))return "null";
    auto bounds=win::screen_bounds(window);HWND parent=GetParent(window);wchar_t name[128]{};
    if(parent)GetClassNameW(parent,name,int(std::size(name)));
    std::ostringstream out;out<<"{\"handle\":"<<reinterpret_cast<std::uintptr_t>(window)
        <<",\"parent\":"<<reinterpret_cast<std::uintptr_t>(parent)<<",\"parentClass\":"<<quote(win::utf8(name))
        <<",\"style\":"<<std::uint32_t(GetWindowLongPtrW(window,GWL_STYLE))
        <<",\"extended\":"<<std::uint32_t(GetWindowLongPtrW(window,GWL_EXSTYLE))
        <<",\"visible\":"<<(IsWindowVisible(window)?"true":"false")
        <<",\"presentations\":"<<reinterpret_cast<std::uintptr_t>(GetPropW(window,L"DesktopTodo.Presentations"))
        <<",\"rect\":["<<bounds.x<<','<<bounds.y<<','<<bounds.width<<','<<bounds.height<<"],\"dpi\":"<<bounds.dpi<<'}';
    return out.str();
}
}
void Application::write_snapshot() {
    if(!options_.automation)return;
    std::ostringstream out;out<<"{\"pid\":"<<GetCurrentProcessId()<<",\"revision\":"<<revision_
        <<",\"error\":"<<quote(win::utf8(last_error_))<<",\"dataDirectory\":"<<quote(win::path_utf8(options_.data_directory))<<",\"startup\":"<<(startup_->enabled()?"true":"false")
        <<",\"canUndo\":"<<(can_undo()?"true":"false")<<",\"canRedo\":"<<(can_redo()?"true":"false")
        <<",\"manager\":"<<native_window(manager_)<<",\"notes\":[";
    bool comma=false;
    for(const auto& note:model().notes) {
        if(comma)out<<',';comma=true;
        auto it=views_.find(note.id);NoteView* view=it==views_.end()?nullptr:it->second.get();
        out<<"{\"id\":"<<note.id<<",\"title\":"<<quote(note.title)<<",\"color\":"<<note.color
            <<",\"mode\":"<<int(note.mode)<<",\"locked\":"<<(note.locked?"true":"false")
            <<",\"corner\":"<<int(note.corner)<<",\"offset\":"<<note.lock_offset<<",\"filter\":"<<int(note.filter)
            <<",\"window\":"<<native_window(view?view->window():nullptr)
            <<",\"unlock\":"<<native_window(view?view->unlock_window():nullptr)
            <<",\"editorFocused\":"<<(view&&view->editor_focused()?"true":"false")<<",\"tasks\":[";
        bool task_comma=false;
        for(const auto& task:note.tasks) {
            if(task_comma)out<<',';task_comma=true;
            out<<"{\"id\":"<<task.id<<",\"text\":"<<quote(task.text)<<",\"icon\":"<<quote(task.icon)
                <<",\"color\":"<<task.color<<",\"status\":"<<int(task.status)<<'}';
        }
        out<<"],\"visible\":[";bool first=true;
        for(auto id:visible_tasks(note)){if(!first)out<<',';first=false;out<<id;}
        out<<"]}";
    }
    out<<"]}";
    auto temporary=options_.control_directory/L"automation.tmp";
    {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);file<<out.str();file.flush();
        if(!file)throw std::runtime_error("Cannot write automation snapshot");}
    auto destination=options_.control_directory/L"automation.json";
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error(win::last_error("Publish automation snapshot"));
}
LRESULT Application::automation(const COPYDATASTRUCT& data) {
    if(!options_.automation||data.dwData!=0x44544F44||!data.lpData||data.cbData<1||data.cbData>65536)return FALSE;
    try {
        std::string payload(static_cast<const char*>(data.lpData),data.cbData);
        if(!payload.empty()&&payload.back()==0)payload.pop_back();
        auto args=parts(payload);
        auto arg=[&](std::size_t index)->const std::string&{if(index>=args.size())throw std::invalid_argument("Missing command argument");return args[index];};
        auto id=[&](std::size_t index){return number(arg(index));};
        auto integer=[&](std::size_t index){auto n=id(index);if(n<std::numeric_limits<int>::min()||n>std::numeric_limits<int>::max())throw std::invalid_argument("Command integer overflow");return int(n);};
        auto color=[&](std::size_t index){auto n=number(arg(index),16);if(n<0||n>0xFFFFFF)throw std::invalid_argument("Command color overflow");return Color(n);};
        const auto& action=arg(0);
        if(action=="snapshot"){synchronize();return TRUE;}
        if(action=="relocate"){relocate_storage(win::wide(arg(1)));synchronize();return TRUE;}
        if(action=="compact"){compact_storage();write_snapshot();return TRUE;}
        if(action=="clear-data") {
            bool cleared=clear_all_data([&](int stage){return arg(std::size_t(stage))=="confirm";});
            synchronize();return cleared?TRUE:FALSE;
        }
        if(action=="motion-state") {
            auto view=views_.find(id(1));if(view==views_.end())throw std::invalid_argument("Missing note");
            std::ofstream file(options_.control_directory/L"motion.json");
            file<<view->second->motion_state();file.close();write_snapshot();return TRUE;
        }
        if(action=="readonly-wheel") {
            auto view=views_.find(id(1));if(view==views_.end())throw std::invalid_argument("Missing note");
            if(!wheel_hook_)throw std::runtime_error("Read-only wheel hook is not registered");
            RECT bounds{};GetWindowRect(view->second->window(),&bounds);
            POINT point{bounds.left+30,bounds.top+view->second->layout().list_top()+20};
            HWND hit=arg(3)=="desktop"?GetParent(view->second->window()):manager_;
            bool consumed=locked_wheel(point,integer(2),hit);write_snapshot();return consumed?TRUE:FALSE;
        }
        if(action=="focus-editor") {
            auto view=views_.find(id(1));if(view==views_.end())throw std::invalid_argument("Missing note");
            view->second->focus_editor();write_snapshot();return view->second->editor_focused()?TRUE:FALSE;
        }
        if(action=="editor-text"||action=="commit-editor") {
            auto view=views_.find(id(1));
            if(view==views_.end()||!view->second->editor_window())throw std::invalid_argument("Missing editor");
            if(action=="editor-text")view->second->set_editor_text(win::wide(arg(2)));
            else view->second->commit_editor();
            synchronize();return TRUE;
        }
        if(action=="toggle-lock") {
            auto view=views_.find(id(1));
            if(view==views_.end()||!IsWindowVisible(view->second->unlock_window()))throw std::invalid_argument("No desktop lock control");
            SendMessageW(view->second->unlock_window(),WM_LBUTTONUP,0,0);
            synchronize();return TRUE;
        }
        if(action=="paint") {
            synchronize();
            for(auto& [identity,view]:views_) {
                RedrawWindow(view->window(),nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
                if(IsWindowVisible(view->unlock_window()))
                    RedrawWindow(view->unlock_window(),nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
            }
            write_snapshot();return last_error_.empty()?TRUE:FALSE;
        }
        if(action=="recreate-windows") {
            for(auto& [identity,view]:views_)view->destroy();
            shell_changed();synchronize();return TRUE;
        }
        if(action=="quit"){write_snapshot();quit();return TRUE;}
        if(action=="show-manager"){show_manager();write_snapshot();return TRUE;}
        if(action=="hide-manager"){ui::hide(manager_);write_snapshot();return TRUE;}
        if(action=="startup"){startup_->set_enabled(integer(1)!=0);refresh_manager();write_snapshot();return TRUE;}
        if(action=="backup"){controller_->backup(arg(1));write_snapshot();return TRUE;}
        if(action=="restore") {
            SqliteStore source(arg(1));auto restored=source.load();
            change([&](Model& model){model=restored;for(auto& note:model.notes)note.bounds=win::visible_bounds(note.bounds);});
            synchronize();return TRUE;
        }
        if(action=="undo")undo();else if(action=="redo")redo();
        else change([&](Model& model){
            if(action=="new-note"){auto note=add_note(model,arg(1));note_by_id(model,note).bounds=win::visible_bounds(note_by_id(model,note).bounds);}
            else if(action=="delete-note")todo::remove_note(model,id(1));
            else if(action=="rename-note")rename_note(model,id(1),arg(2));
            else if(action=="note-color")set_note_color(model,id(1),color(2));
            else if(action=="add-task")add_task(model,id(1),arg(2),ui::now());
            else if(action=="edit-task")edit_task(model,id(1),id(2),arg(3));
            else if(action=="delete-task")remove_task(model,id(1),id(2));
            else if(action=="status")set_status(model,id(1),id(2),Status(integer(3)),ui::now());
            else if(action=="icon")set_task_icon(model,id(1),id(2),arg(3));
            else if(action=="task-color")set_task_color(model,id(1),id(2),color(3));
            else if(action=="filter")set_filter(model,id(1),Filter(integer(2)));
            else if(action=="mode")set_mode(model,id(1),Mode(integer(2)));
            else if(action=="lock")set_lock(model,id(1),integer(2)!=0);
            else if(action=="corner")set_lock_position(model,id(1),Corner(integer(2)),integer(3));
            else if(action=="bounds")set_bounds(model,id(1),{integer(2),integer(3),integer(4),integer(5),integer(6)});
            else if(action=="reorder")reorder_visible(model,id(1),id(2),id(3));
            else if(action=="move")move_task(model,id(1),id(2),id(3),id(4));
            else if(action=="clear-completed")remove_completed(model,id(1));
            else if(action=="lock-on-start")model.lock_on_start=integer(1)!=0;
            else if(action=="seed") {
                int notes=integer(1),tasks=integer(2);
                if(notes<1||notes>20||tasks<0||tasks>10000||notes*tasks>10000)throw std::invalid_argument("Invalid test load");
                model.notes.clear();
                for(int n=0;n<notes;++n) {
                    auto note=add_note(model,"测试便签 "+std::to_string(n+1));
                    auto& target=note_by_id(model,note);target.locked=true;target.bounds=win::visible_bounds(target.bounds);
                    for(int t=0;t<tasks;++t)add_task(model,note,"待办任务 "+std::to_string(t+1),ui::now());
                }
            }
            else throw std::invalid_argument("Unknown automation command");
        });
        synchronize();return TRUE;
    }catch(const std::exception& e){last_error_=win::wide(e.what());try{write_snapshot();}catch(...){}return FALSE;}
}
}
