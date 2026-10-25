#include "todo/maintenance.hpp"
namespace todo {
bool confirm_data_clear(const std::function<bool(int)>& ask) {
    for(int stage=1;stage<=3;++stage)if(!ask(stage))return false;
    return true;
}
}
