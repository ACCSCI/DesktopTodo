#include "todo/application.hpp"
#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <commdlg.h>

namespace todo {
void Application::note_menu(Id id,POINT point) {
    const auto* current=find_note(id);if(!current)return;
    Note copy=*current;
    HMENU menu=CreatePopupMenu(),modes=CreatePopupMenu(),corners=CreatePopupMenu(),offsets=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING,1,L"新建便签…");AppendMenuW(menu,MF_STRING,2,L"修改标题…");
    AppendMenuW(menu,MF_STRING,3,L"便签颜色…");
    for(int i=0;i<3;++i)AppendMenuW(modes,MF_STRING|(copy.mode==Mode(i)?MF_CHECKED:0),100+i,ui::mode_name(Mode(i)).c_str());
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(modes),L"显示模式");
    AppendMenuW(menu,MF_STRING|(copy.mode==Mode::desktop?0:MF_GRAYED),4,copy.locked?L"解锁便签":L"锁定便签");
    const wchar_t* corner_names[]={L"左上角",L"右上角",L"左下角",L"右下角"};
    for(int i=0;i<4;++i)AppendMenuW(corners,MF_STRING|(copy.corner==Corner(i)?MF_CHECKED:0),200+i,corner_names[i]);
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(corners),L"锁定 / 解锁按钮位置");
    const int values[]={0,4,8,16,24};
    for(int i=0;i<5;++i)AppendMenuW(offsets,MF_STRING|(copy.lock_offset==values[i]?MF_CHECKED:0),250+i,(std::to_wstring(values[i])+L" 像素（随 DPI 缩放）").c_str());
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(offsets),L"锁定 / 解锁按钮边距");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING|(can_undo()?0:MF_GRAYED),5,L"撤销    Ctrl+Z");
    AppendMenuW(menu,MF_STRING|(can_redo()?0:MF_GRAYED),6,L"重做    Ctrl+Y");
    AppendMenuW(menu,MF_STRING,7,L"清理已完成待办");AppendMenuW(menu,MF_STRING,8,L"管理全部便签");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,9,L"删除此便签（可撤销）");
    HWND owner=manager_;
    auto view=views_.find(id);if(view!=views_.end()&&view->second->window())owner=view->second->window();
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,owner,nullptr);DestroyMenu(menu);
    if(result==1)new_note();
    else if(result==2){auto title=ui::prompt(manager_,L"修改便签标题",win::wide(copy.title),128);if(title)change([&](Model& m){rename_note(m,id,win::utf8(*title));});}
    else if(result==3){auto color=ui::choose_color(manager_,copy.color);if(color)change([&](Model& m){set_note_color(m,id,*color);});}
    else if(result==4)change([&](Model& m){set_lock(m,id,!copy.locked);});
    else if(result==5)undo();else if(result==6)redo();
    else if(result==7)change([&](Model& m){remove_completed(m,id);});
    else if(result==8)show_manager();else if(result==9)remove_note(id);
    else if(result>=100&&result<103)change([&](Model& m){set_mode(m,id,Mode(result-100));});
    else if(result>=200&&result<204)change([&](Model& m){set_lock_position(m,id,Corner(result-200),copy.lock_offset);});
    else if(result>=250&&result<255)change([&](Model& m){set_lock_position(m,id,copy.corner,values[result-250]);});
}
void Application::backup_dialog() {
    wchar_t path[32768]=L"DesktopTodo-backup.sqlite";
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=manager_;
    dialog.lpstrFilter=L"DesktopTodo 备份 (*.sqlite)\0*.sqlite\0\0";dialog.lpstrFile=path;
    dialog.nMaxFile=DWORD(std::size(path));dialog.lpstrDefExt=L"sqlite";dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST;
    if(GetSaveFileNameW(&dialog)) {
        auto source=std::filesystem::weakly_canonical(options_.data_directory/L"todo.sqlite");
        if(std::filesystem::weakly_canonical(path)==source)throw std::invalid_argument("备份文件不能覆盖正在使用的数据库");
        controller_->backup(win::utf8(path));
    }
}
void Application::restore_dialog() {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=manager_;
    dialog.lpstrFilter=L"DesktopTodo 备份 (*.sqlite)\0*.sqlite\0\0";dialog.lpstrFile=path;
    dialog.nMaxFile=DWORD(std::size(path));dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    if(!GetOpenFileNameW(&dialog))return;
    SqliteStore source(win::utf8(path));auto restored=source.load();
    if(MessageBoxW(manager_,L"用这个备份恢复全部便签？当前数据会先自动备份，也可用撤销返回。",L"恢复备份",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    auto directory=options_.data_directory/L"backups";std::filesystem::create_directories(directory);
    controller_->backup(win::path_utf8(directory/(L"before-restore-"+std::to_wstring(ui::now())+L".sqlite")));
    change([&](Model& model){model=restored;for(auto& note:model.notes)note.bounds=win::visible_bounds(note.bounds);});
}
}
