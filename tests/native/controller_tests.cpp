#include "test.hpp"
#include "todo/actions.hpp"
#include "todo/controller.hpp"
using namespace todo;

struct MemoryRepository : Repository {
    Model stored;
    bool fail = false;
    int saves = 0;
    Model load() override { return stored; }
    void save(const Model& model) override {
        if (fail) throw std::runtime_error("Injected storage failure");
        stored = model; ++saves;
    }
    void backup(const std::string&) override {}
};

TEST_CASE("a failed storage commit leaves both live state and history unchanged") {
    MemoryRepository repository;
    Controller controller(repository);
    repository.fail = true;
    THROWS_AS(controller.change([](Model& m) { add_note(m, "Unsaved"); }), std::runtime_error);
    CHECK(controller.model().notes.empty());
    CHECK(!controller.can_undo());
    CHECK(repository.stored.notes.empty());
}

TEST_CASE("a failing or invalid action never leaks a partially modified candidate") {
    MemoryRepository repository;
    Controller controller(repository);
    THROWS_AS(controller.change([](Model& m) { add_note(m); throw std::invalid_argument("Interrupted action"); }), std::invalid_argument);
    THROWS_AS(controller.change([](Model& m) { add_note(m); m.notes[0].color = 0xFFFFFFFF; }), std::invalid_argument);
    CHECK(controller.model().notes.empty());
    CHECK(repository.saves == 0);
}

TEST_CASE("undo and redo persist whole edits and a new edit discards the redo branch") {
    MemoryRepository repository;
    Controller controller(repository);
    Id note = 0;
    controller.change([&](Model& m) { note = add_note(m, "Original"); });
    controller.change([&](Model& m) { rename_note(m, note, "Changed"); });
    CHECK(controller.undo());
    CHECK(note_by_id(controller.model(), note).title == "Original");
    CHECK(repository.stored == controller.model());
    CHECK(controller.redo());
    CHECK(note_by_id(controller.model(), note).title == "Changed");
    controller.undo();
    controller.change([&](Model& m) { rename_note(m, note, "New branch"); });
    CHECK(!controller.can_redo());
}

TEST_CASE("no-op actions do not write and failed undo preserves the current document") {
    MemoryRepository repository;
    Controller controller(repository);
    controller.change([](Model& m) { add_note(m); });
    CHECK(!controller.change([](Model&) {}));
    CHECK(repository.saves == 1);
    auto old = controller.model();
    repository.fail = true;
    THROWS_AS(controller.undo(), std::runtime_error);
    CHECK(controller.model() == old);
    CHECK(controller.can_undo());
}

TEST_CASE("undo history remains bounded during repeated edits") {
    MemoryRepository repository;
    Controller controller(repository);
    Id note = 0;
    controller.change([&](Model& m) { note = add_note(m); });
    for (int i = 0; i < 100; ++i) controller.change([&](Model& m) { rename_note(m, note, std::to_string(i)); });
    int undos = 0;
    while (controller.undo()) ++undos;
    CHECK(undos == 32);
}
