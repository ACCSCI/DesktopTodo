#include "test.hpp"
#include "todo/actions.hpp"
#include "todo/sqlite_store.hpp"
#include <sqlite3.h>
#include <chrono>
#include <filesystem>
using namespace todo;

struct TempDatabase {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("desktop-todo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".sqlite");
    ~TempDatabase() {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(path.string() + "-wal", error);
        std::filesystem::remove(path.string() + "-shm", error);
    }
    std::string string() const { return path.string(); }
};
static Model sample() {
    Model m;
    Id first = add_note(m, "工作：项目 🎯"), second = add_note(m, "生活");
    Id a = add_task(m, first, "第一行\n第二行：包含引号 \" 和反斜线 \\", 123);
    add_task(m, first, "另一个任务", 125);
    set_status(m, first, a, Status::done, 345);
    set_task_icon(m, first, a, "✅");
    set_task_color(m, first, a, 0x0123AB);
    set_note_color(m, first, 0xEAF4FF);
    set_filter(m, first, Filter::unfinished);
    set_mode(m, second, Mode::topmost);
    set_lock(m, first, true);
    set_lock_position(m, first, Corner::bottom_left, 16);
    set_bounds(m, first, {-1280,60,440,520,192});
    return m;
}

TEST_CASE("SQLite round-trips every field, Unicode text and manual task order") {
    TempDatabase file;
    auto model = sample();
    {
        SqliteStore store(file.string());
        CHECK(store.load() == Model{});
        store.save(model);
        CHECK(store.load() == model);
    }
    SqliteStore reopened(file.string());
    CHECK(reopened.load() == model);
}

TEST_CASE("an injected SQLite write failure rolls back deletes and partial inserts") {
    TempDatabase file;
    SqliteStore store(file.string());
    auto original = sample();
    store.save(original);
    sqlite3* other = nullptr;
    CHECK(sqlite3_open(file.string().c_str(), &other) == SQLITE_OK);
    CHECK(sqlite3_exec(other, "CREATE TRIGGER deny_task BEFORE INSERT ON tasks BEGIN SELECT RAISE(ABORT,'injected write failure'); END;",
                       nullptr, nullptr, nullptr) == SQLITE_OK);
    auto candidate = original;
    rename_note(candidate, candidate.notes.front().id, "Must not persist");
    THROWS_AS(store.save(candidate), std::runtime_error);
    CHECK(store.load() == original);
    sqlite3_close(other);
}

TEST_CASE("SQLite backup is a complete independently reopenable snapshot") {
    TempDatabase source, destination;
    auto model = sample();
    SqliteStore store(source.string());
    store.save(model);
    store.backup(destination.string());
    auto changed = model;
    add_note(changed, "After backup");
    store.save(changed);
    SqliteStore backup(destination.string());
    CHECK(backup.load() == model);
    CHECK(store.load() == changed);
}

TEST_CASE("SQLite refuses invalid models without replacing previously saved data") {
    SqliteStore store(":memory:");
    auto model = sample();
    store.save(model);
    auto invalid = model;
    invalid.notes.front().bounds.width = 1;
    THROWS_AS(store.save(invalid), std::invalid_argument);
    CHECK(store.load() == model);
}

TEST_CASE("newer database schemas are rejected rather than overwritten") {
    TempDatabase file;
    sqlite3* db = nullptr;
    CHECK(sqlite3_open(file.string().c_str(), &db) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "PRAGMA user_version=99", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(db);
    THROWS_AS(SqliteStore rejected(file.string()), std::runtime_error);
}

TEST_CASE("corrupted stored status values are surfaced on load") {
    TempDatabase file;
    SqliteStore store(file.string());
    store.save(sample());
    sqlite3* db = nullptr;
    CHECK(sqlite3_open(file.string().c_str(), &db) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "UPDATE tasks SET status=99", nullptr, nullptr, nullptr) == SQLITE_OK);
    sqlite3_close(db);
    THROWS_AS(store.load(), std::invalid_argument);
}

TEST_CASE("corrupted SQLite numbers cannot wrap into valid colors, enums or dimensions") {
    TempDatabase file;
    SqliteStore store(file.string());
    auto original=sample();
    sqlite3* db=nullptr;
    CHECK(sqlite3_open(file.string().c_str(),&db)==SQLITE_OK);
    struct Close {sqlite3* db;~Close(){sqlite3_close(db);}} close{db};
    const char* corruptions[]={
        "UPDATE notes SET color=4294967296",
        "UPDATE notes SET mode=4294967296",
        "UPDATE notes SET width=4294967636",
        "UPDATE notes SET locked=2",
        "UPDATE notes SET x=1.5",
        "UPDATE tasks SET color=-4294967296",
        "UPDATE tasks SET status=4294967296",
        "UPDATE meta SET value=2 WHERE key='lock_on_start'"
    };
    for(auto sql:corruptions) {
        store.save(original);
        CHECK(sqlite3_exec(db,sql,nullptr,nullptr,nullptr)==SQLITE_OK);
        THROWS_AS(store.load(),std::invalid_argument);
    }
    store.save(original);
    CHECK(store.load()==original);
}
