#include <mbgl/text/glyph.hpp>
#include <mbgl/util/i18n.hpp>

#include <algorithm>
#include <set>

namespace mbgl {

GlyphRange::GlyphRange(uint32_t first_, uint32_t second_, GlyphIDType type_)
    : first((uint16_t)first_),
      second((uint16_t)second_),
      type(type_) {}

bool GlyphRange::operator==(const GlyphRange &other) const {
    return first == other.first && second == other.second && type == other.type;
}

bool GlyphRange::operator<(const GlyphRange &other) const {
    if (first < other.first) return true;
    if (first > other.first) return false;

    if (second < other.second) return true;
    if (second > other.second) return false;

    return type < other.type;
}

GlyphIDType genNewGlyphIDType() {
    static short glyphType = GlyphIDType::FontPBF;
    ++glyphType;
    if (glyphType == GlyphIDType::FontPBF) ++glyphType;
    // Intentionally casting to extend the enum's value space for runtime-generated IDs.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<GlyphIDType>(glyphType);
}

GlyphIDType genNewGlyphIDType(const std::string &url,
                              const FontStack &fontStack,
                              const std::vector<std::pair<uint32_t, uint32_t>> &pairs) {
    static std::map<std::string, std::map<std::string, std::map<std::size_t, GlyphIDType>>> glyphTypes;

    std::size_t hash = 0;
    for (auto &pair : pairs) {
        mbgl::util::hash_combine(hash, pair.first);
        mbgl::util::hash_combine(hash, pair.second);
    }

    auto family = fontStackToString(fontStack);

    auto type = glyphTypes[url][family][hash];
    if (type == GlyphIDType::FontPBF) {
        type = genNewGlyphIDType();
        glyphTypes[url][family][hash] = type;
    }

    return type;
}

void FontFaces::compile() {
    coverage.clear();
    using Event = std::pair<std::size_t, bool>;
    std::map<std::string, std::map<uint32_t, std::vector<Event>>> events;
    for (std::size_t i = 0; i < size(); ++i) {
        for (const auto& range : at(i).ranges) {
            if (range.first > range.second || range.second > 0x10ffff) continue;
            events[at(i).name][range.first].emplace_back(i, true);
            events[at(i).name][range.second + 1].emplace_back(i, false);
        }
    }
    for (const auto& family : events) {
        std::map<std::size_t, std::size_t> active;
        uint32_t first = 0;
        for (const auto& boundary : family.second) {
            if (!active.empty() && first < boundary.first) {
                coverage[family.first].push_back({first, boundary.first - 1, at(active.begin()->first).type});
            }
            for (const auto& event : boundary.second) {
                if (event.second) ++active[event.first];
                else if (--active[event.first] == 0) active.erase(event.first);
            }
            first = boundary.first;
        }
    }
}

bool FontFaces::supports(char32_t codepoint) const {
    if (supportedRanges.empty()) return true; // Ordinary upstream styles do not declare owned coverage.
    auto range = std::upper_bound(supportedRanges.begin(), supportedRanges.end(), codepoint,
                                  [](char32_t value, const FontFace::Range& item) { return value < item.first; });
    if (range == supportedRanges.begin()) return false;
    return codepoint <= (--range)->second;
}

GlyphIDType FontFaces::resolve(const FontStack& stack, char32_t codepoint, const std::string& locale_) const {
    for (const auto& name : stack) {
        if (!locale_.empty() && locale_ != "default" && codepoint >= 0x3000) {
            const auto regional = coverage.find(name + "@" + locale_);
            if (regional != coverage.end()) {
                const auto& spans = regional->second;
                auto span = std::upper_bound(spans.begin(), spans.end(), codepoint,
                                             [](char32_t value, const Span& item) { return value < item.first; });
                if (span != spans.begin() && codepoint <= (--span)->last) return span->type;
            }
        }
        const auto family = coverage.find(name);
        if (family == coverage.end()) continue;
        const auto& spans = family->second;
        auto span = std::upper_bound(spans.begin(), spans.end(), codepoint,
                                     [](char32_t value, const Span& item) { return value < item.first; });
        if (span != spans.begin()) {
            --span;
            if (codepoint <= span->last) return span->type;
        }
    }
    return FontPBF;
}

// Font PBF glyphs use BMP codepoints; supplementary text is shaped to font glyph IDs.
GlyphRange getGlyphRange(GlyphID glyph) {
    unsigned start = (glyph.complex.code / 256) * 256;
    unsigned end = (start + 255);
    start = std::min<unsigned int>(start, 65280);
    end = std::min<unsigned int>(end, 65535);
    return {start, end, glyph.complex.type};
}

} // namespace mbgl
