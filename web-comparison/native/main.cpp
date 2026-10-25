#include <windows.h>
#include <webview/webview.h>
#include "json.hpp"
#include "todo/actions.hpp"
#include "todo/controller.hpp"
#include "todo/sqlite_store.hpp"
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>

using nlohmann::json;
namespace fs = std::filesystem;
struct View { todo::Id id; std::unique_ptr<webview::webview> web; };
struct Host {
    fs::path directory;
    todo::SqliteStore store;
    todo::Controller controller;
    std::vector<View> views;
    std::set<todo::Id> ready;
    unsigned revision = 0;
    HWND manager = nullptr;
    Host(fs::path path) : directory(path), store((path / "todo.sqlite").string()), controller(store) {}
    json note(todo::Id id) {
        const auto& n = todo::note_by_id(controller.model(), id);
        json tasks = json::array();
        for (auto& t : n.tasks) tasks.push_back({{"id",t.id},{"text",t.text},{"icon",t.icon},{"color",t.color},{"status",int(t.status)}});
        return {{"id",n.id},{"title",n.title},{"color",n.color},{"mode",int(n.mode)},
            {"locked",n.locked},{"filter",int(n.filter)},{"tasks",tasks},{"visible",todo::visible_tasks(n)}};
    }
    void publish() {
        json notes = json::array();
        for (auto& v : views) {
            auto item = note(v.id);
            item["window"] = {{"handle",reinterpret_cast<uintptr_t>(v.web->window().value())}};
            notes.push_back(item);
        }
        json out = {{"pid",GetCurrentProcessId()},{"manager",{{"handle",reinterpret_cast<uintptr_t>(manager)}}},
            {"revision",revision},{"readyCount",ready.size()},{"notes",notes}};
        auto temporary = directory / "automation.tmp";
        { std::ofstream stream(temporary); stream << out.dump(); }
        MoveFileExW(temporary.c_str(), (directory / "automation.json").c_str(), MOVEFILE_REPLACE_EXISTING);
    }
    json command(todo::Id id, const json& args) {
        auto action = args.at(0).get<std::string>();
        if (action == "ready") {
            if (args.at(1).get<unsigned>() == revision) ready.insert(id);
            publish(); return nullptr;
        }
        if (action != "snapshot") {
            controller.change([&](todo::Model& m) {
                if (action == "add") todo::add_task(m,id,args.at(1).get<std::string>());
                else if (action == "status") todo::set_status(m,id,args.at(1).get<todo::Id>(),todo::Status(args.at(2).get<int>()));
                else if (action == "filter") todo::set_filter(m,id,todo::Filter(args.at(1).get<int>()));
                else if (action == "lock") todo::set_lock(m,id,args.at(1).get<bool>());
                else if (action == "mode") {
                    // This experiment measures ordinary/topmost windows. Desktop embedding needs a separate compatibility audit.
                    int mode = args.at(1).get<int>();
                    if (mode == 0) throw std::runtime_error("对比实验暂未接入桌面嵌入，请使用窗口或置顶模式");
                    todo::set_mode(m,id,todo::Mode(mode));
                } else if (action == "order") {
                    auto ids = args.at(1).get<std::vector<todo::Id>>();
                    for (auto item : ids) todo::reorder_visible(m,id,item,0);
                } else throw std::runtime_error("Unknown action");
            });
            ++revision;
            ready.erase(id);
            for (auto& v : views) if (v.id == id) {
                bool top = todo::note_by_id(controller.model(),id).mode == todo::Mode::topmost;
                SetWindowPos(static_cast<HWND>(v.web->window().value()),top ? HWND_TOPMOST : HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
            }
        }
        publish();
        return {{"note",note(id)},{"revision",revision}};
    }
};
LRESULT CALLBACK manager_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* host = reinterpret_cast<Host*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message == WM_COPYDATA && host) {
        auto* data = reinterpret_cast<COPYDATASTRUCT*>(lp);
        if (data->dwData != 0x44544F44 || !data->lpData || data->cbData < 1 || data->cbData > 65536) return FALSE;
        std::string payload(static_cast<const char*>(data->lpData),data->cbData);
        if (payload.back() != '\0') return FALSE;
        payload.pop_back();
        if (payload == "quit") { PostQuitMessage(0); return TRUE; }
        if (payload == "snapshot") { host->publish(); return TRUE; }
        if (!payload.empty() && payload.front() == '[') {
            try {
                auto result = host->command(host->views.front().id,json::parse(payload));
                SetWindowPos(static_cast<HWND>(host->views.front().web->window().value()),HWND_TOP,
                    0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
                host->views.front().web->eval("window.receiveNote(" + result.dump() + ")").ensure_ok();
                return TRUE;
            } catch (...) { return FALSE; }
        }
        return FALSE;
    }
    return DefWindowProcW(window,message,wp,lp);
}
int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("Usage: DesktopTodo.WebComparison.exe DATA_DIR FRONTEND [NOTES] [TASKS]");
        fs::path directory = fs::absolute(argv[1]);
        fs::create_directories(directory);
        const auto marker = directory / "comparison-fixture.txt";
        if (fs::exists(directory / "todo.sqlite") && !fs::exists(marker))
            throw std::runtime_error("Refusing to reseed a database not created by this experiment");
        { std::ofstream stream(marker); stream << "Disposable WebView2 comparison fixture\n"; }
        SetEnvironmentVariableW(L"WEBVIEW2_USER_DATA_FOLDER",(directory / "webview-profile").c_str());
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        Host host(directory);
        const int count = argc > 3 ? std::stoi(argv[3]) : 1;
        const int tasks = argc > 4 ? std::stoi(argv[4]) : 10;
        if (count < 1 || count > 20 || tasks < 0 || tasks > 1000) throw std::runtime_error("Invalid workload");
        host.controller.change([&](todo::Model& m) {
            m.notes.clear();
            for (int i=0;i<count;++i) {
                auto id = todo::add_note(m,"测试便签 " + std::to_string(i+1));
                todo::set_mode(m,id,todo::Mode::window);
                for (int t=0;t<tasks;++t) todo::add_task(m,id,"待办任务 " + std::to_string(t+1));
            }
        });
        WNDCLASSW wc{}; wc.lpfnWndProc=manager_proc; wc.lpszClassName=L"TodoComparisonManager";
        wc.hInstance=GetModuleHandleW(nullptr); RegisterClassW(&wc);
        host.manager=CreateWindowW(wc.lpszClassName,L"Comparison IPC",0,0,0,0,0,HWND_MESSAGE,nullptr,wc.hInstance,nullptr);
        SetWindowLongPtrW(host.manager,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(&host));
        for (auto& note : host.controller.model().notes) {
            auto view=std::make_unique<webview::webview>(false,nullptr);
            view->set_title("DesktopTodo · Svelte comparison").ensure_ok();
            view->set_size(340,380,WEBVIEW_HINT_NONE).ensure_ok();
            auto id=note.id;
            view->bind("nativeCommand",[&host,id](std::string args)->std::string {
                try { return host.command(id,json::parse(args)).dump(); }
                catch (const std::exception& e) { return json{{"note",host.note(id)},{"revision",host.revision},{"error",e.what()}}.dump(); }
            }).ensure_ok();
            auto* controller = static_cast<ICoreWebView2Controller*>(view->browser_controller().value());
            ICoreWebView2* browser = nullptr;
            controller->get_CoreWebView2(&browser);
            ICoreWebView2_3* mapped = nullptr;
            constexpr IID mapping_iid{0xA0D6DF20,0x3B92,0x416D,{0xAA,0x0C,0x43,0x7A,0x9C,0x72,0x78,0x57}};
            auto hr = browser->QueryInterface(mapping_iid,reinterpret_cast<void**>(&mapped));
            browser->Release();
            if (FAILED(hr)) throw std::runtime_error("WebView2 virtual host mapping unavailable");
            hr = mapped->SetVirtualHostNameToFolderMapping(L"todo-comparison.example",fs::absolute(argv[2]).c_str(),COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
            mapped->Release();
            if (FAILED(hr)) throw std::runtime_error("WebView2 mapping failed");
            view->navigate("https://todo-comparison.example/index.html").ensure_ok();
            host.views.push_back({id,std::move(view)});
        }
        host.publish();
        host.views.front().web->run().ensure_ok();
        host.views.clear();
        DestroyWindow(host.manager);
        return 0;
    } catch (const std::exception& e) {
        std::ofstream(".tools/web-comparison/error.log") << e.what();
        return 1;
    }
}
