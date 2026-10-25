#include "todo/nim_surface.hpp"
#include "todo/windows.hpp"
#include "duilib/duilib.h"
#include <exception>
#include <filesystem>

namespace todo {
int run_nim_ui(const std::function<int()>& run) {
    class Thread final : public ::ui::FrameworkThread {
        const std::function<int()>& run_;
    public:
        int result=1;
        std::exception_ptr error;
        explicit Thread(const std::function<int()>& fn) : FrameworkThread(L"DesktopTodo",::ui::kThreadUI),run_(fn) {}
        bool OnInit() override {
            auto path=std::filesystem::path(win::executable_path()).parent_path()/L"resources";
            return ::ui::GlobalManager::Instance().Startup(::ui::LocalFilesResParam(::ui::FilePath(path.wstring())));
        }
        void OnRunMessageLoop() override {
            try {result=run_();}catch(...){error=std::current_exception();}
        }
        void OnCleanup() override {::ui::GlobalManager::Instance().Shutdown();}
    } thread(run);
    if(!thread.RunMessageLoop())throw std::runtime_error("nim_duilib resource initialization failed");
    if(thread.error)std::rethrow_exception(thread.error);
    return thread.result;
}
}
