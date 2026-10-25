#include "test.hpp"
#include "todo/window_policy.hpp"
using namespace todo;
TEST_CASE("locked desktop notes accept read-only wheel events only on their exposed surface") {
    Note note;note.mode=Mode::desktop;note.locked=true;
    CHECK(accept_locked_wheel(note,true,true));
    CHECK(!accept_locked_wheel(note,false,true));
    CHECK(!accept_locked_wheel(note,true,false));
    note.locked=false;CHECK(!accept_locked_wheel(note,true,true));
    note.locked=true;note.mode=Mode::topmost;CHECK(!accept_locked_wheel(note,true,true));
}
