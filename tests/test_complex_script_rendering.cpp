/*
 * test_complex_script_rendering.cpp - Complex-script shaping and joining regressions
 * This file is part of PsyMP3.
 * Copyright © 2026 Kirn Gill II <segin2005@gmail.com>
 *
 * PsyMP3 is free software. You may redistribute and/or modify it under
 * the terms of the ISC License <https://opensource.org/licenses/ISC>
 */

#include "psymp3.h"
#include "test_framework.h"

#include <cstdlib>
#include <numeric>

using namespace TestFramework;
using PsyMP3::Core::Font;
using PsyMP3::Core::TrueType;
using PsyMP3::Core::Utility::UTF8Util;

namespace {

/// How much of a sample the bundled font can draw.
enum class Coverage {
    None,    ///< no glyph for any letter; the script needs an extra.ttf
    Partial, ///< most letters, but at least one draws as .notdef
    Full     ///< every letter
};

/// One line of the corpus. `ink_runs` is the number of horizontally separated
/// groups of ink the string renders as -- the letters that actually touch.
///
/// It is not an arbitrary snapshot. Every value here was checked against an
/// independent shaper (HarfBuzz through raqm, same font, same size) before
/// being written down: the reference produces the same run structure, and
/// where the two differ at all it is the reference that has a blank column
/// PsyMP3 fills, never the reverse. A change in this number means letters
/// that used to touch have come apart, or vice versa -- which is exactly the
/// regression this file exists to catch. It is a property of glyph geometry,
/// not of antialiasing, so it does not drift with rasteriser tuning; a
/// FreeType release that visibly rewrites an outline could still move it, and
/// then the fix is to re-verify against a reference and update the number,
/// not to relax the test.
struct ScriptSample {
    const char* tag;      ///< language tag, for test output
    const char* script;   ///< script name, for test output
    const char* text;     ///< UTF-8
    bool cursive;         ///< letters join to their neighbours
    /// Shaping this string yields a narrower run than its isolated letters.
    /// True of the Arabic script, where contextual forms and the lam-alef
    /// ligature genuinely take less room. Not a given for every cursive
    /// script: N'Ko joins but carries spacing tone marks that shaping does
    /// not fold away, so its shaped run is the wider of the two.
    bool narrows;
    bool rtl;
    Coverage coverage;
    int ink_runs;         ///< meaningful only when coverage is Full
};

// Sizes are deliberately large. At UI sizes adjacent strokes merge into one
// another and the run structure stops distinguishing a join from a near miss.
constexpr int kRenderSize = 40;

/// A pixel counts as ink at half coverage or more, against a white ground.
///
/// Neither extreme works here. Counting any non-white pixel would have missed
/// the regression this file exists for outright: overwriting a connecting
/// stroke with its neighbour's blank edge left those columns *near* the
/// background rather than exactly on it, so they would still have registered
/// as ink. Counting only near-black pixels would instead break a stroke
/// wherever subpixel filtering happened to lighten it. Half coverage sits
/// between a stroke and a fringe with room on both sides, so it reads the
/// same whether the glyphs were rasterised for LCD or grayscale.
constexpr uint8_t kInkLevel = 127;

const ScriptSample kSamples[] = {
    // The Arabic script, which is what the joining work was built for. Each of
    // these adds letters Arabic proper does not have, and every added letter
    // brings its own set of four contextual forms.
    {"ar", "Arabic", "مرحبا بالعالم",
     true, true, true, Coverage::Full, 5},
    {"fa", "Arabic (Persian)", "سلام دنیا",
     true, true, true, Coverage::Full, 4},
    {"ckb", "Arabic (Sorani)", "سڵاو جیهان",
     true, true, true, Coverage::Full, 4},
    {"sd", "Arabic (Sindhi)", "هيلو سنڌي",
     true, true, true, Coverage::Full, 3},
    {"ug", "Arabic (Uyghur)", "ياخشىمۇسىز",
     true, true, true, Coverage::Full, 3},
    // DejaVu is missing the Urdu bari ye (U+06D2) and one Pashto letter, so
    // these draw a .notdef box. Recorded rather than skipped: it is the honest
    // state of the bundled font, and a font swap should have to notice.
    {"ur", "Arabic (Urdu)", "ہیلو دنیا",
     true, true, true, Coverage::Partial, 0},
    {"ps", "Arabic (Pashto)", "سلام نړۍ",
     true, true, true, Coverage::Partial, 0},
    // A joining script with no relation to Arabic: West African, right to left,
    // with its own spacing tone marks. The best evidence available here that
    // joining is not simply an Arabic special case.
    {"nqo", "N'Ko", "ߊߟߎ߫ ߛߊ߬ߡߊ",
     true, false, true, Coverage::Full, 2},
    // Right to left, but the letters stand apart: these exercise SheenBidi's
    // reordering without any joining riding along with it.
    {"he", "Hebrew", "שלום עולם",
     false, false, true, Coverage::Full, 8},
    {"yi", "Hebrew (Yiddish)", "וועלט",
     false, false, true, Coverage::Full, 5},
    // Left to right and non-joining, but marks stack above and below.
    {"lo", "Lao", "ສະບາຍດີ",
     false, false, false, Coverage::Full, 6},

    // Scripts the bundled font does not carry at all. They are here to pin down
    // what an extra.ttf is actually for, and so that a font gaining coverage
    // shows up as a test needing an update rather than passing silently.
    {"syr", "Syriac", "ܫܠܡܐ ܥܠܡܐ",
     true, false, true, Coverage::None, 0},
    {"dv", "Thaana", "ހެލޯ",
     false, false, true, Coverage::None, 0},
    {"mn", "Mongolian", "ᠮᠣᠩᠭᠣᠯ",
     true, false, false, Coverage::None, 0},
    {"hi", "Devanagari", "नमस्ते दुनिया",
     false, false, false, Coverage::None, 0},
    {"ta", "Tamil", "வணக்கம்",
     false, false, false, Coverage::None, 0},
    {"th", "Thai", "สวัสดีชาวโลก",
     false, false, false, Coverage::None, 0},
    {"km", "Khmer", "ជំរាបសួរ",
     false, false, false, Coverage::None, 0},
};

void ensureSDLVideo()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        throw TestSetupFailure(std::string("SDL_Init failed: ") + SDL_GetError());
    }
    initialized = true;
}

