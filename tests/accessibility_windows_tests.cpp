#include "accessibility.hpp"
#include <uiautomation.h>
#include <future>
#include <iostream>
#include <thread>

using namespace ss;
namespace {
std::shared_ptr<AccessibilityState> state=std::make_shared<AccessibilityState>();
std::atomic_int invoked{0};
LRESULT CALLBACK fixtureProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_GETOBJECT && DWORD(l)==DWORD(OBJID_CLIENT))return accessibilityObjectResult(w,state);
    if(message==AccessibilityCommandMessage) {
        AccessibilityCommand command;
        std::lock_guard lock(state->mutex);
        auto found=state->pending.find(uint64_t(w));
        if(found==state->pending.end())return 0;
        command=std::move(found->second);state->pending.erase(found);
        auto node=std::find_if(state->nodes.begin(),state->nodes.end(),[&](const auto &entry){return entry.id==command.id;});
        if(node==state->nodes.end() || !node->enabled)return 0;
        if(command.action==AccessibilityAction::Invoke)++invoked;
        if(command.action==AccessibilityAction::Focus)state->focused=node->id;
        if(command.action==AccessibilityAction::SetValue)node->value=command.value;
        return 1;
    }
    if(message==WM_CLOSE){DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){std::lock_guard lock(state->mutex);state->window=nullptr;PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,w,l);
}
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
VARIANT child(LONG id){VARIANT value;VariantInit(&value);value.vt=VT_I4;value.lVal=id;return value;}
}
int main() {
    std::promise<HWND> started;
    auto ready=started.get_future();
    std::thread server([&] {
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        WNDCLASSW cls{};cls.lpfnWndProc=fixtureProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"StreamShelfAccessibilityFixture";
        if(!RegisterClassW(&cls)){started.set_value(nullptr);CoUninitialize();return;}
        HWND window=CreateWindowW(cls.lpszClassName,L"Accessibility fixture",WS_OVERLAPPEDWINDOW,0,0,500,300,nullptr,nullptr,cls.hInstance,nullptr);
        if(!window){started.set_value(nullptr);CoUninitialize();return;}
        {
            std::lock_guard lock(state->mutex);
            state->window=window;
            state->nodes={
                {7,ROLE_SYSTEM_PUSHBUTTON,STATE_SYSTEM_FOCUSABLE,{10,10,80,40},L"Analyze",L"",true,false,true},
                {21,ROLE_SYSTEM_TEXT,STATE_SYSTEM_FOCUSABLE,{10,50,200,80},L"Page URL",L"https://example.test",true,true,true},
                {42,ROLE_SYSTEM_PUSHBUTTON,STATE_SYSTEM_UNAVAILABLE,{10,90,80,120},L"Unavailable",L"",false,false,false}
            };
        }
        started.set_value(window);
        MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
        CoUninitialize();
    });
    HWND window=ready.get();
    if(!window){server.join();return 2;}
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    int exitCode=0;
    IAccessible *accessible=nullptr;
    HMODULE library=LoadLibraryExW(L"oleacc.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    try {
        require(library!=nullptr,"oleacc unavailable");
        auto get=reinterpret_cast<LPFNACCESSIBLEOBJECTFROMWINDOW>(GetProcAddress(library,"AccessibleObjectFromWindow"));
        require(get && SUCCEEDED(get(window,OBJID_CLIENT,IID_IAccessible,reinterpret_cast<void **>(&accessible))),"MSAA object unavailable");
        LONG count=0;require(SUCCEEDED(accessible->get_accChildCount(&count)) && count==3,"incorrect child count");
        IEnumVARIANT *enumerator=nullptr;
        require(SUCCEEDED(accessible->QueryInterface(IID_IEnumVARIANT,reinterpret_cast<void **>(&enumerator))),"child enumeration unavailable");
        VARIANT values[3];ULONG fetched=0;
        require(enumerator->Next(3,values,&fetched)==S_OK && fetched==3 && values[0].lVal==7 && values[1].lVal==21 && values[2].lVal==42,"stable child ids not enumerated");
        enumerator->Release();
        BSTR name=nullptr;require(SUCCEEDED(accessible->get_accName(child(7),&name)) && std::wstring(name)==L"Analyze","incorrect accessible name");SysFreeString(name);
        VARIANT role;require(SUCCEEDED(accessible->get_accRole(child(21),&role)) && role.lVal==ROLE_SYSTEM_TEXT,"incorrect edit role");
        VARIANT status;require(SUCCEEDED(accessible->get_accState(child(42),&status)) && (status.lVal&STATE_SYSTEM_UNAVAILABLE),"disabled state unavailable");
        require(SUCCEEDED(accessible->accSelect(SELFLAG_TAKEFOCUS,child(21))),"focus action failed");
        VARIANT focused;require(SUCCEEDED(accessible->get_accFocus(&focused)) && focused.lVal==21,"focus not exposed");
        require(SUCCEEDED(accessible->accDoDefaultAction(child(7))) && invoked==1,"button action failed");
        require(FAILED(accessible->accDoDefaultAction(child(42))) && invoked==1,"disabled button was activated");
        BSTR replacement=SysAllocString(L"https://new.example/");
        require(SUCCEEDED(accessible->put_accValue(child(21),replacement)),"edit action failed");SysFreeString(replacement);
        BSTR value=nullptr;require(SUCCEEDED(accessible->get_accValue(child(21),&value)) && std::wstring(value)==L"https://new.example/","edit value not exposed");SysFreeString(value);

        IUIAutomation *automation=nullptr;
        require(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,__uuidof(IUIAutomation),reinterpret_cast<void **>(&automation))),"UI Automation client unavailable");
        IUIAutomationElement *root=nullptr;
        require(SUCCEEDED(automation->ElementFromHandle(window,&root)) && root,"UI Automation root unavailable");
        IUIAutomationCondition *condition=nullptr;automation->CreateTrueCondition(&condition);
        IUIAutomationElementArray *children=nullptr;
        require(condition && SUCCEEDED(root->FindAll(TreeScope_Children,condition,&children)) && children,"UI Automation children unavailable");
        int childCount=0;children->get_Length(&childCount);
        require(childCount>=3,"UI Automation bridge did not expose MSAA children");
        children->Release();condition->Release();root->Release();automation->Release();
        {
            std::lock_guard lock(state->mutex);
            state->nodes[0].id=8; // The original control disappeared or was replaced.
        }
        require(FAILED(accessible->accDoDefaultAction(child(7))) && invoked==1,
                "a stale accessible child must not activate its replacement");
        PostMessageW(window,WM_CLOSE,0,0);server.join();
        AccessibleProvider direct(state);
        require(FAILED(direct.accDoDefaultAction(child(7))),"provider accepted an action after window destruction");
        (void)accessible->accDoDefaultAction(child(7));
        require(invoked==1,"action executed after window destruction");
        std::cout<<"PASS MSAA names, roles, states, focus, value and actions; UI Automation bridge; window lifetime\n";
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';exitCode=1;}
    if(server.joinable()){PostMessageW(window,WM_CLOSE,0,0);server.join();}
    if(accessible)accessible->Release();
    if(library)FreeLibrary(library);
    if(SUCCEEDED(initialized))CoUninitialize();
    return exitCode;
}
