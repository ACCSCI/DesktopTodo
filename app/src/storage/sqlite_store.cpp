#include "todo/sqlite_store.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <limits>

namespace todo {
namespace {
void check(sqlite3* db, int code) {
    if (code != SQLITE_OK && code != SQLITE_ROW && code != SQLITE_DONE)
        throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(db));
}
void sql(sqlite3* db, const char* text) {
    char* error = nullptr;
    int rc = sqlite3_exec(db, text, nullptr, nullptr, &error);
    std::string message = error ? error : "";
    sqlite3_free(error);
    if (rc != SQLITE_OK) throw std::runtime_error("SQLite: " + message);
}
class Statement {
    sqlite3* db_;
    sqlite3_stmt* statement_ = nullptr;
public:
    Statement(sqlite3* db, const char* text) : db_(db) {
        check(db_, sqlite3_prepare_v2(db_, text, -1, &statement_, nullptr));
    }
    ~Statement() { sqlite3_finalize(statement_); }
    void number(int field, std::int64_t value) { check(db_, sqlite3_bind_int64(statement_, field, value)); }
    void text(int field, const std::string& value) {
        check(db_, sqlite3_bind_text(statement_, field, value.data(), int(value.size()), SQLITE_TRANSIENT));
    }
    bool row() {
        int rc = sqlite3_step(statement_);
        check(db_, rc);
        return rc == SQLITE_ROW;
    }
    void write() {
        if (row()) throw std::runtime_error("Unexpected SQLite result");
        check(db_, sqlite3_reset(statement_));
        check(db_, sqlite3_clear_bindings(statement_));
    }
    std::int64_t integer(int field) {
        if(sqlite3_column_type(statement_,field)!=SQLITE_INTEGER)
            throw std::invalid_argument("Stored number is not an integer");
        return sqlite3_column_int64(statement_, field);
    }
    std::int64_t bounded(int field,std::int64_t low,std::int64_t high) {
        auto value=integer(field);
        if(value<low||value>high)throw std::invalid_argument("Stored number is out of range");
        return value;
    }
    int dimension(int field) {
        return int(bounded(field,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()));
    }
    bool boolean(int field) { return bounded(field,0,1)!=0; }
    Color color(int field) { return Color(bounded(field,0,0xFFFFFF)); }
    std::string string(int field) {
        const auto* bytes = sqlite3_column_text(statement_, field);
        return bytes ? std::string(reinterpret_cast<const char*>(bytes), sqlite3_column_bytes(statement_, field)) : "";
    }
};
}

SqliteStore::SqliteStore(const std::string& path) {
    int rc = sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        std::string error = db_ ? sqlite3_errmsg(db_) : "Cannot open database";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error(error);
    }
    try {
        sqlite3_busy_timeout(db_, 1500);
        sql(db_, "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;");
        {
            Statement version(db_, "PRAGMA user_version");
            if (!version.row() || version.integer(0) > 1) throw std::runtime_error("Database version is newer than this application");
        }
        sql(db_, R"SQL(
            BEGIN IMMEDIATE;
            CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS notes(
                id INTEGER PRIMARY KEY, position INTEGER NOT NULL UNIQUE,
                title TEXT NOT NULL, color INTEGER NOT NULL, mode INTEGER NOT NULL,
                locked INTEGER NOT NULL, corner INTEGER NOT NULL, lock_offset INTEGER NOT NULL,
                filter INTEGER NOT NULL, x INTEGER NOT NULL, y INTEGER NOT NULL,
                width INTEGER NOT NULL, height INTEGER NOT NULL, dpi INTEGER NOT NULL);
            CREATE TABLE IF NOT EXISTS tasks(
                id INTEGER PRIMARY KEY, note_id INTEGER NOT NULL REFERENCES notes(id) ON DELETE CASCADE,
                position INTEGER NOT NULL, text TEXT NOT NULL, icon TEXT NOT NULL,
                color INTEGER NOT NULL, status INTEGER NOT NULL,
                created INTEGER NOT NULL, completed INTEGER NOT NULL,
                UNIQUE(note_id, position));
            PRAGMA user_version=1;
            COMMIT;
        )SQL");
    } catch (...) {
        sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
SqliteStore::~SqliteStore() { if (db_) sqlite3_close(db_); }
void SqliteStore::compact() {
    sql(db_,"PRAGMA wal_checkpoint(TRUNCATE); VACUUM;");
}
void SqliteStore::clear_data() {
    sql(db_,"PRAGMA secure_delete=ON;");
    save(Model{});
    compact();
}

Model SqliteStore::load() {
    Model result;
    // One consistent snapshot, including when another connection makes a backup.
    sql(db_, "BEGIN");
    try {
        Statement meta(db_, "SELECT key,value FROM meta");
        while (meta.row()) {
            if (meta.string(0) == "next_id") result.next_id = meta.integer(1);
            if (meta.string(0) == "lock_on_start") result.lock_on_start = meta.boolean(1);
        }
        Statement notes(db_, "SELECT id,title,color,mode,locked,corner,lock_offset,filter,x,y,width,height,dpi FROM notes ORDER BY position");
        while (notes.row()) {
            Note n;
            n.id = notes.integer(0); n.title = notes.string(1); n.color = notes.color(2);
            n.mode = Mode(notes.bounded(3,0,2)); n.locked = notes.boolean(4);
            n.corner = Corner(notes.bounded(5,0,3)); n.lock_offset = notes.dimension(6);
            n.filter = Filter(notes.bounded(7,0,4));
            n.bounds = {notes.dimension(8), notes.dimension(9), notes.dimension(10), notes.dimension(11), notes.dimension(12)};
            result.notes.push_back(std::move(n));
        }
        Statement tasks(db_, "SELECT id,note_id,text,icon,color,status,created,completed FROM tasks ORDER BY note_id,position");
        while (tasks.row()) {
            Task t;
            t.id = tasks.integer(0); t.text = tasks.string(2); t.icon = tasks.string(3);
            t.color = tasks.color(4); t.status = Status(tasks.bounded(5,0,2));
            t.created = tasks.integer(6); t.completed = tasks.integer(7);
            note_by_id(result, tasks.integer(1)).tasks.push_back(std::move(t));
        }
        validate(result);
        sql(db_, "COMMIT");
        return result;
    } catch (...) { sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); throw; }
}

