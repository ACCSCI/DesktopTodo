#include "todo/windows.hpp"
#include "todo/sqlite_store.hpp"

namespace todo::win {
bool migrate_legacy_data(const std::filesystem::path& source,const std::filesystem::path& destination) {
    auto old=source/L"todo.sqlite",target=destination/L"todo.sqlite";
    if(std::filesystem::exists(target)||!std::filesystem::exists(old))return false;
    auto backups=std::filesystem::weakly_canonical(source/L"backups");
    auto inside=std::filesystem::weakly_canonical(destination).lexically_relative(backups);
    if(!inside.empty()&&*inside.begin()!=L"..")throw std::invalid_argument("存储位置不能放在现有备份目录内。");
    std::filesystem::create_directories(destination);
    auto temporary=destination/(L".migration-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".sqlite");
    if(std::filesystem::exists(temporary))throw std::runtime_error("Migration temporary file already exists");
    struct Cleanup {
        std::filesystem::path file;
        ~Cleanup(){for(auto suffix:{L"",L"-wal",L"-shm"}){std::error_code ignored;std::filesystem::remove(file.wstring()+suffix,ignored);}}
    } cleanup{temporary};
    {
        SqliteStore original(path_utf8(old));
        // SQLite's backup API includes committed data still residing in WAL.
        original.backup(path_utf8(temporary));
        SqliteStore verified(path_utf8(temporary));verified.load();
    }
    if(std::filesystem::exists(backups)) {
        std::filesystem::create_directories(destination/L"backups");
        std::filesystem::copy(backups,destination/L"backups",
            std::filesystem::copy_options::recursive|std::filesystem::copy_options::skip_existing|std::filesystem::copy_options::skip_symlinks);
    }
    // No replacement: concurrent or existing portable data must never be overwritten.
    if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error(last_error("Publish migrated database"));
    return true;
}
}
