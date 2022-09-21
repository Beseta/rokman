/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct ChainSettings {
    int mode {0};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &apvts);

//==============================================================================
/**
*/
class RokmanAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    RokmanAudioProcessor();
    ~RokmanAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

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
    
    // Se encarga de pasarle al estado los parametros que utilizamos
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Es la variable a la que se cuelgan los datos
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};
private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using DistChain = juce::dsp::ProcessorChain<>;
    using EdgeChain = juce::dsp::ProcessorChain<>;
    using Cln1Chain = juce::dsp::ProcessorChain<>;
    using Cln2Chain = juce::dsp::ProcessorChain<>;
    using MonoChain = juce::dsp::ProcessorChain<Filter, DistChain, EdgeChain, Cln1Chain, Cln2Chain>;
    
    MonoChain leftChannel, rightChannel;
    
    enum ChainPositions {
        HPF,
        Distortion,
        Edge,
        Cln1,
        Cln2
    };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RokmanAudioProcessor)
};
