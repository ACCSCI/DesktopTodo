#pragma once
#include "model.hpp"
#include <algorithm>
#include <vector>

namespace todo {
// A preview never mutates persisted order. Missing/end targets append the row.
inline std::vector<int> drag_preview(const std::vector<Id>& order,Id dragged,Id before,int height) {
    std::vector<int> offsets(order.size());
    if(before==dragged||std::find(order.begin(),order.end(),dragged)==order.end())return offsets;
    auto reordered=order;
    std::erase(reordered,dragged);
    reordered.insert(std::find(reordered.begin(),reordered.end(),before),dragged);
    for(std::size_t i=0;i<order.size();++i) {
        int target=int(std::find(reordered.begin(),reordered.end(),order[i])-reordered.begin());
        offsets[i]=(target-int(i))*height;
    }
    return offsets;
}
}
