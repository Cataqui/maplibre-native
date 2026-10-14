#pragma once
#include <string>
#include <optional>
#include <mbgl/util/feature.hpp>
#include <vector>

namespace mbgl {
struct FontFaces;
bool fontTextSupported(const std::u16string&, const FontFaces&);
std::optional<std::string> renderableSourceName(const PropertyMap&, const FontFaces&);

struct FontTextRun {
    std::u16string text;
    bool rtl = false;
    std::size_t offset = 0;
};
// Preserve logical character order inside each visual run for HarfBuzz.
std::vector<FontTextRun> fontTextRuns(const std::u16string&);
std::vector<FontTextRun> fontTextLineRuns(const std::u16string&, std::size_t begin, std::size_t end);
}
