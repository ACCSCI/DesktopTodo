#pragma once
#include "repository.hpp"
#include <deque>
#include <functional>

namespace todo {
class Controller {
    Repository& repository_;
    Model model_;
    std::deque<Model> undo_, redo_;
    void remember(std::deque<Model>& history, Model model);
public:
    explicit Controller(Repository& repository);
    const Model& model() const { return model_; }
    bool change(const std::function<void(Model&)>& action, bool record_undo = true);
    bool undo();
    bool redo();
    void reload();
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    void backup(const std::string& path) { repository_.backup(path); }
};
}
