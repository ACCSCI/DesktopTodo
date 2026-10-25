#include "todo/application.hpp"
#include <shellapi.h>

namespace todo {
void Application::tray(bool add) {
    if(!manager_)return;
    NOTIFYICONDATAW icon{};icon.cbSize=sizeof(icon);icon.hWnd=manager_;icon.uID=1;
    icon.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;icon.uCallbackMessage=message_tray;
    icon.hIcon=LoadIconW(instance_,MAKEINTRESOURCEW(101));wcscpy_s(icon.szTip,L"DesktopTodo · 桌面待办");
    if(!add){Shell_NotifyIconW(NIM_DELETE,&icon);return;}
    if(!Shell_NotifyIconW(NIM_ADD,&icon))Shell_NotifyIconW(NIM_MODIFY,&icon);
    icon.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&icon);
}
void Application::tray_menu(POINT point) {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,1,L"管理便签");AppendMenuW(menu,MF_STRING,2,L"新建便签…");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,3,L"锁定全部桌面便签");
    AppendMenuW(menu,MF_STRING,4,L"解锁全部桌面便签");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,5,L"退出软件");
    SetForegroundWindow(manager_);
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,manager_,nullptr);
    DestroyMenu(menu);PostMessageW(manager_,WM_NULL,0,0);
    if(result==1)show_manager();else if(result==2)new_note();else if(result==5)quit();
    else if(result==3||result==4)change([&](Model& m){for(auto& note:m.notes)if(note.mode==Mode::desktop)note.locked=result==3;});
}
}
