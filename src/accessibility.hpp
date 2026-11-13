#pragma once
#include <windows.h>
#include <oleacc.h>
#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ss {
// MSAA is exposed through the Windows MSAA-to-UI Automation bridge. Calls
// originating from assistive technology only inspect immutable snapshots; all
// actions are marshalled back to the window thread by a bounded message.
inline constexpr UINT AccessibilityCommandMessage = WM_APP + 71;
enum class AccessibilityAction { Focus, Invoke, SetValue };
struct AccessibleNode {
    LONG id=0, role=ROLE_SYSTEM_PUSHBUTTON, state=0;
    RECT bounds{};
    std::wstring name, value;
    bool enabled=true, editable=false, actionable=true;
};
struct AccessibilityCommand {
    LONG id=0;
    AccessibilityAction action=AccessibilityAction::Focus;
    std::wstring value;
};
struct AccessibilityState {
    std::mutex mutex;
    HWND window=nullptr;
    std::vector<AccessibleNode> nodes;
    LONG focused=0;
    uint64_t nextCommand=0;
    std::map<uint64_t,AccessibilityCommand> pending;

    HRESULT dispatch(AccessibilityCommand command) {
        HWND target=nullptr; uint64_t request=0;
        {
            std::lock_guard lock(mutex);
            if(!window || pending.size()>=128)return E_FAIL;
            target=window; request=++nextCommand;
            pending.emplace(request,std::move(command));
        }
        DWORD_PTR result=0;
        const auto sent=SendMessageTimeoutW(target,AccessibilityCommandMessage,WPARAM(request),0,
                                            SMTO_ABORTIFHUNG|SMTO_BLOCK,1500,&result);
        { std::lock_guard lock(mutex); pending.erase(request); }
        return sent && result ? S_OK : E_FAIL;
    }
};

