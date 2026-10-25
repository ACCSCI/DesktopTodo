#include "todo/application.hpp"
#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <commctrl.h>
#include <shellapi.h>

namespace todo {
namespace {
enum {list_id=100,new_id,menu_id,show_id,delete_id,undo_id,redo_id,backup_id,restore_id,startup_id,lock_id,hide_id,quit_id,help_id,status_id,storage_id};
constexpr wchar_t manager_class[]=L"DesktopTodo.Manager";
}
void Application::create_manager() {
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=instance_;cls.lpfnWndProc=manager_proc;
    cls.lpszClassName=manager_class;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassExW(&cls);taskbar_created_=RegisterWindowMessageW(L"TaskbarCreated");
    int dpi=int(GetDpiForSystem());
    manager_=CreateWindowExW(WS_EX_APPWINDOW,manager_class,L"DesktopTodo · 便签管理",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,pixels(640,dpi),pixels(450,dpi),nullptr,nullptr,instance_,this);
    if(!manager_)throw std::runtime_error(win::last_error("Create manager"));
}
void Application::layout_manager() {
    if(!manager_||!list_)return;
    RECT client{};GetClientRect(manager_,&client);int dpi=int(GetDpiForWindow(manager_));
    auto p=[&](int dip){return pixels(dip,dpi);};
    auto place=[&](int id,int x,int y,int width,int height){SetWindowPos(GetDlgItem(manager_,id),nullptr,x,y,width,height,SWP_NOZORDER|SWP_NOACTIVATE);};
    int width=client.right;
    place(help_id,p(18),p(14),width-p(36),p(42));
    place(list_id,p(18),p(62),width-p(36),std::max(p(80),int(client.bottom)-p(240)));
    int top=std::max(p(150),int(client.bottom)-p(164));
    const int first[]={new_id,menu_id,show_id,delete_id,undo_id,redo_id};
    int button_width=std::max(p(50),(width-p(66))/6);
    for(int i=0;i<6;++i)place(first[i],p(18)+i*(button_width+p(6)),top,button_width,p(30));
    place(startup_id,p(18),top+p(40),width/2-p(20),p(26));
    place(lock_id,width/2,top+p(40),width/2-p(18),p(26));
    place(backup_id,p(18),top+p(76),p(100),p(30));place(restore_id,p(126),top+p(76),p(100),p(30));
    place(storage_id,p(234),top+p(76),p(100),p(30));
    place(hide_id,width-p(226),top+p(76),p(100),p(30));place(quit_id,width-p(118),top+p(76),p(100),p(30));
    place(status_id,p(18),client.bottom-p(40),width-p(36),p(32));
}
void Application::refresh_manager() {
    if(!list_)return;
    int selection=int(SendMessageW(list_,LB_GETCURSEL,0,0));
    Id selected=selection>=0?Id(SendMessageW(list_,LB_GETITEMDATA,selection,0)):0;
    SendMessageW(list_,WM_SETREDRAW,FALSE,0);SendMessageW(list_,LB_RESETCONTENT,0,0);
    int selected_index=-1;
    for(const auto& note:model().notes) {
        std::wstring label=win::wide(note.title)+L"  —  "+ui::mode_name(note.mode)+
            (note.mode==Mode::desktop&&note.locked?L" / 已锁定":L"")+L"  ·  "+std::to_wstring(note.tasks.size())+L" 项";
        int index=int(SendMessageW(list_,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(list_,LB_SETITEMDATA,index,LPARAM(note.id));if(note.id==selected)selected_index=index;
    }
    if(selected_index<0&&!model().notes.empty())selected_index=0;
    SendMessageW(list_,LB_SETCURSEL,selected_index,0);SendMessageW(list_,WM_SETREDRAW,TRUE,0);InvalidateRect(list_,nullptr,TRUE);
    try{SendMessageW(startup_check_,BM_SETCHECK,startup_->enabled()?BST_CHECKED:BST_UNCHECKED,0);}
    catch(const std::exception& e){last_error_=win::wide(e.what());}
    SendMessageW(lock_check_,BM_SETCHECK,model().lock_on_start?BST_CHECKED:BST_UNCHECKED,0);
    EnableWindow(GetDlgItem(manager_,undo_id),can_undo());EnableWindow(GetDlgItem(manager_,redo_id),can_redo());
    auto status=last_error_.empty()?L"全部操作自动保存。关闭此窗口后，便签和托盘继续运行。":last_error_;
    SetWindowTextW(GetDlgItem(manager_,status_id),status.c_str());
}
void Application::manager_command(int id) {
    int selected=int(SendMessageW(list_,LB_GETCURSEL,0,0));
    Id note=selected>=0?Id(SendMessageW(list_,LB_GETITEMDATA,selected,0)):0;
    if(id==new_id)new_note();
    else if(id==menu_id&&note){POINT point{};GetCursorPos(&point);note_menu(note,point);}
    else if(id==show_id&&note){auto it=views_.find(note);if(it!=views_.end())it->second->show();}
    else if(id==delete_id&&note)remove_note(note);
    else if(id==undo_id)undo();else if(id==redo_id)redo();
    else if(id==backup_id)backup_dialog();else if(id==restore_id)restore_dialog();
    else if(id==storage_id){POINT point{};GetCursorPos(&point);storage_menu(point);}
    else if(id==startup_id){startup_->set_enabled(SendMessageW(startup_check_,BM_GETCHECK,0,0)==BST_CHECKED);refresh_manager();}
    else if(id==lock_id){bool lock=SendMessageW(lock_check_,BM_GETCHECK,0,0)==BST_CHECKED;change([&](Model& m){m.lock_on_start=lock;});}
    else if(id==hide_id)ui::hide(manager_);else if(id==quit_id)quit();
}
LRESULT CALLBACK Application::manager_proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<Application*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(message==WM_NCCREATE){self=static_cast<Application*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));self->manager_=window;}
    if(!self)return DefWindowProcW(window,message,w,l);
    try {
        if(message==WM_CREATE) {
            int dpi=int(GetDpiForWindow(window));self->manager_font_=ui::font(dpi,10);
            auto control=[&](const wchar_t* cls,const wchar_t* text,int id,DWORD style){
                HWND child=CreateWindowExW(cls==std::wstring(L"LISTBOX")?WS_EX_CLIENTEDGE:0,cls,text,
                    WS_CHILD|WS_VISIBLE|style,0,0,1,1,window,reinterpret_cast<HMENU>(INT_PTR(id)),self->instance_,nullptr);
                SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(self->manager_font_),TRUE);return child;};
            control(L"STATIC",L"每个便签可单独选择桌面、置顶或窗口模式。\n双击标题或待办文字编辑，拖动行左侧手柄排序；更多操作在右键菜单。",help_id,0);
            self->list_=control(L"LISTBOX",L"",list_id,WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT);
            const wchar_t* names[]={L"新建便签",L"便签菜单",L"显示便签",L"删除便签",L"撤销",L"重做"};
            for(int i=0;i<6;++i)control(L"BUTTON",names[i],new_id+i,WS_TABSTOP|BS_PUSHBUTTON);
            self->startup_check_=control(L"BUTTON",L"登录 Windows 后自动启动",startup_id,WS_TABSTOP|BS_AUTOCHECKBOX);
            self->lock_check_=control(L"BUTTON",L"启动时锁定桌面便签",lock_id,WS_TABSTOP|BS_AUTOCHECKBOX);
            control(L"BUTTON",L"导出备份…",backup_id,WS_TABSTOP|BS_PUSHBUTTON);
            control(L"BUTTON",L"恢复备份…",restore_id,WS_TABSTOP|BS_PUSHBUTTON);
            control(L"BUTTON",L"存储设置…",storage_id,WS_TABSTOP|BS_PUSHBUTTON);
            control(L"BUTTON",L"收起到托盘",hide_id,WS_TABSTOP|BS_PUSHBUTTON);
            control(L"BUTTON",L"退出软件",quit_id,WS_TABSTOP|BS_PUSHBUTTON);
            control(L"STATIC",L"",status_id,0);self->layout_manager();return 0;
        }
        if(message==WM_SIZE){self->layout_manager();return 0;}
        if(message==WM_GETMINMAXINFO){auto* info=reinterpret_cast<MINMAXINFO*>(l);int dpi=int(GetDpiForWindow(window));info->ptMinTrackSize={pixels(570,dpi),pixels(390,dpi)};return 0;}
        if(message==WM_COMMAND){if(LOWORD(w)==list_id&&HIWORD(w)==LBN_DBLCLK)self->manager_command(menu_id);else self->manager_command(LOWORD(w));return 0;}
        if(message==WM_CLOSE){ui::hide(window);return 0;}
        if(message==message_sync){self->synchronize();return 0;}
        if(message==message_show_manager){self->show_manager();return 0;}
        if(message==WM_COPYDATA)return self->automation(*reinterpret_cast<COPYDATASTRUCT*>(l));
        if(message==WM_TIMER&&w==1){KillTimer(window,1);self->synchronize();return 0;}
        if(message==message_tray){
            UINT action=LOWORD(l);
            if(action==WM_LBUTTONUP||action==NIN_SELECT||action==NIN_KEYSELECT)self->show_manager();
            if(action==WM_RBUTTONUP||action==WM_CONTEXTMENU){POINT point{};GetCursorPos(&point);self->tray_menu(point);}return 0;
        }
        if(message==self->taskbar_created_){self->tray(true);self->shell_changed();return 0;}
        if(message==WM_DISPLAYCHANGE||message==WM_SETTINGCHANGE){
            self->change([](Model& m){for(auto& note:m.notes)note.bounds=win::visible_bounds(note.bounds);},false);self->shell_changed();return 0;
        }
        if(message==WM_QUERYENDSESSION)return TRUE;
        if(message==WM_ENDSESSION&&w){self->quit();return 0;}
    }catch(const std::exception& error){self->error(error);return 0;}
    return DefWindowProcW(window,message,w,l);
}
}
