#include <windows.h>
#include <set>
#include <utility>
#include <iostream>
int main(){
 auto setDpi=reinterpret_cast<BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT)>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetProcessDpiAwarenessContext"));
 if(setDpi)setDpi(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
 auto window=FindWindowW(L"StreamShelfNativeOpenGL",nullptr);if(!window)return 2;
 std::set<std::pair<LONG,LONG>> sizes;
 for(int i=0;i<2500;i++){RECT rect{};if(!GetWindowRect(window,&rect))return 3;sizes.emplace(rect.right-rect.left,rect.bottom-rect.top);Sleep(8);}
 std::cout<<"Observed "<<sizes.size()<<" distinct window sizes during transition\n";
 if(sizes.size()<5){std::cout<<"FAIL no smooth multi-frame size transition observed\n";return 1;}
 std::cout<<"PASS multi-frame window resize animation\n";
}
