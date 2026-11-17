#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

BasementalRhodesProcessor::BasementalRhodesProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BasementalRhodes", createParameterLayout())
{
    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    using namespace ParamID;
    pModel = p (model); pBell = p (bell); pBark = p (bark); pDecay = p (decay); pRelease = p (release);
    pDynamics = p (dynamics); pMech = p (mechNoise); pTune = p (tune);
    pDrive = p (drive); pBass = p (bass); pTreble = p (treble); pCab = p (cabinet); pVolume = p (volume);
    pTremOn = p (tremOn); pTremRate = p (tremRate); pTremDepth = p (tremDepth);
    pPhOn = p (phaserOn); pPhRate = p (phaserRate); pPhDepth = p (phaserDepth); pPhFb = p (phaserFb);
    pChOn = p (chorusOn); pChRate = p (chorusRate); pChDepth = p (chorusDepth); pChMix = p (chorusMix);
    pRvOn = p (reverbOn); pRvMix = p (reverbMix); pRvDecay = p (reverbDecay);
}

bool BasementalRhodesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BasementalRhodesProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    updateEngineParams();
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void BasementalRhodesProcessor::updateEngineParams()
{
    brh::EngineParams e;
    e.suitcase   = pModel->load() > 0.5f;
    e.volumeDb   = pVolume->load();
    e.tuneA4     = pTune->load();
    e.voice.bell = pBell->load();
    e.voice.bark = pBark->load();
    e.voice.decay = pDecay->load();
    e.voice.release = pRelease->load();
    e.voice.dynamics = pDynamics->load();
    e.voice.mechNoise = pMech->load();
    e.drive      = pDrive->load();
    e.bassDb     = pBass->load();
    e.trebleDb   = pTreble->load();
    e.cabinet    = pCab->load() > 0.5f;
    e.tremOn     = pTremOn->load() > 0.5f;
    e.tremRate   = pTremRate->load();
    e.tremDepth  = pTremDepth->load();
    e.phaserOn   = pPhOn->load() > 0.5f;
    e.phaserRate = pPhRate->load();
    e.phaserDepth= pPhDepth->load();
    e.phaserFb   = pPhFb->load();
    e.chorusOn   = pChOn->load() > 0.5f;
    e.chorusRate = pChRate->load();
    e.chorusDepth= pChDepth->load();
    e.chorusMix  = pChMix->load();
    e.reverbOn   = pRvOn->load() > 0.5f;
    e.reverbMix  = pRvMix->load();
    e.reverbDecay= pRvDecay->load();
    engine.setParams (e);
}

void BasementalRhodesProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isSustainPedalOn())
        engine.setSustain (true);
    else if (m.isSustainPedalOff())
        engine.setSustain (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff();
}

void BasementalRhodesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // merge on-screen keyboard notes into the incoming MIDI
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    updateEngineParams();

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // scratch for mono hosts
    float scratch[512];

    auto renderRange = [&] (int start, int len)
    {
        if (len <= 0) return;
        if (R != nullptr)
            engine.process (L + start, R + start, len);
        else
        {
            int done = 0;
            while (done < len)
            {
                const int n = std::min (512, len - done);
                engine.process (L + start + done, scratch, n);
                for (int i = 0; i < n; ++i) L[start + done + i] = 0.5f * (L[start + done + i] + scratch[i]);
                done += n;
            }
        }
    };

    int pos = 0;
    for (const auto metadata : midi)
    {
        const int t = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (pos, t - pos);
        pos = t;
        handleMidi (metadata.getMessage());
    }
    renderRange (pos, numSamples - pos);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);
    activeVoices.store (engine.activeVoices());
}

//==============================================================================
int BasementalRhodesProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String BasementalRhodesProcessor::getProgramName (int index)
{
    const auto& ps = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? juce::String (ps[(size_t) index].name) : juce::String();
}

void BasementalRhodesProcessor::setCurrentProgram (int index)
{
    const auto& ps = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;

    // reset everything to defaults, then apply the preset's values
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : ps[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void BasementalRhodesProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BasementalRhodesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* BasementalRhodesProcessor::createEditor()
{
    return new BasementalRhodesEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasementalRhodesProcessor();
}
