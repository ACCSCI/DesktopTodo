#pragma once
#include "model.hpp"
#include <string>

namespace todo {
class Repository {
public:
    virtual ~Repository() = default;
    virtual Model load() = 0;
    virtual void save(const Model& model) = 0;
    virtual void backup(const std::string& path) = 0;
};
class StartupPort {
public:
    virtual ~StartupPort() = default;
    virtual bool enabled() const = 0;
    virtual void set_enabled(bool enabled) = 0;
};
}
