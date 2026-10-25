#include "todo/application.hpp"
#include <commctrl.h>
#include <shellapi.h>
#include <objbase.h>
#include <sstream>
#ifdef TODO_USE_NIM
#include "todo/nim_surface.hpp"
#endif

namespace {
struct Existing {std::uintptr_t key;HWND window=nullptr;};
BOOL CALLBACK find_existing(HWND window,LPARAM data) {
    auto* result=reinterpret_cast<Existing*>(data);wchar_t cls[128]{};GetClassNameW(window,cls,int(std::size(cls)));
    if(wcscmp(cls,L"DesktopTodo.Manager")==0&&reinterpret_cast<std::uintptr_t>(GetPropW(window,L"DesktopTodo.Identity"))==result->key){result->window=window;return FALSE;}
    return TRUE;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{};controls.dwSize=sizeof(controls);controls.dwICC=ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    HANDLE mutex=nullptr;
    try {
        todo::AppOptions options;int count=0;LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
        for(int i=1;i<count;++i) {
            std::wstring value=args[i];
            if(value==L"--startup")options.startup=true;
            else if(value==L"--automation")options.automation=true;
            else if(value==L"--data-dir"&&i+1<count)options.data_directory=args[++i];
            else {LocalFree(args);throw std::invalid_argument("参数：--startup、--automation、--data-dir 路径");}
        }
        LocalFree(args);
        bool portable=options.data_directory.empty();
        if(portable) {
            auto base=todo::win::default_data_directory();options.settings_file=base/L"DesktopTodo.ini";
            options.data_directory=todo::win::read_storage_directory(base,options.settings_file);
        } else options.settings_file=options.data_directory/L"DesktopTodo.ini";
        if(options.automation)options.control_directory=options.settings_file.parent_path();
        std::filesystem::create_directories(options.data_directory);
        options.data_directory=std::filesystem::weakly_canonical(options.data_directory);
        auto key=todo::win::data_identity(options.data_directory);
        std::wstring name=L"Local\\DesktopTodo-"+std::to_wstring(key);
        mutex=CreateMutexW(nullptr,FALSE,name.c_str());DWORD mutex_error=GetLastError();
        if(!mutex)throw std::runtime_error(todo::win::last_error("Single instance"));
        if(mutex_error==ERROR_ALREADY_EXISTS) {
            Existing existing{key};EnumWindows(find_existing,reinterpret_cast<LPARAM>(&existing));
            if(existing.window&&!options.startup)PostMessageW(existing.window,todo::message_show_manager,0,0);
            CloseHandle(mutex);CoUninitialize();return 0;
        }
        if(portable&&!std::filesystem::exists(options.data_directory/L"todo.sqlite")) {
            auto legacy=todo::win::legacy_data_directory();
            if(std::filesystem::exists(legacy/L"todo.sqlite")) {
                std::wstring legacy_name=L"Local\\DesktopTodo-"+std::to_wstring(todo::win::data_identity(std::filesystem::weakly_canonical(legacy)));
                HANDLE legacy_mutex=CreateMutexW(nullptr,FALSE,legacy_name.c_str());DWORD legacy_error=GetLastError();
                if(!legacy_mutex)throw std::runtime_error(todo::win::last_error("Migration lock"));
                struct Close{HANDLE handle;~Close(){CloseHandle(handle);}} close{legacy_mutex};
                if(legacy_error==ERROR_ALREADY_EXISTS)throw std::runtime_error("请先退出旧版 DesktopTodo，再迁移数据。");
                todo::win::migrate_legacy_data(legacy,options.data_directory);
            }
        }
        int result;
        auto run = [&] {
            todo::Application app(instance,options);
            SetPropW(app.manager(),L"DesktopTodo.Identity",reinterpret_cast<HANDLE>(key));
            int code=app.run();RemovePropW(app.manager(),L"DesktopTodo.Identity");return code;
        };
#ifdef TODO_USE_NIM
        result=todo::run_nim_ui(run);
#else
        result=run();
#endif
        CloseHandle(mutex);CoUninitialize();return result;
    }catch(const std::exception& error) {
        MessageBoxW(nullptr,todo::win::wide(error.what()).c_str(),L"DesktopTodo 无法启动",MB_OK|MB_ICONERROR);
        if(mutex)CloseHandle(mutex);CoUninitialize();return 1;
    }
}
