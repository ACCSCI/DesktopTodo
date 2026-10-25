#include "todo/note_view.hpp"
#include "todo/ui.hpp"
#include <array>

namespace todo {
void NoteView::filter_menu(POINT point) {
    const auto* current=note();if(!current)return;
    HMENU menu=CreatePopupMenu();
    for(int i=0;i<5;++i)AppendMenuW(menu,MF_STRING|(current->filter==Filter(i)?MF_CHECKED:0),100+i,ui::filter_name(Filter(i)).c_str());
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window_,nullptr);DestroyMenu(menu);
    if(result>=100&&result<105)app_.change([&](Model& m){set_filter(m,id_,Filter(result-100));});
}
void NoteView::mode_menu(POINT point) {
    const auto* current=note();if(!current)return;
    HMENU menu=CreatePopupMenu();
    for(int i=0;i<3;++i)AppendMenuW(menu,MF_STRING|(current->mode==Mode(i)?MF_CHECKED:0),100+i,ui::mode_name(Mode(i)).c_str());
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window_,nullptr);DestroyMenu(menu);
    if(result>=100&&result<103)app_.change([&](Model& m){set_mode(m,id_,Mode(result-100));});
}
void NoteView::task_menu(Id task,POINT point) {
    const auto* current=note();if(!current)return;
    Task copy=task_by_id(*current,task);
    HMENU menu=CreatePopupMenu(),statuses=CreatePopupMenu(),icons=CreatePopupMenu();
    const wchar_t* status_names[]={L"待开始",L"进行中",L"已完成"};
    for(int i=0;i<3;++i)AppendMenuW(statuses,MF_STRING|(copy.status==Status(i)?MF_CHECKED:0),100+i,status_names[i]);
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(statuses),L"状态");
    const std::array<std::string,12> icon_values={"•","★","☎","⌂","⚑","✓","✦","☕","⚙","📚","🛒","🎯"};
    const wchar_t* icon_names[]={L"•  默认",L"★  重点",L"☎  联系",L"⌂  生活",L"⚑  工作",L"✓  检查",L"✦  灵感",L"☕  休息",L"⚙  工具",L"📚  学习",L"🛒  购物",L"🎯  目标"};
    for(std::size_t i=0;i<icon_values.size();++i)AppendMenuW(icons,MF_STRING|(copy.icon==icon_values[i]?MF_CHECKED:0),200+i,icon_names[i]);
    AppendMenuW(icons,MF_SEPARATOR,0,nullptr);AppendMenuW(icons,MF_STRING,250,L"自定义图标…");
    AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(icons),L"图标");
    AppendMenuW(menu,MF_STRING,300,L"颜色…");AppendMenuW(menu,MF_STRING,301,L"编辑文字    F2");
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,302,L"删除待办    Delete");
    int result=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,window_,nullptr);DestroyMenu(menu);
    if(result>=100&&result<103)app_.change([&](Model& m){set_status(m,id_,task,Status(result-100),ui::now());});
    else if(result>=200&&result<212)app_.change([&](Model& m){set_task_icon(m,id_,task,icon_values[std::size_t(result-200)]);});
    else if(result==250) {
        auto icon=ui::prompt(app_.manager(),L"自定义图标",win::wide(copy.icon),16);
        if(icon)app_.change([&](Model& m){set_task_icon(m,id_,task,win::utf8(*icon));});
    } else if(result==300) {
        auto color=ui::choose_color(app_.manager(),copy.color);
        if(color)app_.change([&](Model& m){set_task_color(m,id_,task,*color);});
    } else if(result==301)begin_edit(task,false,false);
    else if(result==302)app_.change([&](Model& m){remove_task(m,id_,task);});
}
}
