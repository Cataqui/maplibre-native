#include <mbgl/text/shaping.hpp>
#include <mbgl/text/font_text.hpp>
#include <mbgl/util/constants.hpp>
#include <mbgl/util/i18n.hpp>
#include <mbgl/layout/symbol_feature.hpp>
#include <mbgl/math/minmax.hpp>
#include <mbgl/text/bidi.hpp>

#include <algorithm>
#include <list>
#include <numeric>
#include <cmath>

namespace {
// Zero width space that is used to suggest break points for Japanese labels.
char16_t ZWSP = u'\u200b';
} // namespace

namespace mbgl {

// static
AnchorAlignment AnchorAlignment::getAnchorAlignment(style::SymbolAnchorType anchor) {
    AnchorAlignment result(0.5f, 0.5f);

    switch (anchor) {
        case style::SymbolAnchorType::Right:
        case style::SymbolAnchorType::TopRight:
        case style::SymbolAnchorType::BottomRight:
            result.horizontalAlign = 1.0f;
            break;
        case style::SymbolAnchorType::Left:
        case style::SymbolAnchorType::TopLeft:
        case style::SymbolAnchorType::BottomLeft:
            result.horizontalAlign = 0.0f;
            break;
        default:
            break;
    }

    switch (anchor) {
        case style::SymbolAnchorType::Bottom:
        case style::SymbolAnchorType::BottomLeft:
        case style::SymbolAnchorType::BottomRight:
            result.verticalAlign = 1.0f;
            break;
        case style::SymbolAnchorType::Top:
        case style::SymbolAnchorType::TopLeft:
        case style::SymbolAnchorType::TopRight:
            result.verticalAlign = 0.0f;
            break;
        default:
            break;
    }

    return result;
}

style::TextJustifyType getAnchorJustification(style::SymbolAnchorType anchor) {
    switch (anchor) {
        case style::SymbolAnchorType::Right:
        case style::SymbolAnchorType::TopRight:
        case style::SymbolAnchorType::BottomRight:
            return style::TextJustifyType::Right;
        case style::SymbolAnchorType::Left:
        case style::SymbolAnchorType::TopLeft:
        case style::SymbolAnchorType::BottomLeft:
            return style::TextJustifyType::Left;
        default:
            return style::TextJustifyType::Center;
    }
}

PositionedIcon PositionedIcon::shapeIcon(const ImagePosition& image,
                                         const std::array<float, 2>& iconOffset,
                                         style::SymbolAnchorType iconAnchor) {
    AnchorAlignment anchorAlign = AnchorAlignment::getAnchorAlignment(iconAnchor);
    float dx = iconOffset[0];
    float dy = iconOffset[1];
    float left = dx - image.displaySize()[0] * anchorAlign.horizontalAlign;
    float right = left + image.displaySize()[0];
    float top = dy - image.displaySize()[1] * anchorAlign.verticalAlign;
    float bottom = top + image.displaySize()[1];

    Padding collisionPadding;
    if (image.content) {
        auto& content = *image.content;
        const auto pixelRatio = image.pixelRatio;
        collisionPadding.left = content.left / pixelRatio;
        collisionPadding.top = content.top / pixelRatio;
        collisionPadding.right = image.displaySize()[0] - content.right / pixelRatio;
        collisionPadding.bottom = image.displaySize()[1] - content.bottom / pixelRatio;
    }

    return PositionedIcon{image, top, bottom, left, right, collisionPadding};
}

PositionedIcon PositionedIcon::applyTextFit() const {
    if (!_image.textFitWidth && !_image.textFitHeight) {
        return *this;
    }
    const float width = _right - _left;
    const float height = _bottom - _top;

    const float contentWidth = _image.content->right - _image.content->left;
    const float contentHeight = _image.content->bottom - _image.content->top;
    const float contentAspectRatio = contentWidth / contentHeight;
    const auto textFitWidth = _image.textFitWidth.value();
    const auto textFitHeight = _image.textFitHeight.value();
    float newLeft = _left;
    float newRight = _right;
    float newTop = _top;
    float newBottom = _bottom;

    if (textFitHeight == style::TextFit::proportional) {
        if ((textFitWidth == style::TextFit::stretchOnly && (width / height) < contentAspectRatio) ||
            textFitWidth == style::TextFit::proportional) {
            float newIconWidth = std::ceil(height * contentAspectRatio);
            newLeft *= newIconWidth / width;
            newRight = newLeft + newIconWidth;
        }
    } else if (textFitWidth == style::TextFit::proportional) {
        if (textFitHeight == style::TextFit::stretchOnly && contentAspectRatio != 0 &&
            (width / height) > contentAspectRatio) {
            float newIconHeight = std::ceil(width / contentAspectRatio);
            newTop *= newIconHeight / height;
            newBottom = newTop + newIconHeight;
        }
    } else {
        // If neither textFitHeight nor textFitWidth are proportional then
        // there is no effect since the content rectangle should be precisely
        // matched to the content
    }
    return PositionedIcon{_image, newTop, newBottom, newLeft, newRight, _collisionPadding};
}

void PositionedIcon::fitIconToText(const Shaping& shapedText,
                                   const style::IconTextFitType textFit,
                                   const std::array<float, 4>& padding,
                                   const std::array<float, 2>& iconOffset,
                                   const float fontScale) {
    assert(textFit != style::IconTextFitType::None);
    assert(shapedText);

    // We don't respect the icon-anchor, because icon-text-fit is set. Instead,
    // the icon will be centered on the text, then stretched in the given
    // dimensions.

    const float textLeft = shapedText.left * fontScale;
    const float textRight = shapedText.right * fontScale;

    if (textFit == style::IconTextFitType::Width || textFit == style::IconTextFitType::Both) {
        // Stretched horizontally to the text width
        _left = iconOffset[0] + textLeft - padding[3];
        _right = iconOffset[0] + textRight + padding[1];
    } else {
        // Centered on the text
        _left = iconOffset[0] + (textLeft + textRight - image().displaySize()[0]) / 2.0f;
        _right = _left + image().displaySize()[0];
    }

    const float textTop = shapedText.top * fontScale;
    const float textBottom = shapedText.bottom * fontScale;
    if (textFit == style::IconTextFitType::Height || textFit == style::IconTextFitType::Both) {
        // Stretched vertically to the text height
        _top = iconOffset[1] + textTop - padding[0];
        _bottom = iconOffset[1] + textBottom + padding[2];
    } else {
        // Centered on the text
        _top = iconOffset[1] + (textTop + textBottom - image().displaySize()[1]) / 2.0f;
        _bottom = _top + image().displaySize()[1];
    }
}

void align(Shaping& shaping,
           float justify,
           float horizontalAlign,
           float verticalAlign,
           float maxLineLength,
           float maxLineHeight,
           float lineHeight,
           float blockHeight,
           std::size_t lineCount) {
    const float shiftX = (justify - horizontalAlign) * maxLineLength;
    float shiftY = 0.0f;

    if (maxLineHeight != lineHeight) {
        shiftY = -blockHeight * verticalAlign - Shaping::yOffset;
    } else {
        shiftY = (-verticalAlign * static_cast<float>(lineCount) + 0.5f) * lineHeight;
    }

    for (auto& line : shaping.positionedLines) {
        for (auto& positionedGlyph : line.positionedGlyphs) {
            positionedGlyph.x += shiftX;
            positionedGlyph.y += shiftY;
        }
    }
}

// justify left = 0, right = 1, center = .5
void justifyLine(std::vector<PositionedGlyph>& positionedGlyphs, float justify, float lineOffset) {
    if (!justify && !lineOffset) {
        return;
    }

    PositionedGlyph& lastGlyph = positionedGlyphs.back();
    const float lastAdvance = lastGlyph.metrics.advance * lastGlyph.scale;
    const auto lineIndent = static_cast<float>(lastGlyph.x + lastAdvance) * justify;
    for (auto& positionedGlyph : positionedGlyphs) {
        positionedGlyph.x -= lineIndent;
        positionedGlyph.y += lineOffset;
    }
}

float getGlyphAdvance(char16_t codePoint,
                      const SectionOptions& section,
                      const GlyphMap& glyphMap,
                      const ImagePositions& imagePositions,
                      float layoutTextSize,
                      float spacing,
                      const HBShapeAdjust* adjust = nullptr) {
    if (adjust) return adjust->advance * static_cast<float>(section.scale) + (adjust->advance > 0 ? spacing : 0);
    if (!section.imageID) {
        auto glyphs = glyphMap.find(section.fontStackHash);
        if (glyphs == glyphMap.end()) {
            return 0.0f;
        }
        auto it = glyphs->second.find(GlyphID(codePoint, section.type));
        if (it == glyphs->second.end() || !it->second) {
            return 0.0f;
        }
        return static_cast<float>((*it->second)->metrics.advance * section.scale) + spacing;
    } else {
        auto image = imagePositions.find(*section.imageID);
        if (image == imagePositions.end()) {
            return 0.0f;
        }
        return image->second.displaySize()[0] * static_cast<float>(section.scale) * util::ONE_EM / layoutTextSize +
               spacing;
    }
}

float determineAverageLineWidth(const TaggedString& logicalInput,
                                float spacing,
                                float maxWidth,
                                const GlyphMap& glyphMap,
                                const ImagePositions& imagePositions,
                                float layoutTextSize) {
    float totalWidth = 0;

    for (std::size_t i = 0; i < logicalInput.length(); i++) {
        const SectionOptions& section = logicalInput.getSection(i);
        char16_t codePoint = logicalInput.getCharCodeAt(i);
        totalWidth += getGlyphAdvance(codePoint, section, glyphMap, imagePositions, layoutTextSize, spacing, logicalInput.getAdjust(i));
    }

    auto targetLineCount = static_cast<int32_t>(::fmax(1, std::ceil(totalWidth / maxWidth)));
    return totalWidth / targetLineCount;
}

float calculateBadness(const float lineWidth, const float targetWidth, const float penalty, const bool isLastBreak) {
    const float raggedness = static_cast<float>(std::pow(lineWidth - targetWidth, 2));
    if (isLastBreak) {
        // Favor finals lines shorter than average over longer than average
        if (lineWidth < targetWidth) {
            return raggedness / 2;
        } else {
            return raggedness * 2;
        }
    }
    if (penalty < 0) {
        return raggedness - penalty * penalty;
    }
    return raggedness + penalty * penalty;
}

float calculatePenalty(char32_t codePoint, char32_t nextCodePoint, bool penalizableIdeographicBreak) {
    float penalty = 0;
    // Force break on newline
    if (codePoint == 0x0a) {
        penalty -= 10000;
    }

    // Penalize open parenthesis at end of line
    if (codePoint == 0x28 || codePoint == 0xff08) {
        penalty += 50;
    }

    // Penalize close parenthesis at beginning of line
    if (nextCodePoint == 0x29 || nextCodePoint == 0xff09) {
        penalty += 50;
    }

    // Penalize breaks between characters that allow ideographic breaking because
    // they are less preferable than breaks at spaces (or zero width spaces)
    if (penalizableIdeographicBreak) {
        penalty += 150;
    }

    return penalty;
}

struct PotentialBreak {
    PotentialBreak(const std::size_t p_index,
                   const float p_x,
                   const PotentialBreak* p_priorBreak,
                   const float p_badness)
        : index(p_index),
          x(p_x),
          priorBreak(p_priorBreak),
          badness(p_badness) {}

