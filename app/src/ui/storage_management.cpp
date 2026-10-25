#include "todo/application.hpp"
#include "todo/maintenance.hpp"
#include <shlobj.h>
#include <shellapi.h>

namespace todo {
namespace {
int CALLBACK initial_folder(HWND window,UINT message,LPARAM,LPARAM data) {
    if(message==BFFM_INITIALIZED)SendMessageW(window,BFFM_SETSELECTIONW,TRUE,data);
    return 0;
}
}
void Application::relocate_storage(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);auto target=std::filesystem::weakly_canonical(directory);
    auto old=options_.data_directory;if(target==old)return;
    if(std::filesystem::exists(target/L"todo.sqlite"))throw std::runtime_error("目标位置已有数据库。请选择空目录，已有数据可使用恢复备份功能导入。");
    auto key=win::data_identity(target);auto name=L"Local\\DesktopTodo-"+std::to_wstring(key);
    HANDLE handle=CreateMutexW(nullptr,FALSE,name.c_str());DWORD error=GetLastError();
    if(!handle)throw std::runtime_error(win::last_error("Lock target storage"));
    struct Close{HANDLE handle;~Close(){if(handle)CloseHandle(handle);}} close{handle};
    if(error==ERROR_ALREADY_EXISTS)throw std::runtime_error("目标数据库正由另一个 DesktopTodo 实例使用。");
    bool created=false;
    try {
        if(!win::migrate_legacy_data(old,target))throw std::runtime_error("Cannot migrate current database");
        created=true;
        auto next_store=std::make_unique<SqliteStore>(win::path_utf8(target/L"todo.sqlite"));
        auto next_controller=std::make_unique<Controller>(*next_store);
        if(next_controller->model()!=model())throw std::runtime_error("迁移后的数据校验不一致，已保留原数据库。");
        AppOptions next_options=options_;next_options.data_directory=target;
        win::write_storage_directory(options_.settings_file.parent_path(),options_.settings_file,target);
        controller_.swap(next_controller);store_.swap(next_store);std::swap(options_,next_options);
        if(storage_mutex_)CloseHandle(storage_mutex_);storage_mutex_=handle;close.handle=nullptr;
        SetPropW(manager_,L"DesktopTodo.Identity",reinterpret_cast<HANDLE>(key));
        next_controller.reset();next_store.reset();created=false;
    } catch(...) {
        if(created)for(auto suffix:{L"",L"-wal",L"-shm"}) {
            std::error_code ignored;std::filesystem::remove((target/L"todo.sqlite").wstring()+suffix,ignored);
        }
        throw;
    }
    // Both stores are closed/verified before removing the obsolete active files.
    bool leftovers=false;
    for(auto suffix:{L"",L"-wal",L"-shm"}) {
        std::error_code error;std::filesystem::remove((old/L"todo.sqlite").wstring()+suffix,error);
        if(error)leftovers=true;
    }
    ++revision_;last_error_=leftovers?L"存储位置已切换；旧数据库仍被其他程序占用，请关闭后手动删除旧库。":L"";refresh();
}
bool Application::clear_all_data(const std::function<bool(int)>& confirm) {
    if(!confirm_data_clear(confirm))return false;
    try{store_->clear_data();}catch(...){controller_->reload();refresh();throw;}
    controller_->reload();++revision_;last_error_.clear();refresh();return true;
}
void Application::compact_storage(){store_->compact();last_error_.clear();refresh();}
void Application::storage_menu(POINT point) {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING|MF_GRAYED,0,(L"当前数据库："+(options_.data_directory/L"todo.sqlite").wstring()).c_str());
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,1,L"更改存储目录并迁移…");AppendMenuW(menu,MF_STRING,2,L"打开当前数据目录");
    AppendMenuW(menu,MF_STRING,3,L"整理数据库 / 回收空间");
    AppendMenuW(menu,MF_STRING,4,L"清空全部便签和待办（三次确认）…");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,5,L"手动导出备份…");
    AppendMenuW(menu,MF_STRING,6,L"从备份恢复…");
    AppendMenuW(menu,MF_STRING|MF_GRAYED,0,L"自动备份：每日首次启动，保留最近 7 份");
    int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,manager_,nullptr);DestroyMenu(menu);
    if(command==1) {
        auto initial=options_.data_directory.wstring();BROWSEINFOW info{};info.hwndOwner=manager_;
        info.lpszTitle=L"选择新的数据库目录；数据和备份将自动迁移。已有数据库不会被覆盖。";
        info.ulFlags=BIF_RETURNONLYFSDIRS|BIF_USENEWUI;info.lpfn=initial_folder;info.lParam=reinterpret_cast<LPARAM>(initial.c_str());
        auto selection=SHBrowseForFolderW(&info);if(!selection)return;
        wchar_t path[MAX_PATH]{};bool valid=SHGetPathFromIDListW(selection,path);CoTaskMemFree(selection);
        if(valid)relocate_storage(path);
    } else if(command==2)ShellExecuteW(manager_,L"open",options_.data_directory.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    else if(command==3){compact_storage();MessageBoxW(manager_,L"整理完成，便签和待办内容不变。",L"数据库",MB_OK|MB_ICONINFORMATION);}
    else if(command==4)clear_all_data([&](int stage) {
        auto title=L"清空数据 · 第 "+std::to_wstring(stage)+L" / 3 次确认";
        return MessageBoxW(manager_,L"将永久清空当前数据库中的全部便签和待办，不能撤销。\n此次操作不创建备份。已有手动和自动备份保留。\n\n确定继续？",title.c_str(),MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)==IDYES;
    });
    else if(command==5)backup_dialog();else if(command==6)restore_dialog();
}
}
