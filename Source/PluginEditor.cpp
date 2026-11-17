#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

static const std::map<String, String>& tooltipsText()
{
    static const std::map<String, String> t = {
        { ParamID::bell,      "Hammer hardness: how much of the tine's bell-like overtone you hear on the attack." },
        { ParamID::bark,      "Pickup voicing. Higher = tine sits further off the pickup centre: more growl and bark when you dig in." },
        { ParamID::decay,     "How long notes ring while the key is held." },
        { ParamID::release,   "Damper felt softness: short = tight staccato, long = notes ring on after release." },
        { ParamID::dynamics,  "Velocity sensitivity. Lower values even out your playing." },
        { ParamID::mechNoise, "Key thump and damper noise." },
        { ParamID::tune,      "Master tuning (A4)." },
        { ParamID::drive,     "Amp overdrive. Rhodes + cranked amp = classic bite." },
        { ParamID::bass,      "Amp bass (low shelf around 160 Hz)." },
        { ParamID::treble,    "Amp treble (high shelf around 2.8 kHz)." },
        { ParamID::volume,    "Output level." },
        { ParamID::tremRate,  "Vibrato speed. Suitcase: stereo auto-pan between speakers. Stage: amp tremolo." },
        { ParamID::tremDepth, "Vibrato intensity." },
        { ParamID::phaserRate,"Phaser speed." },
        { ParamID::phaserDepth,"Phaser sweep width." },
        { ParamID::phaserFb,  "Phaser resonance." },
        { ParamID::chorusRate,"Chorus speed." },
        { ParamID::chorusDepth,"Chorus modulation depth." },
        { ParamID::chorusMix, "Chorus wet amount." },
        { ParamID::reverbMix, "Spring reverb amount." },
        { ParamID::reverbDecay,"Spring reverb length." },
    };
    return t;
}

BasementalRhodesEditor::BasementalRhodesEditor (BasementalRhodesProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    using namespace ParamID;
    // PIANO
    addKnob (bell, "BELL"); addKnob (bark, "BARK"); addKnob (decay, "DECAY"); addKnob (release, "RELEASE");
    addKnob (dynamics, "DYNAMICS"); addKnob (mechNoise, "MECH"); addKnob (tune, "TUNE");
    // AMP
    addKnob (drive, "DRIVE"); addKnob (bass, "BASS"); addKnob (treble, "TREBLE"); addKnob (volume, "VOLUME");
    addSwitch (cabinet, "CAB");
    // FX
    addSwitch (tremOn, "ON");   addKnob (tremRate, "RATE");   addKnob (tremDepth, "DEPTH");
    addSwitch (phaserOn, "ON"); addKnob (phaserRate, "RATE"); addKnob (phaserDepth, "DEPTH"); addKnob (phaserFb, "FEEDBACK");
    addSwitch (chorusOn, "ON"); addKnob (chorusRate, "RATE"); addKnob (chorusDepth, "DEPTH"); addKnob (chorusMix, "MIX");
    addSwitch (reverbOn, "ON"); addKnob (reverbMix, "MIX");   addKnob (reverbDecay, "DECAY");

    // model selector
    for (auto* b : { &stageButton, &suitcaseButton })
    {
        b->setClickingTogglesState (false);
        content.addAndMakeVisible (*b);
    }
    stageButton.onClick = [this] { setModel (0); };
    suitcaseButton.onClick = [this] { setModel (1); };
    stageButton.setTooltip ("Stage: the classic dry piano, as if plugged into a Twin.");
    suitcaseButton.setTooltip ("Suitcase: built-in Peterson preamp, 4x12 speaker cabinet and stereo vibrato (auto-pan).");

    // presets
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0) proc.setCurrentProgram (idx);
    };
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);
    content.addAndMakeVisible (presetBox);

    // keyboard: 73-key range E1..E7
    keyboard.setAvailableRange (28, 100);
    keyboard.setLowestVisibleKey (28);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Colours_::accent.withAlpha (0.55f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours_::accent.withAlpha (0.2f));
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xfff4f1ea));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff141312));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff8a857c));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.4f));
    keyboard.setWantsKeyboardFocus (false);
    content.addAndMakeVisible (keyboard);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setResizeLimits (kBaseW * 6 / 10, kBaseH * 6 / 10, kBaseW * 2, kBaseH * 2);
    setSize (kBaseW, kBaseH);

    layoutContent();
    startTimerHz (20);
    timerCallback();
}