    const std::size_t index;
    const float x;
    const PotentialBreak* priorBreak;
    const float badness;
};

PotentialBreak evaluateBreak(const std::size_t breakIndex,
                             const float breakX,
                             const float targetWidth,
                             const std::list<PotentialBreak>& potentialBreaks,
                             const float penalty,
                             const bool isLastBreak) {
    // We could skip evaluating breaks where the line length (breakX - priorBreak.x) > maxWidth
    //  ...but in fact we allow lines longer than maxWidth (if there's no break points)
    //  ...and when targetWidth and maxWidth are close, strictly enforcing maxWidth can give
    //     more lopsided results.

    const PotentialBreak* bestPriorBreak = nullptr;
    float bestBreakBadness = calculateBadness(breakX, targetWidth, penalty, isLastBreak);
    for (const auto& potentialBreak : potentialBreaks) {
        const float lineWidth = breakX - potentialBreak.x;
        float breakBadness = calculateBadness(lineWidth, targetWidth, penalty, isLastBreak) + potentialBreak.badness;
        if (breakBadness <= bestBreakBadness) {
            bestPriorBreak = &potentialBreak;
            bestBreakBadness = breakBadness;
        }
    }

    return {breakIndex, breakX, bestPriorBreak, bestBreakBadness};
}

std::set<std::size_t> leastBadBreaks(const PotentialBreak& lastLineBreak) {
    std::set<std::size_t> leastBadBreaks = {lastLineBreak.index};
    const PotentialBreak* priorBreak = lastLineBreak.priorBreak;
    while (priorBreak) {
        leastBadBreaks.insert(priorBreak->index);
        priorBreak = priorBreak->priorBreak;
    }
    return leastBadBreaks;
}

// We determine line breaks based on shaped text in logical order. Working in visual order would be
//  more intuitive, but we can't do that because the visual order may be changed by line breaks!
std::set<std::size_t> determineLineBreaks(const TaggedString& logicalInput,
                                          const float spacing,
                                          float maxWidth,
                                          const GlyphMap& glyphMap,
                                          const ImagePositions& imagePositions,
                                          float layoutTextSize) {
    if (!maxWidth) {
        return {};
    }

    if (logicalInput.empty()) {
        return {};
    }

    const float targetWidth = determineAverageLineWidth(
        logicalInput, spacing, maxWidth, glyphMap, imagePositions, layoutTextSize);

    std::list<PotentialBreak> potentialBreaks;
    float currentX = 0;
    // Find first occurrence of zero width space (ZWSP) character.
    const bool hasServerSuggestedBreaks = logicalInput.rawText().find_first_of(ZWSP) != std::string::npos;

    for (std::size_t i = 0; i < logicalInput.length(); i++) {
        const SectionOptions& section = logicalInput.getSection(i);
        const auto codePoint = logicalInput.getCodepoint(i);
        if (!util::i18n::isWhitespace(codePoint)) {
            currentX += getGlyphAdvance(logicalInput.getCharCodeAt(i), section, glyphMap, imagePositions, layoutTextSize, spacing, logicalInput.getAdjust(i));
        }

        // Ideographic characters, spaces, and word-breaking punctuation that
        // often appear without surrounding spaces.
        if (i < logicalInput.length() - 1) {
            const bool allowsIdeographicBreak = util::i18n::allowsIdeographicBreaking(codePoint);
            if ((section.imageID || allowsIdeographicBreak || util::i18n::allowsWordBreaking(codePoint)) &&
                (!logicalInput.getAdjust(i) || logicalInput.getAdjust(i)->clusterEnd)) {
                const bool penalizableIdeographicBreak = allowsIdeographicBreak && hasServerSuggestedBreaks;
                const std::size_t nextIndex = i + 1;
                potentialBreaks.push_back(evaluateBreak(
                    nextIndex,
                    currentX,
                    targetWidth,
                    potentialBreaks,
                    calculatePenalty(codePoint, logicalInput.getCodepoint(nextIndex), penalizableIdeographicBreak),
                    false));
            }
        }
    }

    return leastBadBreaks(evaluateBreak(logicalInput.length(), currentX, targetWidth, potentialBreaks, 0, true));
}

void shapeLines(Shaping& shaping,
                std::vector<TaggedString>& lines,
                const float spacing,
                const float lineHeight,
                const style::SymbolAnchorType textAnchor,
                const style::TextJustifyType textJustify,
                const WritingModeType writingMode,
                const GlyphMap& glyphMap,
                const GlyphPositions& glyphPositions,
                const ImagePositions& imagePositions,
                float layoutTextSize,
                bool allowVerticalPlacement) {
    float x = 0.0f;
    float y = Shaping::yOffset;

    float maxLineLength = 0.0f;
    float maxLineHeight = 0.0f;

    const float justify = textJustify == style::TextJustifyType::Right  ? 1.0f
                          : textJustify == style::TextJustifyType::Left ? 0.0f
                                                                        : 0.5f;

    for (TaggedString& line : lines) {
        // Collapse whitespace so it doesn't throw off justification
        line.trim();

        const double lineMaxScale = line.getMaxScale();
        const double maxLineOffset = (lineMaxScale - 1.0) * util::ONE_EM;
        double lineOffset = 0.0;
        shaping.positionedLines.emplace_back();
        auto& positionedLine = shaping.positionedLines.back();
        auto& positionedGlyphs = positionedLine.positionedGlyphs;

        if (line.empty()) {
            y += lineHeight; // Still need a line feed after empty line
            continue;
        }

        for (std::size_t i = 0; i < line.length(); i++) {
            const std::size_t sectionIndex = line.getSectionIndex(i);
            const SectionOptions& section = line.sectionAt(sectionIndex);
            const HBShapeAdjust* adjust = line.getAdjust(i);
            GlyphID codePoint(line.getCharCodeAt(i), section.type);

            double baselineOffset = 0.0;
            Rect<uint16_t> rect;
            GlyphMetrics metrics;
            float advance = 0.0f;
            float xHBOffset = 0.0f;
            float yHBOffset = 0.0f;
            float verticalAdvance = util::ONE_EM;
            double sectionScale = section.scale;
            assert(sectionScale);

            const bool vertical = !(
                writingMode == WritingModeType::Horizontal || codePoint.complex.type != GlyphIDType::FontPBF ||
                // Don't verticalize glyphs that have no upright orientation
                // if vertical placement is disabled.
                (!allowVerticalPlacement && !util::i18n::hasUprightVerticalOrientation(codePoint)) ||
                // If vertical placement is ebabled, don't verticalize glyphs
                // that are from complex text layout script, or whitespaces.
                (allowVerticalPlacement &&
                 (util::i18n::isWhitespace(codePoint) || util::i18n::isCharInComplexShapingScript(codePoint))));

            if (!section.imageID) {
                auto glyphPositionMap = glyphPositions.find(section.fontStackHash);
                if (glyphPositionMap == glyphPositions.end()) {
                    continue;
                }

                auto glyphPosition = glyphPositionMap->second.find(codePoint);
                if (glyphPosition != glyphPositionMap->second.end()) {
                    rect = glyphPosition->second.rect;
                    metrics = glyphPosition->second.metrics;
                } else {
                    auto glyphs = glyphMap.find(section.fontStackHash);
                    if (glyphs == glyphMap.end()) {
                        continue;
                    }

                    auto glyph = glyphs->second.find(codePoint);
                    if (glyph == glyphs->second.end() || !glyph->second) {
                        continue;
                    }
                    metrics = (*glyph->second)->metrics;
                }
                advance = static_cast<float>(metrics.advance);

                if (adjust) advance = adjust->advance;
                if (adjust) {
                    xHBOffset = (float)(adjust->x_offset * section.scale);
                    yHBOffset = (float)(adjust->y_offset * section.scale);
                }
                if (advance < 0.01f) {
                    // Advance is 0, this glyph should align to the preview glyph remove spacing
                    xHBOffset -= spacing;
                }

                // We don't know the baseline, but since we're laying out
                // at 24 points, we can calculate how much it will move when
                // we scale up or down.
                baselineOffset = (lineMaxScale - sectionScale) * util::ONE_EM;
            } else {
                auto image = imagePositions.find(*section.imageID);
                if (image == imagePositions.end()) {
                    continue;
                }
                shaping.iconsInText |= true;
                const auto& displaySize = image->second.displaySize();
                metrics.width = static_cast<uint32_t>(displaySize[0]);
                metrics.height = static_cast<uint32_t>(displaySize[1]);
                metrics.left = ImagePosition::padding;
                metrics.top = -Glyph::borderSize;
                metrics.advance = vertical ? metrics.height : metrics.width;
                rect = image->second.paddedRect;

                // If needed, allow to set scale factor for an image using
                // alias "image-scale" that could be alias for "font-scale"
                // when FormattedSection is an image section.
                sectionScale = sectionScale * util::ONE_EM / layoutTextSize;

                // Difference between one EM and an image size.
                // Aligns bottom of an image to a baseline level.
                float imageOffset = util::ONE_EM - displaySize[1] * static_cast<float>(sectionScale);
                baselineOffset = maxLineOffset + imageOffset;
                advance = verticalAdvance = static_cast<float>(metrics.advance);

                // Difference between height of an image and one EM at max line scale.
                // Pushes current line down if an image size is over 1 EM at max line scale.
                double offset = (vertical ? displaySize[0] : displaySize[1]) * sectionScale -
                                util::ONE_EM * lineMaxScale;
                if (offset > 0.0 && offset > lineOffset) {
                    lineOffset = offset;
                }
            }

            if (!vertical) {
                positionedGlyphs.emplace_back(codePoint,
                                              x + xHBOffset,
                                              y + static_cast<float>(baselineOffset) + yHBOffset,
                                              vertical,
                                              section.fontStackHash,
                                              static_cast<float>(sectionScale),
                                              rect,
                                              metrics,
                                              section.imageID,
                                              sectionIndex);
                if (advance > 0.01f) {
                    // Only thce glyph with advance should increase spacing
                    x += advance * static_cast<float>(sectionScale) + spacing;
                }
            } else {
                positionedGlyphs.emplace_back(codePoint,
                                              x,
                                              y + static_cast<float>(baselineOffset),
                                              vertical,
                                              section.fontStackHash,
                                              static_cast<float>(sectionScale),
                                              rect,
                                              metrics,
                                              section.imageID,
                                              sectionIndex);
                x += verticalAdvance * static_cast<float>(sectionScale) + spacing;
                shaping.verticalizable |= true;
            }
        }

        // Only justify if we placed at least one glyph
        if (!positionedGlyphs.empty()) {
            float lineLength = x - spacing; // Don't count trailing spacing
            maxLineLength = util::max(lineLength, maxLineLength);
            justifyLine(positionedGlyphs, justify, static_cast<float>(lineOffset));
        }

        auto currentLineHeight = static_cast<float>(lineHeight * lineMaxScale + lineOffset);
        x = 0.0f;
        y += currentLineHeight;
        positionedLine.lineOffset = static_cast<float>(std::max(lineOffset, maxLineOffset));
        maxLineHeight = std::max(currentLineHeight, maxLineHeight);
    }

    auto anchorAlign = AnchorAlignment::getAnchorAlignment(textAnchor);
    const float height = y - Shaping::yOffset;
    align(shaping,
          justify,
          anchorAlign.horizontalAlign,
          anchorAlign.verticalAlign,
          maxLineLength,
          maxLineHeight,
          lineHeight,
          height,
          lines.size());

    // Calculate the bounding box
    shaping.top += -anchorAlign.verticalAlign * height;
    shaping.bottom = shaping.top + height;
    shaping.left += -anchorAlign.horizontalAlign * maxLineLength;
    shaping.right = shaping.left + maxLineLength;
}

Shaping getShaping(const TaggedString& formattedString,
                   const float maxWidth,
                   const float lineHeight,
                   const style::SymbolAnchorType textAnchor,
                   const style::TextJustifyType textJustify,
                   const float spacing,
                   const std::array<float, 2>& translate,
                   const WritingModeType writingMode,
                   BiDi& bidi,
                   const GlyphMap& glyphMap,
                   const GlyphPositions& glyphPositions,
                   const ImagePositions& imagePositions,
                   float layoutTextSize,
                   float layoutTextSizeAtBucketZoomLevel,
                   bool allowVerticalPlacement) {
    assert(layoutTextSize);
    std::vector<TaggedString> reorderedLines;
    if (formattedString.rawText().length()) {
        if (formattedString.hbShaped() && !formattedString.logicalText().empty()) {
            // Glyph IDs are already shaped. Wrap their clusters in source order, then resolve bidi per line.
            std::vector<std::size_t> indices(formattedString.length());
            std::iota(indices.begin(), indices.end(), 0);
            std::stable_sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
                return formattedString.logicalPosition(left) < formattedString.logicalPosition(right);
            });
            const auto logical = formattedString.reordered(indices);
            auto breaks = determineLineBreaks(logical, spacing, maxWidth, glyphMap, imagePositions, layoutTextSize);
            for (std::size_t index = 0; index < logical.length(); ++index) {
                if (logical.getCodepoint(index) == 0x0a) breaks.insert(index + 1);
            }
            breaks.insert(logical.length());
            std::size_t begin = 0;
            for (const auto end : breaks) {
                if (begin == end) continue;
                const auto sourceBegin = logical.logicalPosition(begin);
                const auto sourceEnd = end == logical.length() ? logical.logicalText().size() : logical.logicalPosition(end);
                std::vector<std::size_t> visual;
                for (const auto& run : fontTextLineRuns(logical.logicalText(), sourceBegin, sourceEnd)) {
                    std::vector<std::pair<std::size_t, std::size_t>> clusters;
                    for (auto index = begin; index < end;) {
                        const auto position = logical.logicalPosition(index);
                        auto next = index + 1;
                        while (next < end && logical.logicalPosition(next) == position) ++next;
                        if (position >= run.offset && position < run.offset + run.text.size()) clusters.emplace_back(index, next);
                        index = next;
                    }
                    if (run.rtl) std::reverse(clusters.begin(), clusters.end());
                    for (const auto& cluster : clusters) {
                        for (auto index = cluster.first; index < cluster.second; ++index) visual.push_back(index);
                    }
                }
                reorderedLines.push_back(logical.reordered(visual));
                begin = end;
            }
        } else if (formattedString.hbShaped()) {
            auto breaks = determineLineBreaks(formattedString, spacing, maxWidth, glyphMap, imagePositions, layoutTextSize);
            for (std::size_t i = 0; i < formattedString.length(); ++i) {
                if (formattedString.getCodepoint(i) == 0x0a) breaks.insert(i + 1);
            }
            breaks.insert(formattedString.length());
            std::size_t begin = 0;
            for (const auto end : breaks) {
                TaggedString line;
                for (auto at = begin; at < end;) {
                    const auto sectionIndex = formattedString.getSectionIndex(at);
                    const auto& section = formattedString.getSection(at);
                    auto next = at + 1;
                    while (next < end && formattedString.getSectionIndex(next) == sectionIndex) ++next;
                    auto text = formattedString.rawText().substr(at, next - at);
                    if (section.adjusts) {
                        const auto offset = at - static_cast<std::size_t>(section.startIndex);
                        auto adjusts = std::make_shared<std::vector<HBShapeAdjust>>(section.adjusts->begin() + static_cast<std::ptrdiff_t>(offset),
                                                                                  section.adjusts->begin() + static_cast<std::ptrdiff_t>(offset + next - at));
                        line.addTextSection(text, section.scale, section.fontStack, section.type, adjusts, section.keySection, section.textColor);
                    } else if (section.imageID) {
                        line.addImageSection(*section.imageID);
                    } else {
                        line.addTextSection(text, section.scale, section.fontStack, section.type, section.keySection, section.textColor);
                    }
                    at = next;
                }
                reorderedLines.push_back(std::move(line));
                begin = end;
            }
        } else if (formattedString.sectionCount() == 1) {
            if (formattedString.getSection(0).type != GlyphIDType::FontPBF) {
                reorderedLines.emplace_back(formattedString);
            } else {
                auto untaggedLines = bidi.processText(
                    formattedString.rawText(),
                    determineLineBreaks(formattedString, spacing, maxWidth, glyphMap, imagePositions, layoutTextSize));
                for (const auto& line : untaggedLines) {
                    reorderedLines.emplace_back(line, formattedString.sectionAt(0));
                }
            }
        } else {
            StyledText subString;
            GlyphIDType sectionType = GlyphIDType::FontPBF;
            auto strLen = formattedString.getStyledText().first.length();

            std::vector<SectionOptions> formattedSections = formattedString.getSections();
            if (formattedSections.size() > 0) sectionType = formattedSections[0].type;

            std::vector<StyledText> pendStrings;

            auto processAline = [&](StyledText line) {
                reorderedLines.emplace_back(line, formattedSections);

                auto cutLens = (int32_t)line.first.length();

                for (auto& sec : formattedSections) {
                    sec.startIndex -= cutLens;
                }
            };

            auto applyLineAndPendingStrings = [&](StyledText line) {
                if (pendStrings.empty()) {
                    processAline(line);
                } else {
                    StyledText combine;
                    for (auto& pendString : pendStrings) {
                        combine.first.append(pendString.first);
                        combine.second.insert(combine.second.end(), pendString.second.begin(), pendString.second.end());
                    }
                    pendStrings.clear();
                    combine.first.append(line.first);
                    combine.second.insert(combine.second.end(), line.second.begin(), line.second.end());
                    processAline(combine);
                }
            };

            auto applySubString = [&]() {
                if (subString.first.length()) {
                    if (GlyphIDType::FontPBF == sectionType) {
                        auto processedLines = bidi.processStyledText(
                            subString,
                            determineLineBreaks({subString, formattedString.getSections()},
                                                spacing,
                                                maxWidth,
                                                glyphMap,
                                                imagePositions,
                                                layoutTextSize));

                        auto lastChar = u'x';
                        if (!subString.first.empty()) lastChar = subString.first[subString.first.length() - 1];

                        if (u'\n' == lastChar) {
                            for (const auto& line : processedLines) {
                                applyLineAndPendingStrings(line);
                            }
                        } else {
                            auto lineCount = processedLines.size();
                            if (lineCount > 1) {
                                for (size_t lineIndex = 0; lineIndex < lineCount - 1; ++lineIndex) {
                                    applyLineAndPendingStrings(processedLines[lineIndex]);
                                }
                            }
                            if (lineCount) {
                                pendStrings.push_back(processedLines[lineCount - 1]);
                            }
                        }

                    } else {
                        pendStrings.push_back(subString);
                    }
                }
            };

            for (size_t charIndex = 0; charIndex < strLen; ++charIndex) {
                auto& ch = formattedString.getStyledText().first[charIndex];
                auto& sec = formattedString.getStyledText().second[charIndex];
                auto& secType = formattedSections[sec].type;

                if (sectionType != secType) {
                    applySubString();

                    subString.first.clear();
                    subString.second.clear();

                    sectionType = secType;
                }

                subString.first += ch;
                subString.second.emplace_back(sec);
            }

            applySubString();

            if (!pendStrings.empty()) {
                StyledText combine;
                for (auto& pendString : pendStrings) {
                    combine.first.append(pendString.first);
                    combine.second.insert(combine.second.end(), pendString.second.begin(), pendString.second.end());
                }
                pendStrings.clear();
                processAline(combine);
            }
        }
    }

    Shaping shaping(translate[0], translate[1], writingMode);
    shapeLines(shaping,
               reorderedLines,
               spacing,
               lineHeight,
               textAnchor,
               textJustify,
               writingMode,
               glyphMap,
               glyphPositions,
               imagePositions,
               layoutTextSizeAtBucketZoomLevel,
               allowVerticalPlacement);

    return shaping;
}

} // namespace mbgl