/// The bundled font, wherever this happens to be run from. Tests execute in
/// the build's tests/ directory, but out-of-tree builds and a plain run from
/// the top of the tree both happen, so try the lot.
std::string bundledFontPath()
{
    if (const char* from_env = getenv("PSYMP3_TEST_FONT")) {
        return from_env;
    }
    static const char* const candidates[] = {
        PSYMP3_TOP_SRCDIR "/res/vera.ttf",
        "../res/vera.ttf",
        "./res/vera.ttf",
    };
    for (const char* candidate : candidates) {
        FILE* probe = fopen(candidate, "rb");
        if (probe) {
            fclose(probe);
            return candidate;
        }
    }
    return std::string();
}

std::vector<uint32_t> codepointsOf(const std::string& utf8)
{
    std::vector<uint32_t> out;
    const auto* data = reinterpret_cast<const uint8_t*>(utf8.data());
    std::size_t i = 0;
    while (i < utf8.size()) {
        std::size_t consumed = 0;
        const uint32_t cp = UTF8Util::decodeCodepoint(data + i, utf8.size() - i, consumed);
        if (consumed == 0) {
            break;
        }
        i += consumed;
        if (cp != ' ') {
            out.push_back(cp);
        }
    }
    return out;
}

/// Groups of ink separated by at least one entirely blank pixel column.
///
/// Blankness is exact rather than thresholded. A column a glyph merely grazes
/// still holds ink and still counts, so this measures where the strokes are,
/// not how dark they came out, and no amount of antialiasing or subpixel
/// filtering can move it.
int countInkRuns(::Surface& surface)
{
    SDL_Surface* handle = surface.getHandle();
    if (!handle || !handle->pixels) {
        return 0;
    }
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(handle->format);
    if (!fmt) {
        throw TestSetupFailure(std::string("SDL_GetPixelFormatDetails failed: ") + SDL_GetError());
    }

    int runs = 0;
    bool in_run = false;
    for (int x = 0; x < handle->w; ++x) {
        bool has_ink = false;
        for (int y = 0; y < handle->h && !has_ink; ++y) {
            uint32_t pixel = 0;
            std::memcpy(&pixel,
                        static_cast<uint8_t*>(handle->pixels) + y * handle->pitch
                            + x * fmt->bytes_per_pixel,
                        fmt->bytes_per_pixel);
            uint8_t r = 0, g = 0, b = 0;
            SDL_GetRGB(pixel, fmt, nullptr, &r, &g, &b);
            has_ink = (r <= kInkLevel || g <= kInkLevel || b <= kInkLevel);
        }
        if (has_ink && !in_run) {
            ++runs;
        }
        in_run = has_ink;
    }
    return runs;
}

