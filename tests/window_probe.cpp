#include <windows.h>
#include <iostream>
int main() {
    auto setDpi = reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext"));
    if(setDpi) setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HWND window=FindWindowW(L"StreamShelfNativeOpenGL", nullptr);
    if(!window){std::cerr<<"Application window missing\n";return 1;}
    RECT rect{}; GetWindowRect(window,&rect);
    RECT client{};GetClientRect(window,&client);POINT origin{};ClientToScreen(window,&origin);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
    CURSORINFO cursor{sizeof(cursor)};GetCursorInfo(&cursor);
    std::cout<<"Window "<<rect.left<<","<<rect.top<<" "<<rect.right-rect.left<<"x"<<rect.bottom-rect.top<<"\n";
    std::cout<<"Work area "<<monitor.rcWork.left<<","<<monitor.rcWork.top<<" "<<monitor.rcWork.right-monitor.rcWork.left<<"x"<<monitor.rcWork.bottom-monitor.rcWork.top<<"\n";
    std::cout<<"Maximized "<<!!IsZoomed(window)<<"; cursor showing "<<!!(cursor.flags&CURSOR_SHOWING)<<"\n";
    std::cout<<"Client inset "<<origin.x-rect.left<<","<<origin.y-rect.top<<"; client size "<<client.right<<"x"<<client.bottom<<"\n";
    if(origin.x<monitor.rcWork.left||origin.y<monitor.rcWork.top||origin.x+client.right>monitor.rcWork.right||origin.y+client.bottom>monitor.rcWork.bottom){std::cerr<<"FAIL painted area covers desktop outside work area\n";return 1;}
    std::cout<<"PASS painted window stays inside desktop work area\n";
    if(IsZoomed(window)){
        if(origin.x!=monitor.rcWork.left||origin.y!=monitor.rcWork.top||client.right!=monitor.rcWork.right-monitor.rcWork.left||client.bottom!=monitor.rcWork.bottom-monitor.rcWork.top){std::cerr<<"FAIL maximized client does not fill work area\n";return 1;}
    }else if(origin.y!=rect.top||origin.x!=rect.left||client.right!=rect.right-rect.left||client.bottom!=rect.bottom-rect.top){std::cerr<<"FAIL unexpected native frame inset\n";return 1;}
    std::cout<<"PASS no unpainted native title/border strip in visible client area\n";
    return 0;
}
