#pragma once
#include <stdexcept>
#include <vector>
namespace ss {
enum class SourceState { Waiting, Running, Ready, Failed, Cancelled };
enum class AssemblyState { Waiting, Ready, NeedsDecision, NoUsableSources, Cancelled };
struct AssemblyBatch {
    std::vector<SourceState> sources;
    bool cancelled = false;
    AssemblyState state() const {
        if (cancelled) return AssemblyState::Cancelled;
        if (sources.empty()) return AssemblyState::NoUsableSources;
        bool ready = false, missing = false;
        for (auto source : sources) {
            if (source == SourceState::Waiting || source == SourceState::Running) return AssemblyState::Waiting;
            if (source == SourceState::Ready) ready = true;
            else missing = true;
        }
        if (!ready) return AssemblyState::NoUsableSources;
        return missing ? AssemblyState::NeedsDecision : AssemblyState::Ready;
    }
    std::vector<bool> available(bool acceptPartial) const {
        auto current = state();
        if (current != AssemblyState::Ready && !(acceptPartial && current == AssemblyState::NeedsDecision))
            throw std::runtime_error("Сборка недоступна до завершения загрузок и подтверждения пользователя");
        std::vector<bool> result;
        for (auto source : sources) result.push_back(source == SourceState::Ready);
        return result;
    }
};
}