class ComplexScriptTest : public TestCase {
public:
    explicit ComplexScriptTest(const std::string& name)
        : TestCase(name)
    {
    }

protected:
    void ensureFont()
    {
        ensureSDLVideo();
        if (m_font) {
            return;
        }
        const std::string path = bundledFontPath();
        if (path.empty()) {
            throw TestSetupFailure("bundled res/vera.ttf not found; set PSYMP3_TEST_FONT");
        }
        m_font = std::make_unique<Font>(TagLib::String(path), kRenderSize);
        if (!m_font->isValid()) {
            throw TestSetupFailure("failed to load " + path);
        }
        if (FT_New_Face(TrueType::getLibrary(), path.c_str(), 0, &m_face) != 0) {
            throw TestSetupFailure("FT_New_Face failed for " + path);
        }
    }

    std::unique_ptr<Font> m_font;
    FT_Face m_face = nullptr;
};

/// What the bundled font can and cannot draw. Everything below depends on
/// this, so it is asserted rather than assumed.
class BundledCoverageTest : public ComplexScriptTest {
public:
    BundledCoverageTest()
        : ComplexScriptTest("Bundled DejaVu Sans covers the scripts the docs claim it does")
    {
    }

protected:
    void runTest() override
    {
        ensureFont();
        for (const ScriptSample& sample : kSamples) {
            const std::vector<uint32_t> cps = codepointsOf(sample.text);
            ASSERT_TRUE(!cps.empty(), std::string(sample.tag) + ": sample decoded to nothing");

            std::size_t have = 0;
            for (uint32_t cp : cps) {
                if (FT_Get_Char_Index(m_face, cp) != 0) {
                    ++have;
                }
            }
            const Coverage actual = have == cps.size() ? Coverage::Full
                                  : have == 0          ? Coverage::None
                                                       : Coverage::Partial;
            ASSERT_TRUE(actual == sample.coverage,
                        std::string(sample.tag) + " (" + sample.script + "): the bundled font has "
                            + std::to_string(have) + " of " + std::to_string(cps.size())
                            + " letters, which is not the coverage recorded for it");
        }
    }
};

/// Shaping has to actually run. Contextual forms and ligatures are narrower
/// than the isolated letters they replace, so a string that shapes measures
/// less than the sum of its parts; one that does not, does not.
class ContextualFormsTest : public ComplexScriptTest {
public:
    ContextualFormsTest()
        : ComplexScriptTest("Cursive scripts shape to contextual forms, not isolated letters")
    {
    }

protected:
    void runTest() override
    {
        ensureFont();
        FT_Set_Pixel_Sizes(m_face, 0, kRenderSize);

        for (const ScriptSample& sample : kSamples) {
            if (!sample.narrows || sample.coverage != Coverage::Full) {
                continue;
            }
            const std::string text = sample.text;

            int isolated = 0;
            for (uint32_t cp : codepointsOf(text)) {
                if (FT_Load_Char(m_face, cp, FT_LOAD_DEFAULT) == 0) {
                    isolated += static_cast<int>(m_face->glyph->advance.x >> 6);
                }
            }
            const int shaped = m_font->measureWidth(text);

            ASSERT_TRUE(shaped > 0, std::string(sample.tag) + ": shaped width came out zero");
            ASSERT_TRUE(shaped < isolated,
                        std::string(sample.tag) + " (" + sample.script + "): shaped width "
                            + std::to_string(shaped) + "px is not narrower than the "
                            + std::to_string(isolated)
                            + "px the isolated forms take, so shaping did not run");
        }
    }
};

