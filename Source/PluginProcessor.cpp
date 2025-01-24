/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SimpleEQAudioProcessor::SimpleEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

SimpleEQAudioProcessor::~SimpleEQAudioProcessor()
{
}

//==============================================================================
const juce::String SimpleEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SimpleEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SimpleEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SimpleEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SimpleEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int SimpleEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SimpleEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SimpleEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void SimpleEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void SimpleEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::dsp::ProcessSpec spec; //prepares the filters before using them - spec gets passed to each link in the chain
    spec.maximumBlockSize = samplesPerBlock; //max # of amples to process at once
    spec.numChannels = 1; //for mono
    spec.sampleRate = sampleRate;
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    auto chainSettings = getChainSettings(apvts); //makes the filter with the values from our interface
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 
                                                                                chainSettings.peakFreq, 
                                                                                chainSettings.peakQuality, 
                                                                                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    *leftChain.get<ChainPosition::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPosition::Peak>().coefficients = *peakCoefficients;
}//updating 

void SimpleEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SimpleEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SimpleEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    auto chainSettings = getChainSettings(apvts); //makes the filter with the values from our interface
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),// sets up peak/bandpass filter
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels));
    *leftChain.get<ChainPosition::Peak>().coefficients = *peakCoefficients;
    *rightChain.get<ChainPosition::Peak>().coefficients = *peakCoefficients;

    //adding coefficients for the cut filters in the left and right channels
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, getSampleRate(), (chainSettings.lowCutSlope + 1) * 2);//slope(db/oct) of cutfilters known as its order - this helper function creates these filters
    auto& leftLowCut = leftChain.get<ChainPosition::LowCut>();
    leftLowCut.setBypassed<0>(true); //bypass all four of the filters in the LowCutChain 
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
        case Slope_12:
        {
            *leftLowCut.get<0>().coefficients = *cutCoefficients[0]; //set the filter coefficient
            leftLowCut.setBypassed<0>(false); // turn the filter back on
            break;
        }
        case Slope_24:
        {
            *leftLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            leftLowCut.setBypassed<0>(false); //turns the filter back on
            *leftLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            leftLowCut.setBypassed<1>(false);
            break;
        }
        case Slope_36:
        {
            *leftLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            leftLowCut.setBypassed<0>(false); //turns the filter back on
            *leftLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            leftLowCut.setBypassed<2>(false);
            break;

        }
        case Slope_48:
        {
            *leftLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            leftLowCut.setBypassed<0>(false); //turns the filter back on
            *leftLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            leftLowCut.setBypassed<2>(false);
            *leftLowCut.get<3>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            leftLowCut.setBypassed<3>(false);
            break;
        }
    }
    // setting coefficients for the rightLow cut filter
    auto& rightLowCut = rightChain.get<ChainPosition::LowCut>();
    rightLowCut.setBypassed<0>(true); //bypass all four of the filters in the LowCutChain 
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
        case Slope_12:
        {
            *rightLowCut.get<0>().coefficients = *cutCoefficients[0]; //set the filter coefficient
            rightLowCut.setBypassed<0>(false); // turn the filter back on
            break;
        }
        case Slope_24:
        {
            *rightLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            rightLowCut.setBypassed<0>(false); //turns the filter back on
            *rightLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            rightLowCut.setBypassed<1>(false);
            break;
        }
        case Slope_36:
        {
            *rightLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            rightLowCut.setBypassed<0>(false); //turns the filter back on
            *rightLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            rightLowCut.setBypassed<2>(false);
            break;

        }
        case Slope_48:
        {
            *rightLowCut.get<0>().coefficients = *cutCoefficients[0]; //since its a second order filter two coefficients are returned
            rightLowCut.setBypassed<0>(false); //turns the filter back on
            *rightLowCut.get<1>().coefficients = *cutCoefficients[1]; //set the filter coefficient
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            rightLowCut.setBypassed<2>(false);
            *rightLowCut.get<3>().coefficients = *cutCoefficients[2]; //set the filter coefficient
            rightLowCut.setBypassed<3>(false);
            break;
        }
    }

    juce::dsp::AudioBlock<float> block(buffer); //processor chains need processor contexts each context has audio block that will be passed to the links in the chain
    auto leftBlock = block.getSingleChannelBlock(0); //extracts left channnel audio into a block
    auto rightBlock = block.getSingleChannelBlock(1);
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock); //Passes the block to the audio context
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    //now we can pass the context to the chains
    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool SimpleEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load(); //getRawParamValue returns the value in units that are meaningful (db,Hz, db/oct) rather than normalized values
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load(); //loads the param values into an instance of the struct
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    return settings;

}


juce::AudioProcessorValueTreeState::ParameterLayout SimpleEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", 
                                                           "LowCut Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f,0.25f), 20.f));//creates a pointer that gets cleaned up when its out of scope
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq",
                                                           "HighCut Freq",
                                                            juce::NormalisableRange<float>(20.f, 20000.f,1.f, 0.25f), 20000.f)); //argument list (label,label,low range of slider, high range,step size,pot taper), default value
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 750.f)); //argument list (label,label,low range of slider, high range,step size,pot taper), default value
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 0.25f), 20000.f)); //argument list (label,label,low range of slider, high range,step size,pot taper), default value
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
                                                           "Peak Quality",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 0.25f), 1.f)); //argument list (label,label,low range of slider, high range,step size,pot taper), default value
    juce::StringArray stringArray;
    for (int i = 0; i < 4; i++)
    {

        //for slope choice 0: 12 db/oct we need an order of 2 (1 coeff)
        //for slope choice 1: 24 db/oct we need an order of 4 (2 coeff)
        juce::String str;
        str << (12 +( i * 12));
        str << " db/Oct";
        stringArray.add(str);
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0)); //this makes a control like a dropdown of choices
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0));
    return layout;
}

juce::AudioProcessorEditor* SimpleEQAudioProcessor::createEditor()
{
   // return new SimpleEQAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void SimpleEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void SimpleEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleEQAudioProcessor();
}
