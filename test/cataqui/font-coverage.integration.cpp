#include <mbgl/text/glyph.hpp>
#include <cassert>
#include <iostream>
#include <mbgl/text/font_text.hpp>
#include <mbgl/text/shaping.hpp>
#include <mbgl/text/bidi.hpp>
#include <mbgl/style/parser.hpp>
#include <mbgl/util/i18n.hpp>

int main() {
    mbgl::style::Parser parser;
    const auto error = parser.parse(R"({"version":8,"font-faces":{},"sources":{},"layers":[]})");
    assert(!error);
    assert(parser.fontFaces);
    mbgl::style::Parser populated;
    const auto populatedError = populated.parse(R"({"version":8,"font-faces":{"Inter SemiBold":[{"url":"cataqui://fonts/test","unicode-range":["U+3000-9FFF"]}]},"sources":{},"layers":[]})");
    assert(!populatedError);
    assert(populated.fontFaces && populated.fontFaces->size() == 1);
    assert(populated.fontFaces->resolve({"Inter SemiBold"}, U'東') != mbgl::FontPBF);

    mbgl::FontFaces faces;
    mbgl::FontFace first("Inter Regular", "cataqui://fonts/first", {{0x3000, 0x9fff}, {0x20000, 0x2ffff}});
    first.type = static_cast<mbgl::GlyphIDType>(1);
    mbgl::FontFace second("Inter Regular", "cataqui://fonts/second", {{0x8000, 0x10ffff}});
    second.type = static_cast<mbgl::GlyphIDType>(2);
    faces.push_back(first);
    faces.push_back(second);
    faces.supportedRanges = {{32, 126}, {0x3000, 0x9fff}, {0x20000, 0x2ffff}};
    faces.compile();
    assert(mbgl::fontTextSupported(u"Cafe 東京", faces));
    assert(!mbgl::fontTextSupported(u"\U0010ffff", faces));
    const mbgl::PropertyMap names{{"name", std::string("\xf4\x8f\xbf\xbf")}, {"name2", std::string("東京")}};
    assert(mbgl::renderableSourceName(names, faces) == std::optional<std::string>("東京"));
    assert(!mbgl::renderableSourceName({{"name", std::string("  ")}}, faces));
    assert(faces.supports(U'A'));
    assert(faces.supports(U'\U00020bb7'));
    assert(!faces.supports(0x10ffff));
    assert(faces.resolve({"Inter Regular"}, U'A') == mbgl::FontPBF);
    assert(faces.resolve({"Inter Regular"}, U'東') == first.type);
    assert(faces.resolve({"Inter Regular"}, U'\U00020bb7') == first.type);
    assert(faces.resolve({"Inter Regular"}, 0xa000) == second.type);
    assert(faces.resolve({"Other"}, U'東') == mbgl::FontPBF);
    assert(faces.resolve({"Other", "Inter Regular"}, U'東') == first.type);
    const auto latin = mbgl::fontTextRuns(u"Restaurant 東京");
    assert(latin.size() == 1 && !latin.front().rtl);
    const auto rtl = mbgl::fontTextRuns(u"مطعم Tokyo");
    assert(rtl.size() == 2 && rtl.front().text == u"Tokyo" && !rtl.front().rtl && rtl.back().rtl);
    const auto lines = mbgl::fontTextRuns(u"東京\n東京");
    assert(lines.size() == 3 && lines[1].text == u"\n");
    assert(mbgl::util::i18n::allowsIdeographicBreaking(char32_t(0x20bb7)));
    mbgl::TaggedString shaped;
    const mbgl::FontStack stack{"Fallback"};
    auto adjustments = std::make_shared<std::vector<mbgl::HBShapeAdjust>>();
    std::u16string indices;
    mbgl::GlyphMap glyphs;
    const auto hash = mbgl::FontStackHasher()(stack);
    for (char16_t i = 1; i <= 6; ++i) {
        indices += i;
        adjustments->emplace_back(0, 0, 24, U'東');
        auto glyph = mbgl::makeMutable<mbgl::Glyph>();
        glyph->id = mbgl::GlyphID(i, first.type);
        glyph->metrics.advance = 24;
        const auto id = glyph->id;
        glyphs[hash].emplace(id, std::move(glyph));
    }
    shaped.addTextSection(indices, 1, stack, first.type, adjustments);
    shaped.setHBShaped(true);
    mbgl::BiDi bidi;
    const auto result = mbgl::getShaping(shaped, 72, 24, mbgl::style::SymbolAnchorType::Center,
                                        mbgl::style::TextJustifyType::Center, 0, {0, 0},
                                        mbgl::WritingModeType::Horizontal, bidi, glyphs, {{hash, {}}}, {}, 24, 24, false);
    assert(result.positionedLines.size() == 2);
    assert(result.positionedLines[0].positionedGlyphs.size() == 3);


    // Wrapped RTL labels begin with the first logical word, not the last word in the visual glyph buffer.
    const std::u16string hebrew = u"אב גד";
    auto rtlAdjusts = std::make_shared<std::vector<mbgl::HBShapeAdjust>>();
    mbgl::GlyphMap rtlGlyphs;
    std::u16string visual;
    for (char16_t code = 5; code > 0; --code) {
        visual += code;
        rtlAdjusts->emplace_back(0, 0, 24, hebrew[code - 1], true, code - 1);
        auto glyph = mbgl::makeMutable<mbgl::Glyph>();
        glyph->id = mbgl::GlyphID(code, first.type);
        glyph->metrics.advance = 24;
        const auto id = glyph->id;
        rtlGlyphs[hash].emplace(id, std::move(glyph));
    }
    mbgl::TaggedString rtlText;
    rtlText.addTextSection(visual, 1, stack, first.type, rtlAdjusts);
    rtlText.setLogicalText(hebrew);
    rtlText.setHBShaped(true);
    const auto wrappedRtl = mbgl::getShaping(rtlText, 72, 24, mbgl::style::SymbolAnchorType::Center,
                                           mbgl::style::TextJustifyType::Center, 0, {0, 0},
                                           mbgl::WritingModeType::Horizontal, bidi, rtlGlyphs, {{hash, {}}}, {}, 24, 24, false);
    assert(wrappedRtl.positionedLines.size() == 2);
    assert(wrappedRtl.positionedLines.front().positionedGlyphs.back().glyph.complex.code == 1);
    assert(wrappedRtl.positionedLines.back().positionedGlyphs.back().glyph.complex.code == 4);
}
