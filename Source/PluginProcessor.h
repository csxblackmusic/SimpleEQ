/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48,
};

struct ChainSettings // a data structure to store all the parameters from the AudioProcessor
{
    float peakFreq{ 0 }, peakGainInDecibels{ 0 }, peakQuality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    int lowCutSlope{Slope::Slope_12}, highCutSlope{ Slope::Slope_12 };
};
ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts); //Will return a struct with the values of our params


//==============================================================================
/**
*/
class SimpleEQAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    SimpleEQAudioProcessor();
    ~SimpleEQAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts{*this,nullptr,"Parameters",createParameterLayout()};

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private: //dsp will only process a single channel of audio at once
    using Filter = juce::dsp::IIR::Filter<float>; //type alias
    using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>; //each filter is 12db/oct by default - this chain gives us 4x12 =4 8db/oct
    using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;//processor chains process all the audio its passed in a context with whats in your chain
    MonoChain leftChain, rightChain; //since dsp is mono by default and were writing a stereo plugin
    enum ChainPosition
    {
        LowCut,
        Peak,
        HighCut
    };
    //==============================================================================t
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleEQAudioProcessor)
};
