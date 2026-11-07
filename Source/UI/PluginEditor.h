#pragma once
#include "../PluginProcessor.h"
#include "Widgets.h"
#include "LcdDisplay.h"
#include "Pages.h"

namespace PV
{
/** Largest 16:10 editor width (<= wanted) that fits the primary display's usable area, so the window never opens off-screen. */
int fitEditorWidth (int wanted);

class PVKeyboard : public juce::MidiKeyboardComponent
{
public:
    using juce::MidiKeyboardComponent::MidiKeyboardComponent;
protected:
    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool down, bool over, juce::Colour line, juce::Colour text) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float> area, bool down, bool over, juce::Colour noteFill) override;
};

class WheelSlider : public juce::Slider
{
public:
    explicit WheelSlider (bool spring) : springBack (spring) { setSliderStyle (juce::Slider::LinearVertical); setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0); setMouseDragSensitivity (120); }
    void mouseUp (const juce::MouseEvent& e) override { juce::Slider::mouseUp (e); if (springBack) setValue (0.0, juce::sendNotificationSync); }
    void mouseDoubleClick (const juce::MouseEvent&) override { setValue (springBack ? 0.0 : 0.0, juce::sendNotificationSync); }
private:
    bool springBack;
};

class LayerList : public juce::Component
{
public:
    LayerList (PlanetVelProcessor&, std::function<void (int)> pick);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setSelected (int l) { sel = l; repaint(); }
private:
    int rowAt (int y) const { return juce::jlimit (0, kLayers - 1, y * kLayers / juce::jmax (1, getHeight())); }
    PlanetVelProcessor& proc; std::function<void (int)> pick; int sel = 0, dragRow = -1;
};

class PlanetVelEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget,
                        private juce::Timer, private juce::ChangeListener
{
public:
    explicit PlanetVelEditor (PlanetVelProcessor&);
    ~PlanetVelEditor() override;
    void resized() override;
    void parentHierarchyChanged() override;
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    static constexpr int kBaseW = 1600, kBaseH = 1000;
    void setPage (int page);                       // also used by the test harness
    void selectLayer (int layer);
private:
    class Content;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void setScalePercent (int pct);
    void applyTheme();
    void buildProgramPage();

    PlanetVelProcessor& proc;
    PVLookAndFeel laf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<Content> content;
    juce::TooltipWindow tooltip { this, 500 };
    int page = 0, selectedLayer = 0, frame = 0;
    bool restoring = false;
    friend class Content;
};
} // namespace PV
