#include "test.hpp"
#include "todo/maintenance.hpp"
#include "todo/sqlite_store.hpp"
#include "todo/actions.hpp"
using namespace todo;
TEST_CASE("clearing requires three successive confirmations and cancellation stops immediately") {
    for(int cancel=1;cancel<=3;++cancel) {
        int calls=0;
        CHECK(!confirm_data_clear([&](int stage){CHECK(stage==++calls);return stage!=cancel;}));
        CHECK(calls==cancel);
    }
    int calls=0;CHECK(confirm_data_clear([&](int stage){CHECK(stage==++calls);return true;}));CHECK(calls==3);
}
TEST_CASE("database compaction preserves notes while clear removes their content") {
    SqliteStore store(":memory:");Model model;auto id=add_note(model,"计划");add_task(model,id,"不可丢失");
    store.save(model);store.compact();CHECK(store.load()==model);
    store.clear_data();CHECK(store.load()==Model{});
    add_note(model,"再次使用");store.save(model);CHECK(store.load()==model);
}
