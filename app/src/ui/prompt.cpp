#include "todo/ui.hpp"
#include <commctrl.h>

namespace todo::ui {
namespace {
struct Prompt {
    HWND window = nullptr, edit = nullptr, owner = nullptr;
    HFONT face = nullptr;
    bool done = false, accepted = false;
    int limit = 512;
    std::wstring initial;
};
LRESULT CALLBACK prompt_proc(HWND window,UINT message,WPARAM w,LPARAM l) {
    auto* prompt = reinterpret_cast<Prompt*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        prompt = static_cast<Prompt*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(prompt));
        prompt->window = window;
    }
    if (!prompt) return DefWindowProcW(window,message,w,l);
    if (message == WM_CREATE) {
        int dpi = int(GetDpiForWindow(window));
        prompt->face = font(dpi,11);
        auto p = [&](int dip) { return pixels(dip,dpi); };
        prompt->edit = CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",prompt->initial.c_str(),
            WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,p(18),p(20),p(370),p(30),window,
            reinterpret_cast<HMENU>(100),nullptr,nullptr);
        SendMessageW(prompt->edit,WM_SETFONT,reinterpret_cast<WPARAM>(prompt->face),TRUE);
        SendMessageW(prompt->edit,EM_SETLIMITTEXT,prompt->limit,0);
        for (int i=0;i<2;++i) {
            HWND button = CreateWindowExW(0,L"BUTTON",i ? L"取消" : L"确定",
                WS_CHILD|WS_VISIBLE|WS_TABSTOP|(i ? BS_PUSHBUTTON : BS_DEFPUSHBUTTON),
                p(214+i*90),p(68),p(82),p(30),window,reinterpret_cast<HMENU>(INT_PTR(i ? IDCANCEL : IDOK)),nullptr,nullptr);
            SendMessageW(button,WM_SETFONT,reinterpret_cast<WPARAM>(prompt->face),TRUE);
        }
        SendMessageW(prompt->edit,EM_SETSEL,0,-1);
        SetFocus(prompt->edit);
        return 0;
    }
    if (message == WM_COMMAND && (LOWORD(w)==IDOK || LOWORD(w)==IDCANCEL)) {
        prompt->accepted = LOWORD(w)==IDOK; prompt->done=true; return 0;
    }
    if (message == WM_CLOSE) { prompt->done=true; return 0; }
    if (message == WM_ERASEBKGND) {
        RECT rect{}; GetClientRect(window,&rect); fill(reinterpret_cast<HDC>(w),rect,GetSysColor(COLOR_WINDOW)); return 1;
    }
    return DefWindowProcW(window,message,w,l);
}
}
std::optional<std::wstring> prompt(HWND owner,const std::wstring& title,const std::wstring& initial,int limit) {
    WNDCLASSEXW cls{}; cls.cbSize=sizeof(cls); cls.lpfnWndProc=prompt_proc;
    cls.hInstance=GetModuleHandleW(nullptr); cls.lpszClassName=L"DesktopTodo.Prompt";
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    RegisterClassExW(&cls);
    Prompt state; state.owner=owner;state.initial=initial;state.limit=limit;
    int dpi = owner ? int(GetDpiForWindow(owner)) : int(GetDpiForSystem());
    RECT area{}; SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
    int width=pixels(420,dpi),height=pixels(150,dpi);
    state.window=CreateWindowExW(WS_EX_DLGMODALFRAME,L"DesktopTodo.Prompt",title.c_str(),
        WS_POPUP|WS_CAPTION|WS_SYSMENU,(area.right-width)/2,(area.bottom-height)/2,width,height,
        owner,nullptr,cls.hInstance,&state);
    if (!state.window) throw std::runtime_error(win::last_error("Text dialog"));
    EnableWindow(owner,FALSE); show(state.window); SetForegroundWindow(state.window);SetFocus(state.edit);
    MSG message{};
    while (!state.done) {
        int result=GetMessageW(&message,nullptr,0,0);
        if (result<=0) { if (!result) PostQuitMessage(int(message.wParam)); break; }
        if (message.message==WM_KEYDOWN && GetAncestor(message.hwnd,GA_ROOT)==state.window && (message.wParam==VK_RETURN || message.wParam==VK_ESCAPE)) {
            state.accepted=message.wParam==VK_RETURN;state.done=true;continue;
        }
        if (!IsDialogMessageW(state.window,&message)) { TranslateMessage(&message);DispatchMessageW(&message); }
    }
    std::optional<std::wstring> result;
    if (state.accepted) {
        int length=GetWindowTextLengthW(state.edit);
        std::wstring text(std::size_t(length)+1,L'\0');
        GetWindowTextW(state.edit,text.data(),length+1);text.resize(length);result=std::move(text);
    }
    DestroyWindow(state.window); if (state.face) DeleteObject(state.face);
    EnableWindow(owner,TRUE);
    return result;
}
}
