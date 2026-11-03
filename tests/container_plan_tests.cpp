#include "container_plan.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace ss;
int main() {
    ContainerPlan p;
    p.tracks = {{1, 2, TrackRole::Audio, "rus", "Русский", true, false}, {0, 0, TrackRole::Video, "", "", true, false}, {1, 3, TrackRole::Subtitle, "eng", "", false, true}};
    auto args = p.ffmpegArgs(2);
    assert(args[1] == L"1:2" && args[3] == L"0:0" && args[5] == L"1:3");
    assert(std::find(args.begin(), args.end(), L"-disposition:s:0") != args.end());
    assert(std::find(args.begin(), args.end(), L"forced") != args.end());
    auto partial = p.availableOnly({false, true});
    assert(partial.tracks.size() == 2 && partial.tracks[0].input == 0 && partial.tracks[1].input == 0);
    assert(p.tracks.size() == 3 && p.tracks[0].input == 1);
    bool noMedia = false; try { p.availableOnly({false, false}); } catch (...) { noMedia = true; } assert(noMedia);
    p.tracks.push_back(p.tracks[0]);
    bool rejected = false; try { p.validate(2); } catch (...) { rejected = true; } assert(rejected);
    p.tracks.pop_back(); p.tracks[2].input = 2;
    rejected = false; try { p.validate(2); } catch (...) { rejected = true; } assert(rejected);
    std::cout << "Container plan checks passed\n";
}