class AccessibleEnumerator final : public IEnumVARIANT {
    std::atomic<ULONG> references{1};
    std::vector<LONG> ids;
    size_t index=0;
  public:
    explicit AccessibleEnumerator(std::vector<LONG> values, size_t position=0):ids(std::move(values)),index(position){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if(!out)return E_POINTER;
        *out=nullptr;
        if(iid==IID_IUnknown || iid==IID_IEnumVARIANT){*out=static_cast<IEnumVARIANT *>(this);AddRef();return S_OK;}
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--references;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE Next(ULONG count, VARIANT *values, ULONG *fetched) override {
        if(!values || (count!=1 && !fetched))return E_POINTER;
        ULONG n=0;
        while(n<count && index<ids.size()){VariantInit(&values[n]);values[n].vt=VT_I4;values[n].lVal=ids[index++];++n;}
        if(fetched)*fetched=n;
        return n==count?S_OK:S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG count) override {
        const auto left=ids.size()-index;index+=std::min<size_t>(count,left);return count<=left?S_OK:S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {index=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE Clone(IEnumVARIANT **out) override {
        if(!out)return E_POINTER;*out=new AccessibleEnumerator(ids,index);return S_OK;
    }
};

class AccessibleProvider final : public IAccessible {
    std::atomic<ULONG> references{1};
    std::shared_ptr<AccessibilityState> state;
    bool find(VARIANT child, AccessibleNode &out) const {
        if(child.vt!=VT_I4 || child.lVal<=0)return false;
        std::lock_guard lock(state->mutex);
        auto it=std::find_if(state->nodes.begin(),state->nodes.end(),[&](const auto &node){return node.id==child.lVal;});
        if(it==state->nodes.end())return false;
        out=*it;return true;
    }
    static HRESULT string(const std::wstring &value, BSTR *out) {
        if(!out)return E_POINTER;
        *out=SysAllocStringLen(value.data(),UINT(value.size()));
        return *out?S_OK:E_OUTOFMEMORY;
    }
  public:
    explicit AccessibleProvider(std::shared_ptr<AccessibilityState> value):state(std::move(value)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
        if(!out)return E_POINTER;
        *out=nullptr;
        if(iid==IID_IUnknown || iid==IID_IDispatch || iid==IID_IAccessible){*out=static_cast<IAccessible *>(this);AddRef();return S_OK;}
        if(iid==IID_IEnumVARIANT){
            std::vector<LONG> ids;
            {std::lock_guard lock(state->mutex);for(const auto &node:state->nodes)ids.push_back(node.id);}
            *out=static_cast<IEnumVARIANT *>(new AccessibleEnumerator(std::move(ids)));return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return ++references;}
    ULONG STDMETHODCALLTYPE Release() override {auto n=--references;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *count) override {if(!count)return E_POINTER;*count=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo **) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR *,UINT,LCID,DISPID *) override {return DISP_E_UNKNOWNNAME;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS *,VARIANT *,EXCEPINFO *,UINT *) override {return DISP_E_MEMBERNOTFOUND;}
    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch **out) override {if(!out)return E_POINTER;*out=nullptr;return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accChildCount(LONG *out) override {
        if(!out)return E_POINTER;std::lock_guard lock(state->mutex);*out=LONG(state->nodes.size());return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT child, IDispatch **out) override {
        if(!out)return E_POINTER;*out=nullptr;AccessibleNode node;return find(child,node)?S_FALSE:E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT child, BSTR *out) override {
        if(child.vt==VT_I4 && child.lVal==CHILDID_SELF)return string(L"StreamShelf",out);
        AccessibleNode node;return find(child,node)?string(node.name,out):E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child, BSTR *out) override {
        if(!out)return E_POINTER;*out=nullptr;AccessibleNode node;
        if(!find(child,node))return E_INVALIDARG;
        return node.value.empty() && !node.editable?S_FALSE:string(node.value,out);
    }
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT, BSTR *out) override {if(!out)return E_POINTER;*out=nullptr;return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child, VARIANT *out) override {
        if(!out)return E_POINTER;VariantInit(out);
        if(child.vt==VT_I4 && child.lVal==CHILDID_SELF){out->vt=VT_I4;out->lVal=ROLE_SYSTEM_CLIENT;return S_OK;}
        AccessibleNode node;if(!find(child,node))return E_INVALIDARG;
        out->vt=VT_I4;out->lVal=node.role;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT child, VARIANT *out) override {
        if(!out)return E_POINTER;VariantInit(out);
        if(child.vt==VT_I4 && child.lVal==CHILDID_SELF){out->vt=VT_I4;out->lVal=STATE_SYSTEM_FOCUSABLE;return S_OK;}
        AccessibleNode node;if(!find(child,node))return E_INVALIDARG;
        out->vt=VT_I4;out->lVal=node.state | (node.enabled?0:STATE_SYSTEM_UNAVAILABLE);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT, BSTR *out) override {if(!out)return E_POINTER;*out=nullptr;return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR *file,VARIANT,LONG *topic) override {if(!file||!topic)return E_POINTER;*file=nullptr;*topic=-1;return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT,BSTR *out) override {if(!out)return E_POINTER;*out=nullptr;return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT *out) override {
        if(!out)return E_POINTER;VariantInit(out);std::lock_guard lock(state->mutex);
        if(state->window && state->focused){out->vt=VT_I4;out->lVal=state->focused;return S_OK;}return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT *out) override {if(!out)return E_POINTER;VariantInit(out);return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child,BSTR *out) override {
        if(!out)return E_POINTER;*out=nullptr;AccessibleNode node;
        if(!find(child,node))return E_INVALIDARG;
        return node.enabled && node.actionable?string(node.editable?L"Focus":L"Activate",out):S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accSelect(LONG flags,VARIANT child) override {
        AccessibleNode node;if(!find(child,node))return E_INVALIDARG;
        if(!node.enabled || !node.actionable || !(flags&SELFLAG_TAKEFOCUS))return S_FALSE;
        return state->dispatch({node.id,AccessibilityAction::Focus,{}});
    }
    HRESULT STDMETHODCALLTYPE accLocation(LONG *x,LONG *y,LONG *w,LONG *h,VARIANT child) override {
        if(!x||!y||!w||!h)return E_POINTER;
        RECT r{};
        if(child.vt==VT_I4 && child.lVal==CHILDID_SELF){std::lock_guard lock(state->mutex);if(!state->window || !GetWindowRect(state->window,&r))return E_FAIL;}
        else {AccessibleNode node;if(!find(child,node))return E_INVALIDARG;r=node.bounds;}
        *x=r.left;*y=r.top;*w=r.right-r.left;*h=r.bottom-r.top;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accNavigate(LONG direction,VARIANT start,VARIANT *out) override {
        if(!out)return E_POINTER;VariantInit(out);
        if(start.vt!=VT_I4)return E_INVALIDARG;
        std::lock_guard lock(state->mutex);
        if(state->nodes.empty())return S_FALSE;
        size_t index=0;
        if(start.lVal==CHILDID_SELF){
            if(direction==NAVDIR_FIRSTCHILD)index=0;
            else if(direction==NAVDIR_LASTCHILD)index=state->nodes.size()-1;
            else return S_FALSE;
        } else {
            auto it=std::find_if(state->nodes.begin(),state->nodes.end(),[&](const auto &node){return node.id==start.lVal;});
            if(it==state->nodes.end())return E_INVALIDARG;
            index=size_t(it-state->nodes.begin());
            if(direction==NAVDIR_NEXT && index+1<state->nodes.size())++index;
            else if(direction==NAVDIR_PREVIOUS && index>0)--index;
            else return S_FALSE;
        }
        out->vt=VT_I4;out->lVal=state->nodes[index].id;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accHitTest(LONG x,LONG y,VARIANT *out) override {
        if(!out)return E_POINTER;VariantInit(out);std::lock_guard lock(state->mutex);
        POINT point{x,y};
        for(auto it=state->nodes.rbegin();it!=state->nodes.rend();++it)if(PtInRect(&it->bounds,point)){out->vt=VT_I4;out->lVal=it->id;return S_OK;}
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override {
        AccessibleNode node;if(!find(child,node))return E_INVALIDARG;
        if(!node.enabled || !node.actionable)return E_ACCESSDENIED;
        return state->dispatch({node.id,node.editable?AccessibilityAction::Focus:AccessibilityAction::Invoke,{}});
    }
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT,BSTR) override {return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT child,BSTR value) override {
        AccessibleNode node;if(!find(child,node))return E_INVALIDARG;
        if(!node.enabled || !node.editable)return E_ACCESSDENIED;
        if(!value || SysStringLen(value)>32768)return E_INVALIDARG;
        return state->dispatch({node.id,AccessibilityAction::SetValue,std::wstring(value,SysStringLen(value))});
    }
};
inline LRESULT accessibilityObjectResult(WPARAM wparam,const std::shared_ptr<AccessibilityState> &state) {
    static HMODULE library=LoadLibraryExW(L"oleacc.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    static auto marshal=library?reinterpret_cast<LPFNLRESULTFROMOBJECT>(GetProcAddress(library,"LresultFromObject")):nullptr;
    if(!marshal)return 0;
    auto *provider=new AccessibleProvider(state);
    auto result=marshal(IID_IAccessible,wparam,provider);
    provider->Release();
    return result;
}
} // namespace ss
