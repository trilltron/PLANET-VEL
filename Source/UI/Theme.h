// Colour themes: "Crimson" (default red-violet alien dusk), "Violet Night", "Eclipse".
#pragma once
#include "../Common.h"

namespace PV
{
struct Theme
{
    juce::Colour bg0, bg1, panel, border, accent, accent2, hot, led, lcd, lcdText, text, dim;
};

inline const Theme& themeFor (int i)
{
    static const Theme crimson { juce::Colour (0xff120812), juce::Colour (0xff1e0b1f), juce::Colour (0xff2a0f2c), juce::Colour (0xff5a2160), juce::Colour (0xffff2e63), juce::Colour (0xff9b4dff),
                                 juce::Colour (0xffff5fa2), juce::Colour (0xffffb347), juce::Colour (0xff1a0610), juce::Colour (0xffff6b8a), juce::Colour (0xffe9d6ea), juce::Colour (0xff9a7aa0) };
    static const Theme violet  { juce::Colour (0xff0b0818), juce::Colour (0xff150b28), juce::Colour (0xff1c1236), juce::Colour (0xff3f2a7a), juce::Colour (0xff9b6bff), juce::Colour (0xff4dd8ff),
                                 juce::Colour (0xffc9a0ff), juce::Colour (0xffffb347), juce::Colour (0xff0a0618), juce::Colour (0xffb9a2ff), juce::Colour (0xffdcd6ee), juce::Colour (0xff8a82ac) };
    static const Theme eclipse { juce::Colour (0xff0a0908), juce::Colour (0xff161110), juce::Colour (0xff1c1715), juce::Colour (0xff4a3a30), juce::Colour (0xffffb347), juce::Colour (0xffff5a36),
                                 juce::Colour (0xffffd08a), juce::Colour (0xffffb347), juce::Colour (0xff0c0806), juce::Colour (0xffffb86b), juce::Colour (0xffeadfd2), juce::Colour (0xff9c8c7c) };
    return i == 1 ? violet : (i == 2 ? eclipse : crimson);
}
inline int& themeIndexRef() { static int idx = 0; return idx; }
inline const Theme& theme() { return themeFor (themeIndexRef()); }
inline void setThemeIndex (int i) { themeIndexRef() = juce::jlimit (0, 2, i); }
inline const char* themeName (int i) { return i == 1 ? "Violet Night" : i == 2 ? "Eclipse" : "Crimson"; }

inline juce::Font monoFont (float h, bool bold = false)
{ return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain)); }
inline juce::Font sansFont (float h, bool bold = true)
{ return juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(), h, bold ? juce::Font::bold : juce::Font::plain)); }

/** Draws spaced-out uppercase text (hardware-label look). */
inline void drawSpaced (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, const juce::Font& f, float tracking = 0.0f)
{
    g.setFont (f.withExtraKerningFactor (tracking));
    g.drawText (t, r, j, false);
}
} // namespace PV
