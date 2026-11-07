// Secondary editor pages: LAYERS, COMBI (layer mix + mod matrix), FX, ARP, GLOBAL (settings).
#pragma once
#include "Widgets.h"

namespace PV
{
/** Base: pages that contain controls bound to the currently selected layer. */
class LayerBoundPage : public juce::Component
{
public:
    virtual void setLayer (int layer) = 0;
};

class LayersPage : public LayerBoundPage, private juce::Timer
{
public:
    explicit LayersPage (PlanetVelProcessor&);
    ~LayersPage() override { stopTimer(); }
    void setLayer (int layer) override;
    std::function<void (int)> onLayerPicked;
private:
    void timerCallback() override;
    void refreshEngineUi();
    PlanetVelProcessor& proc; int layer = 0, shownEngine = -1;
    std::vector<std::unique_ptr<PanelBg>> panels;
    std::vector<std::pair<KnobControl*, int>> layerKnobs;                // knob, LayerParam offset
    std::vector<std::unique_ptr<KnobControl>> knobs;
    std::array<std::unique_ptr<juce::Component>, 4> tabs;
    SettingPills* layerTabs = nullptr;
    std::unique_ptr<SettingPills> layerTabsOwner;
    std::unique_ptr<ChoicePills> enginePills, typePills, ftypePills, lfo1Shape, lfo2Shape;
    std::unique_ptr<ToggleSwitch> mute, solo;
    std::array<KnobControl*, 6> pKnobs {};
    std::unique_ptr<EnvView> env;
    KnobControl* lfo1 = nullptr; KnobControl* lfo2 = nullptr;
};

class ModMatrixView : public juce::Component
{
public:
    explicit ModMatrixView (PlanetVelProcessor&);
    void resized() override;
    void refresh();
private:
    struct Row : juce::Component
    {
        Row (PlanetVelProcessor&, int slot);
        void resized() override;
        void refresh();
        PlanetVelProcessor& proc; int slot; PVButton src { "-" }, dest { "-" }; juce::Slider amount; juce::Label num;
    };
    PlanetVelProcessor& proc; juce::Viewport vp; juce::Component holder; std::vector<std::unique_ptr<Row>> rows;
};

class CombiPage : public juce::Component
{
public:
    explicit CombiPage (PlanetVelProcessor&);
    void resized() override;
    void refresh() { matrix.refresh(); labels.repaint(); }
private:
    struct Labels : juce::Component                        // letters / engine names drawn above the panel background
    {
        explicit Labels (PlanetVelProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        PlanetVelProcessor& proc;
    };
    PlanetVelProcessor& proc; Labels labels { proc }; PanelBg mixBg { "LAYER MIX" }, modBg { u8 ("MOD MATRIX  \xc2\xb7  32 SLOTS") };
    std::vector<std::unique_ptr<KnobControl>> knobs; std::vector<std::unique_ptr<ToggleSwitch>> toggles; ModMatrixView matrix;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (PlanetVelProcessor&);
    void resized() override;
private:
    PlanetVelProcessor& proc;
    std::vector<std::unique_ptr<PanelBg>> panels; std::vector<std::unique_ptr<KnobControl>> knobs; std::vector<std::unique_ptr<ChoicePills>> pills; std::vector<std::unique_ptr<ToggleSwitch>> toggles;
    KnobControl* dlyTime = nullptr;
};

class ArpStepEditor : public juce::Component
{
public:
    explicit ArpStepEditor (PlanetVelProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void setLane (int l) { lane = l; repaint(); }
private:
    void edit (const juce::MouseEvent&, bool first);
    PlanetVelProcessor& proc; int lane = 0, lastStep = -1;
};

class ArpPage : public juce::Component
{
public:
    explicit ArpPage (PlanetVelProcessor&);
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    juce::File writeMidiFile() const;
    PlanetVelProcessor& proc; bool dragging = false;
    PanelBg ctrlBg { "ARPEGGIATOR" }, seqBg { u8 ("STEP PATTERN  \xc2\xb7  32 STEPS") };
    std::unique_ptr<ToggleSwitch> on; std::unique_ptr<ChoicePills> pattern, octaves; std::unique_ptr<KnobControl> rate, gate;
    std::unique_ptr<SettingPills> length, lane; ArpStepEditor editor; PVButton dragMidi { "DRAG MIDI  >  PIANO ROLL" }, randomize { "RANDOMIZE" };
    juce::TemporaryFile* dummy = nullptr; std::vector<std::unique_ptr<KnobControl>> knobsKeep; std::vector<std::unique_ptr<juce::Label>> keep;
};

class GlobalPage : public juce::Component
{
public:
    GlobalPage (PlanetVelProcessor&, std::function<void (int)> setScale, std::function<void()> themeChanged);
    void resized() override;
    void sync();
private:
    struct Info : juce::Component                          // text drawn above the panel backgrounds
    {
        explicit Info (PlanetVelProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        PlanetVelProcessor& proc;
    };
    PlanetVelProcessor& proc; std::function<void (int)> setScale; std::function<void()> themeChanged; Info info { proc };
    PanelBg engineBg { "ENGINE" }, viewBg { "INTERFACE" }, libBg { "LIBRARY & TUNING" }, aboutBg { "ABOUT", false };
    std::unique_ptr<SettingPills> quality, poly, pbRange, themePills, scalePills, animation, mpe;
    PVButton lib { "SET LIBRARY FOLDER..." }, sample { "LOAD GRANULAR SAMPLE..." }, tuning { "LOAD TUNING (.scl)..." }, resetTuning { "RESET TUNING" };
    std::unique_ptr<juce::FileChooser> chooser; std::vector<std::unique_ptr<juce::Label>> labels;
};
} // namespace PV
