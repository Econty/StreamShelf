#pragma once
#include "platform.hpp"
#include <set>
namespace ss {
enum class TrackRole { Video, Audio, Subtitle };
struct PlannedTrack {
    size_t input = 0;
    int stream = 0;
    TrackRole role = TrackRole::Video;
    std::string language, title;
    bool byDefault = false, forced = false;
};
struct ContainerPlan {
    std::vector<PlannedTrack> tracks;
    void validate(size_t inputs) const {
        if (inputs == 0 || inputs > 64 || tracks.empty() || tracks.size() > 256)
            throw std::runtime_error("Добавьте от 1 до 64 источников и хотя бы одну дорожку");
        bool playable = false;
        int defaults[3]{};
        std::set<std::pair<size_t,int>> seen;
        for (const auto &t : tracks) {
            int role = int(t.role);
            if (role < 0 || role > 2 || t.input >= inputs || t.stream < 0 || !seen.emplace(t.input,t.stream).second)
                throw std::runtime_error("Некорректная или повторяющаяся дорожка");
            if (t.forced && t.role != TrackRole::Subtitle)
                throw std::runtime_error("Признак принудительной дорожки доступен только субтитрам");
            if (t.byDefault && ++defaults[role] > 1)
                throw std::runtime_error("В каждой категории может быть только одна дорожка по умолчанию");
            playable |= t.role != TrackRole::Subtitle;
        }
        if (!playable) throw std::runtime_error("Добавьте видео или аудио");
    }
    // Construct a proposal only. The caller must obtain user consent before using it.
    ContainerPlan availableOnly(const std::vector<bool> &available) const {
        validate(available.size());
        ContainerPlan result;
        std::vector<size_t> remap(available.size());
        size_t next = 0;
        for (size_t i = 0; i < available.size(); ++i)
            if (available[i]) remap[i] = next++;
        for (auto track : tracks) {
            if (!available[track.input]) continue;
            track.input = remap[track.input];
            result.tracks.push_back(std::move(track));
        }
        result.validate(next);
        return result;
    }
    std::vector<std::wstring> ffmpegArgs(size_t inputs) const {
        validate(inputs);
        std::vector<std::wstring> args;
        int index[3]{};
        for (const auto &t : tracks) {
            args.insert(args.end(), {L"-map", std::to_wstring(t.input) + L":" + std::to_wstring(t.stream)});
        }
        for (const auto &t : tracks) {
            int role = int(t.role), n = index[role]++;
            std::wstring spec = std::wstring(role == 0 ? L"v" : role == 1 ? L"a" : L"s") + L":" + std::to_wstring(n);
            std::wstring disposition = t.byDefault ? L"default" : L"0";
            if (t.forced) disposition = t.byDefault ? L"default+forced" : L"forced";
            args.insert(args.end(), {L"-disposition:" + spec, disposition});
            if (!t.language.empty()) args.insert(args.end(), {L"-metadata:s:" + spec, L"language=" + wide(t.language)});
            if (!t.title.empty()) args.insert(args.end(), {L"-metadata:s:" + spec, L"title=" + wide(t.title)});
        }
        return args;
    }
};
}


