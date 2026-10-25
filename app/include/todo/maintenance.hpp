#pragma once
#include <functional>
namespace todo {
bool confirm_data_clear(const std::function<bool(int)>& ask);
}
