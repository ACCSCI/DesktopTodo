#include "test.hpp"
#include "todo/windows.hpp"
#include "todo/sqlite_store.hpp"
#include "todo/actions.hpp"
#include <fstream>
using namespace todo;

namespace {
struct Directories {
    std::filesystem::path root=std::filesystem::temp_directory_path()/
        (L"DesktopTodo-portable-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    std::filesystem::path old=root/L"旧数据",target=root/L"程序目录"/L"data";
    Directories(){std::filesystem::create_directories(old);}
    ~Directories(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}
};
}
TEST_CASE("portable data lives beside the executable independently of the working directory") {
    CHECK(win::portable_data_directory(L"D:\\我的软件\\DesktopTodo.exe")==std::filesystem::path(L"D:\\我的软件"));
    CHECK(win::default_data_directory()==std::filesystem::path(win::executable_path()).parent_path());
}
TEST_CASE("legacy migration preserves every model field and backups without removing the original") {
    Directories folders;Model model;auto id=add_note(model,"迁移测试 🎯");add_task(model,id,"保留全部待办");
    {
        SqliteStore source(win::path_utf8(folders.old/L"todo.sqlite"));source.save(model);
        std::filesystem::create_directories(folders.old/L"backups");
        source.backup(win::path_utf8(folders.old/L"backups"/L"daily.sqlite"));
        CHECK(win::migrate_legacy_data(folders.old,folders.target));
        CHECK(source.load()==model);
    }
    SqliteStore migrated(win::path_utf8(folders.target/L"todo.sqlite"));CHECK(migrated.load()==model);
    SqliteStore backup(win::path_utf8(folders.target/L"backups"/L"daily.sqlite"));CHECK(backup.load()==model);
    CHECK(std::filesystem::exists(folders.old/L"todo.sqlite"));
}
TEST_CASE("existing portable data always wins and an absent legacy store creates no phantom database") {
    Directories folders;
    CHECK(!win::migrate_legacy_data(folders.old,folders.target));
    CHECK(!std::filesystem::exists(folders.target/L"todo.sqlite"));
    std::filesystem::create_directories(folders.target);
    Model model;add_note(model,"Portable is authoritative");
    SqliteStore target(win::path_utf8(folders.target/L"todo.sqlite"));target.save(model);
    std::ofstream(folders.old/L"todo.sqlite")<<"corrupt old database";
    CHECK(!win::migrate_legacy_data(folders.old,folders.target));CHECK(target.load()==model);
}
TEST_CASE("failed migration cannot publish an empty portable database or erase the source") {
    Directories folders;std::ofstream(folders.old/L"todo.sqlite")<<"invalid sqlite data";
    THROWS_AS(win::migrate_legacy_data(folders.old,folders.target),std::runtime_error);
    CHECK(!std::filesystem::exists(folders.target/L"todo.sqlite"));
    CHECK(std::filesystem::file_size(folders.old/L"todo.sqlite")>0);
}
TEST_CASE("storage settings use relative paths within the portable directory and preserve Unicode locations") {
    Directories folders;auto settings=folders.root/L"DesktopTodo.ini";
    CHECK(win::read_storage_directory(folders.root,settings)==folders.root);
    auto target=folders.root/L"自定义数据";std::filesystem::create_directories(target);
    win::write_storage_directory(folders.root,settings,target);
    CHECK(win::read_storage_directory(folders.root,settings)==std::filesystem::weakly_canonical(target));
    std::ifstream file(settings);std::string contents((std::istreambuf_iterator<char>(file)),{});
    CHECK(contents.find("directory=")!=std::string::npos);
    CHECK(contents.find(win::path_utf8(folders.root))==std::string::npos);
    file.close();
    win::write_storage_directory(folders.root,settings,folders.root);
    CHECK(win::read_storage_directory(folders.root,settings)==std::filesystem::weakly_canonical(folders.root));
}
TEST_CASE("migration rejects destinations inside its own backup tree") {
    Directories folders;SqliteStore store(win::path_utf8(folders.old/L"todo.sqlite"));
    auto target=folders.old/L"backups"/L"nested";
    THROWS_AS(win::migrate_legacy_data(folders.old,target),std::invalid_argument);
    CHECK(!std::filesystem::exists(target/L"todo.sqlite"));
}
