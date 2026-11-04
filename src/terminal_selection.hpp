#pragma once
#include <algorithm>
namespace ss {
inline float terminalSelectionScroll(float pointerY, float viewTop, float viewHeight, float dt) {
    constexpr float edge = 24.f;
    float speed = 0;
    if (pointerY < viewTop + edge) speed = -std::min(650.f, 90.f + (viewTop + edge - pointerY) * 9.f);
    else if (pointerY > viewTop + viewHeight - edge) speed = std::min(650.f, 90.f + (pointerY - (viewTop + viewHeight - edge)) * 9.f);
    return speed * std::clamp(dt,0.f,.08f);
}
inline bool preserveTerminalSelectionOnWheel(bool terminalPage, bool mouseDown, bool selecting) {
    return terminalPage && mouseDown && selecting;
}
}
