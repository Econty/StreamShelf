#include "assembly_batch.hpp"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>
using namespace ss;
int main() {
    AssemblyBatch batch{{SourceState::Ready, SourceState::Running}};
    assert(batch.state() == AssemblyState::Waiting);
    batch.sources[1] = SourceState::Failed;
    assert(batch.state() == AssemblyState::NeedsDecision);
    bool rejected = false; try { batch.available(false); } catch (...) { rejected = true; } assert(rejected);
    auto mask = batch.available(true); assert(mask[0] && !mask[1]);
    batch.sources[1] = SourceState::Ready;
    assert(batch.state() == AssemblyState::Ready && batch.available(false)[1]);
    batch.sources = {SourceState::Failed, SourceState::Cancelled};
    assert(batch.state() == AssemblyState::NoUsableSources);
    batch.cancelled = true; assert(batch.state() == AssemblyState::Cancelled);
    std::cout << "Assembly batch checks passed\n";
}