BasementalRhodesEditor::~BasementalRhodesEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

BasementalRhodesEditor::Knob& BasementalRhodesEditor::addKnob (const char* paramId, const String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    k->slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k->slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    k->slider.setColour (Slider::textBoxTextColourId, Colours_::cream);
    auto it = tooltipsText().find (paramId);
    if (it != tooltipsText().end()) k->slider.setTooltip (it->second);
    k->label.setText (name, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (FontOptions (11.5f, Font::bold));
    k->label.setColour (Label::textColourId, Colours_::textDim);
    content.addAndMakeVisible (k->slider);
    content.addAndMakeVisible (k->label);
    k->att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    auto& ref = *k;
    knobById[paramId] = k.get();
    knobs.push_back (std::move (k));
    return ref;
}

BasementalRhodesEditor::Switch& BasementalRhodesEditor::addSwitch (const char* paramId, const String& name)
{
    auto s = std::make_unique<Switch>();
    s->button.setButtonText (name);
    content.addAndMakeVisible (s->button);
    s->att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, s->button);
    auto& ref = *s;
    switchById[paramId] = s.get();
    switches.push_back (std::move (s));
    return ref;
}

void BasementalRhodesEditor::setModel (int index)
{
    if (auto* p = proc.apvts.getParameter (ParamID::model))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void BasementalRhodesEditor::timerCallback()
{
    const bool suitcase = proc.apvts.getRawParameterValue (ParamID::model)->load() > 0.5f;
    stageButton.setToggleState (! suitcase, dontSendNotification);
    suitcaseButton.setToggleState (suitcase, dontSendNotification);

    if (proc.getCurrentProgram() != presetBox.getSelectedItemIndex())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);

    // dim effect knobs when their section is off
    auto dim = [this] (const char* sw, std::initializer_list<const char*> ids)
    {
        const bool on = proc.apvts.getRawParameterValue (sw)->load() > 0.5f;
        for (auto* id : ids) if (auto it = knobById.find (id); it != knobById.end()) it->second->slider.setEnabled (on);
    };
    using namespace ParamID;
    dim (tremOn, { tremRate, tremDepth });
    dim (phaserOn, { phaserRate, phaserDepth, phaserFb });
    dim (chorusOn, { chorusRate, chorusDepth, chorusMix });
    dim (reverbOn, { reverbMix, reverbDecay });

    const int vc = proc.getActiveVoiceCount();
    if (vc != voiceCount) { voiceCount = vc; content.repaint (0, 0, kBaseW, 80); }
}

void BasementalRhodesEditor::paint (Graphics& g)
{
    g.fillAll (Colours_::tolex);
}

void BasementalRhodesEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (AffineTransform::scale (scale));
}

