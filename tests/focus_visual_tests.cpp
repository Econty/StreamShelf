#include "focus_visual.hpp"
#include <iostream>
#include <stdexcept>

using namespace ss;
static void check(bool value, const char *label) { if (!value) throw std::runtime_error(label); }
int main() {
    try {
        for (int fps : {30, 60, 120}) {
            FocusVisual visual;
            const float frame = 1.f / fps;
            check(visual.step(0, frame, false) == 0, "mouse/default focus is invisible");
            visual.show(1);
            float first = visual.step(1, frame, false);
            check(first > 0 && first < 1, "keyboard focus fades in");
            for (int i=1;i<=fps;++i) visual.step(1.+double(i)/fps,frame,false);
            check(visual.opacity > .99f, "focus becomes clearly visible at every frame rate");
            visual.show(3.5);
            visual.step(4.1,frame,false);
            check(visual.opacity > .99f, "another Tab refreshes the lifetime");
            float before = visual.opacity;
            float fading = visual.step(6.5,frame,false);
            check(fading > 0 && fading < before, "expiry fades without dropping immediately");
            for(int i=1;i<=fps;++i)visual.step(6.5+double(i)/fps,frame,false);
            check(visual.opacity == 0, "expired focus finishes dissolving");
            visual.show(8);
            check(visual.step(8,frame,false)>0, "Tab reveals focus after expiry");
            visual.hide();
            for(int i=1;i<=fps;++i)visual.step(8.+double(i)/fps,frame,false);
            check(visual.opacity == 0, "mouse interaction dismisses the keyboard cue");
        }
        FocusVisual reduced;
        reduced.show(2);
        check(reduced.step(2,.016f,true)==1, "reduced motion immediately reveals focus");
        check(reduced.step(5,.016f,true)==0, "reduced motion still observes the timeout");
        reduced.show(6);
        reduced.step(6,.016f,true); reduced.hide();
        check(reduced.step(6.1,.016f,true)==0, "reduced motion mouse dismissal is immediate");
        std::cout << "PASS focus visibility lifetime, repeated Tab, mouse dismissal and 30/60/120 FPS fades\n";
        return 0;
    } catch(const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
