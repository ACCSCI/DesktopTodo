#pragma once
#include "controller.hpp"
#include "actions.hpp"
#include "sqlite_store.hpp"
#include "windows.hpp"
#include <memory>
#include <unordered_map>

namespace todo {
class NoteView;
struct AppOptions {
    std::filesystem::path data_directory;
    std::filesystem::path settings_file,control_directory;
    bool startup = false;
    bool automation = false;
};
inline constexpr UINT message_sync = WM_APP + 10;
inline constexpr UINT message_tray = WM_APP + 11;
inline constexpr UINT message_show_manager = WM_APP + 12;
inline constexpr UINT message_readonly_wheel = WM_APP + 13;

class Application {
    HINSTANCE instance_;
    AppOptions options_;
    std::unique_ptr<SqliteStore> store_;
    std::unique_ptr<Controller> controller_;
    std::unique_ptr<win::Startup> startup_;
    std::unordered_map<Id,std::unique_ptr<NoteView>> views_;
    HWND manager_ = nullptr, list_ = nullptr, startup_check_ = nullptr, lock_check_ = nullptr;
    HFONT manager_font_ = nullptr;
    UINT taskbar_created_ = 0;
    bool quitting_ = false, synchronizing_ = false;
    int shell_attempts_ = 0;
    std::wstring last_error_;
    std::uint64_t revision_ = 0;
    std::wstring startup_test_key_;
    HANDLE storage_mutex_=nullptr;
    HHOOK wheel_hook_=nullptr;
    static Application* wheel_owner_;
    static LRESULT CALLBACK wheel_proc(int,WPARAM,LPARAM);
    void update_wheel_hook();
    void create_manager();
    void layout_manager();
    void refresh_manager();
    void manager_command(int id);
    void tray(bool add);
    void synchronize();
    void write_snapshot();
    LRESULT automation(const COPYDATASTRUCT& data);
    static LRESULT CALLBACK manager_proc(HWND,UINT,WPARAM,LPARAM);
public:
    Application(HINSTANCE instance, AppOptions options);
    ~Application();
    int run();
    HINSTANCE instance() const { return instance_; }
    HWND manager() const { return manager_; }
    const Model& model() const { return controller_->model(); }
    const Note* find_note(Id id) const;
    bool change(const std::function<void(Model&)>& action, bool undo = true);
    void undo();
    void redo();
    bool can_undo() const { return controller_->can_undo(); }
    bool can_redo() const { return controller_->can_redo(); }
    void show_manager();
    void quit();
    void error(const std::exception& error);
    void refresh();
    void shell_changed();
    void new_note();
    void note_menu(Id note, POINT point);
    void tray_menu(POINT point);
    void backup_dialog();
    void restore_dialog();
    void storage_menu(POINT point);
    void relocate_storage(const std::filesystem::path& directory);
    bool clear_all_data(const std::function<bool(int)>& confirm);
    void compact_storage();
    NoteView* view_at(POINT screen);
    bool locked_wheel(POINT screen,int delta,HWND hit=nullptr);
    const AppOptions& options() const { return options_; }
    void remove_note(Id id);
};
}