void BasementalRhodesEditor::layoutContent()
{
    using namespace ParamID;
    sections.clear();

    // header
    presetBox.setBounds (kBaseW - 20 - 250, 22, 250, 32);
    stageButton.setBounds (kBaseW - 20 - 250 - 24 - 220, 22, 110, 32);
    suitcaseButton.setBounds (kBaseW - 20 - 250 - 24 - 110, 22, 110, 32);
    stageButton.setConnectedEdges (Button::ConnectedOnRight);
    suitcaseButton.setConnectedEdges (Button::ConnectedOnLeft);

    auto placeKnobs = [this] (Rectangle<int> area, std::initializer_list<const char*> ids, int labelH)
    {
        const int n = (int) ids.size();
        const int w = area.getWidth() / n;
        int i = 0;
        for (auto* id : ids)
        {
            auto cell = Rectangle<int> (area.getX() + i * w, area.getY(), w, area.getHeight());
            auto* k = knobById[id];
            k->label.setBounds (cell.removeFromTop (labelH));
            k->slider.setBounds (cell.reduced (2, 0));
            ++i;
        }
    };
    auto addSection = [this] (const String& title, Rectangle<int> r) { sections.push_back ({ title, r }); return r; };
    auto inner = [] (Rectangle<int> r) { return r.withTrimmedTop (30).reduced (8, 6); };
    auto placeSwitch = [this] (const char* id, Rectangle<int> sec)
    {
        switchById[id]->button.setBounds (sec.getRight() - 74, sec.getY() + 5, 66, 20);
    };

    // row 1
    const int r1y = 104, r1h = 200;
    auto piano = addSection ("PIANO", { 20, r1y, 640, r1h });
    placeKnobs (inner (piano), { bell, bark, decay, release, dynamics, mechNoise, tune }, 18);
    auto ampS = addSection ("AMP", { 672, r1y, 348, r1h });
    placeKnobs (inner (ampS), { drive, bass, treble, volume }, 18);
    placeSwitch (cabinet, ampS);

    // row 2
    const int r2y = 316, r2h = 160;
    auto vib = addSection ("VIBRATO", { 20, r2y, 222, r2h });
    placeKnobs (inner (vib), { tremRate, tremDepth }, 16);
    placeSwitch (tremOn, vib);
    auto ph = addSection ("PHASER", { 254, r2y, 282, r2h });
    placeKnobs (inner (ph), { phaserRate, phaserDepth, phaserFb }, 16);
    placeSwitch (phaserOn, ph);
    auto ch = addSection ("CHORUS", { 548, r2y, 282, r2h });
    placeKnobs (inner (ch), { chorusRate, chorusDepth, chorusMix }, 16);
    placeSwitch (chorusOn, ch);
    auto rv = addSection ("SPRING", { 842, r2y, 178, r2h });
    placeKnobs (inner (rv), { reverbMix, reverbDecay }, 16);
    placeSwitch (reverbOn, rv);

    // keys
    keyboard.setBounds (20, 498, kBaseW - 40, 124);
    keyboard.setKeyWidth ((float) (kBaseW - 40) / 43.0f); // 43 white keys in E1..E7
}

void BasementalRhodesEditor::paintContent (Graphics& g)
{
    const auto W = (float) kBaseW;

    // tolex background with subtle texture
    g.fillAll (Colours_::tolex);
    Random rng (7);
    g.setColour (Colours::white.withAlpha (0.018f));
    for (int i = 0; i < 2600; ++i)
        g.fillRect ((float) rng.nextInt (kBaseW), (float) rng.nextInt (kBaseH), 1.0f, 1.0f);

    // header
    g.setColour (Colours_::accent);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("B A S E M E N T A L", 24, 16, 300, 16, Justification::centredLeft);
    g.setColour (Colours_::cream);
    g.setFont (FontOptions (30.0f, Font::bold));
    g.drawText (JucePlugin_Name, 22, 32, 420, 36, Justification::centredLeft);
    g.setColour (Colours_::textDim);
    g.setFont (FontOptions (12.0f));
    g.drawText ("Electro-mechanical tine piano  |  voices: " + String (voiceCount), 300, 44, 260, 18, Justification::centredLeft);

    // aluminium name rail with red felt strip (like the key slip on a '73)
    Rectangle<float> rail (0, 80, W, 12);
    g.setGradientFill (ColourGradient (Colour (0xffe2e0db), 0, rail.getY(), Colour (0xff8e8b85), 0, rail.getBottom(), false));
    g.fillRect (rail);
    g.setColour (Colours_::feltRed);
    g.fillRect (0.0f, rail.getBottom(), W, 4.0f);

    // sections
    for (auto& s : sections)
    {
        auto r = s.area.toFloat();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 2), 8.0f);
        g.setColour (Colours_::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours_::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours_::cream);
        g.setFont (FontOptions (13.0f, Font::bold));
        g.drawText (s.title, r.getX() + 12, r.getY() + 6, 200, 18, Justification::centredLeft);
        g.setColour (Colours_::accent.withAlpha (0.8f));
        g.fillRect (r.getX() + 12, r.getY() + 26, 26.0f, 2.0f);
    }

    // keyboard frame
    g.setColour (Colour (0xff0e0d0c));
    g.fillRoundedRectangle (14.0f, 490.0f, W - 28.0f, 138.0f, 6.0f);
    g.setColour (Colours_::feltRed);
    g.fillRect (20.0f, 494.0f, W - 40.0f, 4.0f);
}
