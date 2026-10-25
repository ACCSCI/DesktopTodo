#include "test.hpp"
#include "todo/drag_preview.hpp"
#include "todo/window_policy.hpp"
using namespace todo;
TEST_CASE("drag previews open only the destination gap without changing task order") {
    std::vector<Id> order{1,2,3,4};
    CHECK((drag_preview(order,1,4,38)==std::vector<int>{76,-38,-38,0}));
    CHECK((drag_preview(order,4,2,38)==std::vector<int>{0,38,38,-76}));
    CHECK((drag_preview(order,2,0,38)==std::vector<int>{0,76,-38,-38}));
    CHECK((order==std::vector<Id>{1,2,3,4}));
}
TEST_CASE("cancelled and self-targeted drag previews have no displacement at any DPI") {
    for(int dpi:{96,144,192}) {
        CHECK((drag_preview({1,2,3},2,2,pixels(38,dpi))==std::vector<int>{0,0,0}));
        CHECK((drag_preview({1,2,3},8,0,pixels(38,dpi))==std::vector<int>{0,0,0}));
    }
}
TEST_CASE("extra toolbar breathing room preserves row pitch and footer geometry") {
    Layout old{680,760,192};auto spaced=old;spaced.toolbar_gap=12;
    CHECK(spaced.list_top()-old.list_top()==24);
    CHECK(spaced.row_height()==old.row_height());
    CHECK(spaced.list_bottom()==old.list_bottom());
}
