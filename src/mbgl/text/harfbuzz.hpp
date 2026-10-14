#pragma once

#include <mbgl/text/glyph.hpp>

#ifdef MLN_TEXT_SHAPING_HARFBUZZ
#include "freetype.hpp"
#endif

namespace mbgl {

#ifndef MLN_TEXT_SHAPING_HARFBUZZ
struct FreeTypeLibrary {};
#endif

struct HBShapeAdjust {
    float x_offset;
    float y_offset;

    float advance; // advanceX
    char32_t codepoint = 0;
    bool clusterEnd = true;
    uint32_t cluster = 0;

    HBShapeAdjust(float x, float y, float a, char32_t codepoint_ = 0, bool clusterEnd_ = true, uint32_t cluster_ = 0)
        : x_offset(x),
          y_offset(y),
          advance(a), codepoint(codepoint_), clusterEnd(clusterEnd_), cluster(cluster_) {}
};

class HBShaper {
public:
    explicit HBShaper(GlyphIDType type, const std::string &fontFileData, const FreeTypeLibrary &lib);
    ~HBShaper();

    void createComplexGlyphIDs(const std::u16string &text,
                               std::vector<GlyphID> &glyphIDs,
                               std::vector<HBShapeAdjust> &adjusts);
    Glyph rasterizeGlyph(const GlyphID &glyph);

    bool valid();

private:
    class Impl;

    std::unique_ptr<Impl> impl;
};

} // namespace mbgl
