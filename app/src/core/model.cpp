#include "todo/model.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace todo {
bool valid_utf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        auto first = static_cast<unsigned char>(text[i++]);
        if (first == 0) return false;
        if (first < 0x80) continue;
        int count;
        std::uint32_t value, minimum;
        if (first >= 0xC2 && first <= 0xDF) { count = 1; value = first & 31; minimum = 0x80; }
        else if (first >= 0xE0 && first <= 0xEF) { count = 2; value = first & 15; minimum = 0x800; }
        else if (first >= 0xF0 && first <= 0xF4) { count = 3; value = first & 7; minimum = 0x10000; }
        else return false;
        if (i + count > text.size()) return false;
        while (count--) {
            auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xC0) != 0x80) return false;
            value = (value << 6) | (next & 63);
        }
        if (value < minimum || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) return false;
    }
    return true;
}

std::string clean_text(const std::string& text, std::size_t limit) {
    if (text.size() > limit || !valid_utf8(text)) throw std::invalid_argument("Invalid or oversized UTF-8 text");
    auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) throw std::invalid_argument("Text must not be empty");
    auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

bool matches(Filter filter, Status status) {
    switch (filter) {
    case Filter::all: return true;
    case Filter::unfinished: return status != Status::done;
    case Filter::pending: return status == Status::pending;
    case Filter::doing: return status == Status::doing;
    case Filter::done: return status == Status::done;
    }
    throw std::invalid_argument("Unknown filter");
}

std::vector<Id> visible_tasks(const Note& note) {
    std::vector<Id> result;
    for (const auto& task : note.tasks) if (matches(note.filter, task.status)) result.push_back(task.id);
    return result;
}

Note& note_by_id(Model& model, Id id) {
    auto it = std::find_if(model.notes.begin(), model.notes.end(), [id](const Note& n) { return n.id == id; });
    if (it == model.notes.end()) throw std::out_of_range("Note does not exist");
    return *it;
}
const Note& note_by_id(const Model& model, Id id) {
    auto it = std::find_if(model.notes.begin(), model.notes.end(), [id](const Note& n) { return n.id == id; });
    if (it == model.notes.end()) throw std::out_of_range("Note does not exist");
    return *it;
}
Task& task_by_id(Note& note, Id id) {
    auto it = std::find_if(note.tasks.begin(), note.tasks.end(), [id](const Task& t) { return t.id == id; });
    if (it == note.tasks.end()) throw std::out_of_range("Task does not exist");
    return *it;
}
const Task& task_by_id(const Note& note, Id id) {
    auto it = std::find_if(note.tasks.begin(), note.tasks.end(), [id](const Task& t) { return t.id == id; });
    if (it == note.tasks.end()) throw std::out_of_range("Task does not exist");
    return *it;
}

void validate(const Model& model) {
    if (model.next_id <= 0) throw std::invalid_argument("Invalid ID sequence");
    std::unordered_set<Id> ids;
    auto accept = [&](Id id) {
        if (id <= 0 || id >= model.next_id || !ids.insert(id).second) throw std::invalid_argument("Invalid or duplicate ID");
    };
    for (const auto& note : model.notes) {
        accept(note.id);
        clean_text(note.title, 512);
        if (note.color > 0xFFFFFF || int(note.mode) < 0 || int(note.mode) > 2 ||
            int(note.corner) < 0 || int(note.corner) > 3 || int(note.filter) < 0 || int(note.filter) > 4 ||
            note.lock_offset < 0 || note.lock_offset > 64) throw std::invalid_argument("Invalid note settings");
        const auto& b = note.bounds;
        if (b.width < 240 || b.height < 180 || b.width > 8000 || b.height > 8000 ||
            b.dpi < 48 || b.dpi > 768 || b.x < -100000 || b.x > 100000 || b.y < -100000 || b.y > 100000)
            throw std::invalid_argument("Invalid note bounds");
        for (const auto& task : note.tasks) {
            accept(task.id);
            clean_text(task.text, 8192);
            clean_text(task.icon, 64);
            if (task.color > 0xFFFFFF || int(task.status) < 0 || int(task.status) > 2 ||
                task.created < 0 || task.completed < 0 ||
                (task.status != Status::done && task.completed != 0)) throw std::invalid_argument("Invalid task settings");
        }
    }
}
}
