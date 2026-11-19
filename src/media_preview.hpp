#pragma once
#include "platform.hpp"

namespace ss {
enum class MediaPreviewKind { image, video, audio };
PreviewImage decodeMediaPreview(const fs::path &ffmpeg, const fs::path &ffprobe, const std::wstring &input,
                                bool remote, MediaPreviewKind kind, std::atomic_bool &cancel);
}
