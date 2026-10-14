#include "font_text.hpp"
#include <unicode/ubidi.h>
#include <unicode/uchar.h>
#include <unicode/utf16.h>
#include <mbgl/text/glyph.hpp>
#include <mbgl/util/utf.hpp>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace mbgl {
bool fontTextSupported(const std::u16string& text, const FontFaces& faces) {
    for (int32_t index = 0; index < static_cast<int32_t>(text.size());) {
        UChar32 codepoint;
        U16_NEXT(text.data(), index, static_cast<int32_t>(text.size()), codepoint);
        if (codepoint < 0 || (codepoint >= 32 &&
            !u_hasBinaryProperty(codepoint, UCHAR_DEFAULT_IGNORABLE_CODE_POINT) &&
            !faces.supports(static_cast<char32_t>(codepoint)))) return false;
    }
    return true;
}

std::optional<std::string> renderableSourceName(const PropertyMap& properties, const FontFaces& faces) {
    std::vector<std::pair<std::string, const Value*>> names;
    for (const auto& entry : properties) {
        const auto& key = entry.first;
        if ((key == "name" || key == "name2" || key == "name3" ||
             key.starts_with("name:") || key.starts_with("name_")) && entry.second.is<std::string>()) {
            names.emplace_back(key, &entry.second);
        }
    }
    std::sort(names.begin(), names.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
    if (names.size() > 64) names.resize(64);
    for (const auto& name : names) {
        const auto& value = name.second->get<std::string>();
        if (value.empty() || value.size() > 4096) continue;
        const auto text = util::convertUTF8ToUTF16(value);
        if (std::any_of(text.begin(), text.end(), [](char16_t c) { return !u_isUWhiteSpace(c) && !u_hasBinaryProperty(c, UCHAR_DEFAULT_IGNORABLE_CODE_POINT); }) &&
            fontTextSupported(text, faces)) return value;
    }
    return std::nullopt;
}

std::vector<FontTextRun> fontTextRuns(const std::u16string& text) {
    std::vector<FontTextRun> runs;
    std::unique_ptr<UBiDi, decltype(&ubidi_close)> bidi(ubidi_open(), &ubidi_close);
    if (!bidi) throw std::runtime_error("Font bidi allocation failed");
    std::size_t start = 0;
    while (start < text.size()) {
        auto end = text.find(u'\n', start);
        if (end == std::u16string::npos) end = text.size();
        if (end > start) {
            UErrorCode error = U_ZERO_ERROR;
            ubidi_setPara(bidi.get(), reinterpret_cast<const UChar*>(text.data() + start),
                          static_cast<int32_t>(end - start), UBIDI_DEFAULT_LTR, nullptr, &error);
            const auto count = ubidi_countRuns(bidi.get(), &error);
            if (U_FAILURE(error)) throw std::runtime_error("Font bidi run resolution failed");
            for (int32_t i = 0; i < count; ++i) {
                int32_t begin = 0;
                int32_t length = 0;
                const auto direction = ubidi_getVisualRun(bidi.get(), i, &begin, &length);
                runs.push_back({text.substr(start + static_cast<std::size_t>(begin), static_cast<std::size_t>(length)),
                                direction == UBIDI_RTL, start + static_cast<std::size_t>(begin)});
            }
        }
        if (end < text.size()) runs.push_back({u"\n", false, end});
        start = end + 1;
    }
    return runs;
}

std::vector<FontTextRun> fontTextLineRuns(const std::u16string& text, std::size_t begin, std::size_t end) {
    if (begin >= end) return {};
    if (text[end - 1] == u'\n') --end;
    if (begin >= end) return {};
    const auto separator = begin == 0 ? std::u16string::npos : text.rfind(u'\n', begin - 1);
    const auto paragraphBegin = separator == std::u16string::npos ? 0 : separator + 1;
    auto paragraphEnd = text.find(u'\n', begin);
    if (paragraphEnd == std::u16string::npos) paragraphEnd = text.size();
    if (end > paragraphEnd) throw std::runtime_error("Font line crosses a paragraph boundary");
    std::unique_ptr<UBiDi, decltype(&ubidi_close)> paragraph(ubidi_open(), &ubidi_close);
    std::unique_ptr<UBiDi, decltype(&ubidi_close)> line(ubidi_open(), &ubidi_close);
    if (!paragraph || !line) throw std::runtime_error("Font bidi allocation failed");
    UErrorCode error = U_ZERO_ERROR;
    ubidi_setPara(paragraph.get(), reinterpret_cast<const UChar*>(text.data() + paragraphBegin),
                  static_cast<int32_t>(paragraphEnd - paragraphBegin), UBIDI_DEFAULT_LTR, nullptr, &error);
    ubidi_setLine(paragraph.get(), static_cast<int32_t>(begin - paragraphBegin),
                  static_cast<int32_t>(end - paragraphBegin), line.get(), &error);
    const auto count = ubidi_countRuns(line.get(), &error);
    if (U_FAILURE(error)) throw std::runtime_error("Font bidi line resolution failed");
    std::vector<FontTextRun> runs;
    for (int32_t index = 0; index < count; ++index) {
        int32_t start = 0, length = 0;
        const auto direction = ubidi_getVisualRun(line.get(), index, &start, &length);
        const auto offset = begin + static_cast<std::size_t>(start);
        runs.push_back({text.substr(offset, static_cast<std::size_t>(length)), direction == UBIDI_RTL, offset});
    }
    return runs;
}
}
