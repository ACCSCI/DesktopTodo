#pragma once
#include "todo/drag_preview.hpp"
#include "duilib/duilib.h"
#include <map>
#include <memory>
#include <sstream>

namespace todo {
class RowMotion {
    struct Row {
        ::ui::Control* control=nullptr;
        std::unique_ptr<::ui::AnimationPlayer> animation;
        int target=0;
    };
    std::map<Id,Row> rows_;
    std::vector<Id> order_;
    std::map<Id,int> origins_;
    int height_=38;
    void move(Row& row,int target,bool immediate=false) {
        if(!immediate&&row.target==target)return;
        row.target=target;
        row.animation->Stop();
        if(immediate) {row.control->SetRenderOffsetY(target);return;}
        row.animation->SetStartValue(row.control->GetRenderOffset().y);
        row.animation->SetEndValue(target);
        row.animation->Start();
    }
public:
    std::string inspect() const {
        std::ostringstream out;out<<"[";bool first=true;
        for(Id id:order_) {
            const auto& row=rows_.at(id);auto rect=row.control->GetPos();
            if(!first)out<<",";first=false;
            out<<"{\"id\":"<<id<<",\"offset\":"<<row.control->GetRenderOffset().y
               <<",\"target\":"<<row.target<<",\"playing\":"<<(row.animation->IsPlaying()?"true":"false")
               <<",\"rect\":["<<rect.left<<","<<rect.top<<","<<rect.right<<","<<rect.bottom<<"]";
            auto* box=dynamic_cast<::ui::Box*>(row.control);
            auto* content=dynamic_cast<::ui::Box*>(box->GetItemAt(0));
            auto grip=content->GetItemAt(0)->GetPos();
            out<<",\"grip\":["<<grip.left<<","<<grip.top<<","<<grip.right<<","<<grip.bottom<<"]}";
        }
        return out.str()+"]";
    }
    void rebuild(int height) {rows_.clear();order_.clear();height_=height;}
    void add(Id id,::ui::Control* control) {
        Row row;row.control=control;row.animation=std::make_unique<::ui::AnimationPlayer>();
        row.animation->SetTotalMillSeconds(140);
        row.animation->SetFrameIntervalMillSeconds(16);
        row.animation->SetEasingFunctionType(::ui::EasingFunctionType::EaseOutCubic);
        row.animation->SetPlayCallback([control](int value){control->SetRenderOffsetY(value);});
        int index=int(order_.size());order_.push_back(id);
        if(auto old=origins_.find(id);old!=origins_.end()) {
            control->SetRenderOffsetY(old->second-index*height_);
            row.target=control->GetRenderOffset().y;
        }
        auto [it,inserted]=rows_.emplace(id,std::move(row));
        move(it->second,0);
    }
    void capture() {
        origins_.clear();
        for(std::size_t i=0;i<order_.size();++i)
            origins_[order_[i]]=int(i)*height_+rows_.at(order_[i]).control->GetRenderOffset().y;
    }
    void preview(Id dragged,Id before,int displacement) {
        auto offsets=drag_preview(order_,dragged,before,height_);
        for(std::size_t i=0;i<order_.size();++i) {
            auto& row=rows_.at(order_[i]);bool held=order_[i]==dragged;
            row.control->SetBkColor(held?L"#FFF0EAFB":L"");
            move(row,held?displacement:offsets[i],held);
        }
    }
    void settle() {
        origins_.clear();
        for(auto& [id,row]:rows_) {row.control->SetBkColor(L"");move(row,0);}
    }
};
}
