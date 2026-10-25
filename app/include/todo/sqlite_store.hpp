#pragma once
#include "repository.hpp"
struct sqlite3;

namespace todo {
class SqliteStore final : public Repository {
    sqlite3* db_ = nullptr;
public:
    explicit SqliteStore(const std::string& path);
    ~SqliteStore() override;
    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;
    Model load() override;
    void save(const Model& model) override;
    void backup(const std::string& path) override;
    void compact();
    void clear_data();
};
}
