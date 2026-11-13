#pragma once
#ifdef STREAMSHELF_GPU_TEST
#include "core.hpp"
#include <filesystem>
#define GL_GLEXT_PROTOTYPES
#include "glcorearb.h"
namespace ss {
namespace fs = std::filesystem;
std::string readFile(const fs::path &, size_t limit = 64 * 1024 * 1024);
} // namespace ss
#else
#include "platform.hpp"
#include <GL/gl.h>
#include "glext.h"
#endif
#include <stack>
namespace ss {
struct Glyph {
    float advance, left, top;
    int x, y, w, h;
};
class Canvas {
    struct Vertex {
        float x, y, u, v, r, g, b, a, p, q, radius, kind;
    };
    struct Batch {
        Rect clip;
        int start, count;
        unsigned image=0;
    };
    std::vector<Vertex> vertices;
    std::vector<Batch> batches;
    std::vector<Rect> clips;
    // A texture may still be referenced by a batch queued for this frame.
    std::vector<unsigned> retiredImages;
    int batchStart = 0;
    unsigned batchImage=0;
    unsigned program = 0, vao = 0, vbo = 0, font = 0, history = 0, snapshot = 0, fbo = 0;
    int pixelW = 0, pixelH = 0, snapshotW = 0, snapshotH = 0;
    int frozenW = 0, frozenH = 0;
    float width = 0, height = 0;
    std::map<uint32_t, Glyph> glyphs;
    std::map<uint64_t, float> kern;
    void seal();
    void vertex(float, float, float, float, Ink, float, float, float, float);
    void quad(Rect, float, float, float, float, Ink, float = 0, float = 0, float = 0, float = 0);

  public:
    float opacity = 1, offsetX = 0, offsetY = 0;
    bool frozen = false;
    void init(const fs::path &);
    void begin(float, float, int, int, bool deferSnapshots = false);
    void rect(Rect, float, Ink);
    void border(Rect, float, Ink, Ink, float thickness = 1);
    void circle(float, float, float, Ink);
    void glow(Rect, Ink);
    void line(float, float, float, float, float, Ink);
    void text(const std::string &, float, float, float, Ink, float maxWidth = 1e8f);
    float measure(const std::string &, float);
    void push(Rect);
    void pop();
    Rect clip() const {
        return clips.back();
    }
    void drawSnapshot(Rect, float, bool blur = false);
    unsigned createImage(const PreviewImage &);
    void deleteImage(unsigned);
    void image(unsigned,Rect,int,int,float alpha=1);
    void freeze();
    void render();
    void capture();
    void destroy();
    std::string driver();
};
} // namespace ss