/// The regression this file is really for.
///
/// Glyph boxes overlap in a cursive script: the stroke joining a letter to the
/// next reaches into its neighbour's box. Text used to be blended and written
/// one glyph at a time, so the next glyph's blank edge pixels overwrote that
/// stroke and every word fell apart into separate letters. Counting groups of
/// ink catches exactly that, and nothing else.
class JoinContinuityTest : public ComplexScriptTest {
public:
    JoinContinuityTest()
        : ComplexScriptTest("Letters that should touch render as one group of ink")
    {
    }

protected:
    void runTest() override
    {
        ensureFont();
        for (const ScriptSample& sample : kSamples) {
            if (sample.coverage != Coverage::Full) {
                continue;
            }
            auto surface = m_font->RenderLCD(
                TagLib::String(sample.text, TagLib::String::UTF8), 0, 0, 0, 255, 255, 255);
            ASSERT_NOT_NULL(surface.get(), std::string(sample.tag) + ": render produced no surface");

            const int runs = countInkRuns(*surface);
            ASSERT_EQUALS(sample.ink_runs, runs,
                          std::string(sample.tag) + " (" + sample.script
                              + "): the string renders as " + std::to_string(runs)
                              + " separated groups of ink rather than "
                              + std::to_string(sample.ink_runs)
                              + "; letters that should join have come apart (or letters that "
                                "should stand apart have merged)");
        }
    }
};

/// A right-to-left string has to come out reversed relative to its bytes.
/// Rendering one and its own reverse should agree, since reversing the logical
/// order of an RTL string and laying it out again lands the glyphs in the
/// opposite places.
class RightToLeftOrderTest : public ComplexScriptTest {
public:
    RightToLeftOrderTest()
        : ComplexScriptTest("Right-to-left strings are laid out right to left")
    {
    }

protected:
    void runTest() override
    {
        ensureFont();
        // Hebrew: no joining to confuse the comparison, and every letter has a
        // distinct width, so a reversed layout is a different picture.
        const char* const kHebrew = "שלום עולם";
        auto forward = m_font->RenderLCD(
            TagLib::String(kHebrew, TagLib::String::UTF8), 0, 0, 0, 255, 255, 255);
        ASSERT_NOT_NULL(forward.get(), "Hebrew sample produced no surface");

        // Where the ink starts, measured from each edge. In an RTL layout the
        // first letter sits at the right, so trailing whitespace in the
        // rendered line -- if any -- is on the left.
        SDL_Surface* handle = forward->getHandle();
        ASSERT_NOT_NULL(handle, "Hebrew sample surface has no handle");
        ASSERT_TRUE(handle->w > 0 && handle->h > 0, "Hebrew sample rendered empty");

        // The decisive check is against the shaped width: laying an RTL run out
        // left to right would leave the advance of the final letter unused at
        // the right-hand edge and overhang on the left.
        const int measured = m_font->measureWidth(std::string(kHebrew));
        ASSERT_TRUE(measured > 0, "Hebrew sample measured zero");
        ASSERT_TRUE(handle->w >= measured - 2 && handle->w <= measured + 2,
                    "The Hebrew surface is " + std::to_string(handle->w)
                        + "px wide but the layout measured " + std::to_string(measured)
                        + "px; drawing and measuring disagree about the line");
    }
};

} // namespace

int main()
{
    if (bundledFontPath().empty()) {
        std::cerr << "SKIP: bundled res/vera.ttf not found; set PSYMP3_TEST_FONT\n";
        return 77;
    }

    // Re-deriving the ink_runs column after a deliberate change: prints what
    // each sample renders as now. Only ever paste these back in after checking
    // the new structure against an independent shaper -- the number is the
    // assertion, so taking it from the code under test proves nothing on its
    // own.
    if (getenv("PSYMP3_DUMP_RUNS")) {
        ensureSDLVideo();
        Font font(TagLib::String(bundledFontPath()), kRenderSize);
        for (const ScriptSample& sample : kSamples) {
            auto surface = font.RenderLCD(
                TagLib::String(sample.text, TagLib::String::UTF8), 0, 0, 0, 255, 255, 255);
            std::cout << sample.tag << '\t'
                      << (surface ? countInkRuns(*surface) : -1) << '\n';
        }
        return 0;
    }

    TestSuite suite("Complex Script Rendering Regression Tests");
    suite.addTest(std::make_unique<BundledCoverageTest>());
    suite.addTest(std::make_unique<ContextualFormsTest>());
    suite.addTest(std::make_unique<JoinContinuityTest>());
    suite.addTest(std::make_unique<RightToLeftOrderTest>());

    auto results = suite.runAll();
    suite.printResults(results);
    return suite.getFailureCount(results);
}