void SqliteStore::save(const Model& model) {
    validate(model);
    sql(db_, "BEGIN IMMEDIATE");
    try {
        sql(db_, "DELETE FROM tasks; DELETE FROM notes; DELETE FROM meta;");
        Statement meta(db_, "INSERT INTO meta(key,value) VALUES(?,?)");
        meta.text(1, "next_id"); meta.number(2, model.next_id); meta.write();
        meta.text(1, "lock_on_start"); meta.number(2, model.lock_on_start); meta.write();
        Statement note(db_, "INSERT INTO notes VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        Statement task(db_, "INSERT INTO tasks VALUES(?,?,?,?,?,?,?,?,?)");
        for (std::size_t i = 0; i < model.notes.size(); ++i) {
            const auto& n = model.notes[i];
            note.number(1, n.id); note.number(2, i); note.text(3, n.title);
            note.number(4, n.color); note.number(5, int(n.mode)); note.number(6, n.locked);
            note.number(7, int(n.corner)); note.number(8, n.lock_offset); note.number(9, int(n.filter));
            note.number(10, n.bounds.x); note.number(11, n.bounds.y); note.number(12, n.bounds.width);
            note.number(13, n.bounds.height); note.number(14, n.bounds.dpi); note.write();
            for (std::size_t j = 0; j < n.tasks.size(); ++j) {
                const auto& t = n.tasks[j];
                task.number(1, t.id); task.number(2, n.id); task.number(3, j);
                task.text(4, t.text); task.text(5, t.icon); task.number(6, t.color);
                task.number(7, int(t.status)); task.number(8, t.created); task.number(9, t.completed);
                task.write();
            }
        }
        sql(db_, "COMMIT");
    } catch (...) { sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr); throw; }
}

void SqliteStore::backup(const std::string& path) {
    sqlite3* destination = nullptr;
    int rc = sqlite3_open_v2(path.c_str(), &destination, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        if (destination) sqlite3_close(destination);
        throw std::runtime_error("Cannot open backup destination");
    }
    auto* backup = sqlite3_backup_init(destination, "main", db_, "main");
    if (!backup) {
        std::string error = sqlite3_errmsg(destination);
        sqlite3_close(destination);
        throw std::runtime_error(error);
    }
    rc = sqlite3_backup_step(backup, -1);
    int finish = sqlite3_backup_finish(backup);
    std::string error = sqlite3_errmsg(destination);
    sqlite3_close(destination);
    if (rc != SQLITE_DONE || finish != SQLITE_OK) throw std::runtime_error("Backup failed: " + error);
}
}
