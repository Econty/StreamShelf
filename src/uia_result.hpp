#pragma once
#include <windows.h>
namespace ss::installer_detail {
// Bind the out variable by reference: an inline COM call may evaluate before or after
// the other argument, but the pointer must be read AFTER the call has completed.
template<class T> bool hasInterface(HRESULT result, T *const &pointer) {
    return SUCCEEDED(result) && pointer != nullptr;
}
}
