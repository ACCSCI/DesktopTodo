#include "todo/controller.hpp"

namespace todo {
namespace {
std::size_t weight(const Model& model) {
    std::size_t bytes = sizeof(model);
    for (const auto& note : model.notes) {
        bytes += sizeof(note) + note.title.size();
        for (const auto& task : note.tasks) bytes += sizeof(task) + task.text.size() + task.icon.size();
    }
    return bytes;
}
}
Controller::Controller(Repository& repository) : repository_(repository), model_(repository.load()) {
    validate(model_);
}
void Controller::reload() {
    auto model=repository_.load();validate(model);model_=std::move(model);undo_.clear();redo_.clear();
}
void Controller::remember(std::deque<Model>& history, Model model) {
    // Bound both history count and retained text memory for desktop idleness.
    history.push_back(std::move(model));
    std::size_t total = 0;
    for (const auto& item : history) total += weight(item);
    while (history.size() > 1 && (history.size() > 32 || total > 8 * 1024 * 1024)) {
        total -= weight(history.front());
        history.pop_front();
    }
}
bool Controller::change(const std::function<void(Model&)>& action, bool record_undo) {
    Model candidate = model_;
    action(candidate);
    validate(candidate);
    if (candidate == model_) return false;
    // A failed commit cannot leak an unpersisted update into the live UI.
    repository_.save(candidate);
    if (record_undo) { remember(undo_, model_); redo_.clear(); }
    model_ = std::move(candidate);
    return true;
}
bool Controller::undo() {
    if (undo_.empty()) return false;
    repository_.save(undo_.back());
    remember(redo_, model_);
    model_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}
bool Controller::redo() {
    if (redo_.empty()) return false;
    repository_.save(redo_.back());
    remember(undo_, model_);
    model_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}
}
