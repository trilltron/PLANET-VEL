// Central Triton-style LCD: animated planet/radar orb, program header, sub-tabs and the preset browser
// (categories, search, engine filter, favourites, Explore, prev/next, A/B, save).
#pragma once
#include "Widgets.h"

namespace PV
{
class LcdDisplay : public juce::Component, private juce::ChangeListener, private juce::TextEditor::Listener
{
public:
    explicit LcdDisplay (PlanetVelProcessor&);
    ~LcdDisplay() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void tick (bool animate);                                   // ~30 fps
    std::function<void (int)> onSubTab;
    std::function<void()> onSearchDone;                         // search box finished (Enter / Esc): give the typing keys back to the piano                         // asks the editor to switch top level page

    void step (int delta);
    void explore();
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { ensureCurrentVisible(); repaint(); }
    void ensureCurrentVisible();
    void textEditorTextChanged (juce::TextEditor&) override { rebuildFilter(); }
    void rebuildFilter();
    void ensureUserPresets();
    void load (int visibleRow);
    void drawOrb (juce::Graphics&, juce::Rectangle<float>);
    juce::Rectangle<int> screen() const { return { getWidth() * 7 / 100, getHeight() * 24 / 100, getWidth() * 86 / 100, getHeight() * 65 / 100 }; }
    struct Layout { juce::Rectangle<int> header, tabs, cats, search, chips, list, footer; };
    Layout layout() const;

    PlanetVelProcessor& proc;
    juce::TextEditor searchBox;
    std::vector<const Preset*> all; std::vector<int> factoryIndex;      // parallel to `all` (-1 = user)
    std::vector<int> visible;                                           // indices into `all`
    std::vector<Preset> userPresets; bool userLoaded = false;
    juce::StringArray cats;
    int catSel = 0, engineMask = 0, catScroll = 0, listScroll = 0, subTab = 5;
    float t = 0.0f; int sigPhase = 0;
    juce::Rectangle<int> btnPrev, btnNext, btnExplore, btnAB, btnSave;
    std::vector<juce::Rectangle<int>> tabRects, chipRects, catRects;
    struct RowHit { juce::Rectangle<int> row, star; int vis; }; std::vector<RowHit> rowHits;
};
} // namespace PV
