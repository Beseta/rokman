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
    using Compressor = juce::dsp::Compressor<float>;
    using WaveShaper = juce::dsp::WaveShaper<float>;
    using Gain = juce::dsp::Gain<float>;
    using MidBandPassFilter = juce::dsp::ProcessorChain<Filter, Filter>;
    using ComplexFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter>;
    using MonoChain = juce::dsp::ProcessorChain<Filter, Compressor, Filter, MidBandPassFilter, Gain, WaveShaper, Gain, Filter, ComplexFilter>;
    
    MonoChain leftChannel, rightChannel;
    
    enum ChainPositions {
        HPF,
        Comp,
        HBEQ,
        MBPF,
        OPAMP,
        AD,
        OPAMP2,
        LBEQ,
        CF,
    };
    
    float getFrequency(const ChainSettings &chainSettings);
    
    using Coefficients = Filter::CoefficientsPtr;
    
    static void updateCoefficients(Coefficients &old, const Coefficients &replacements);
    
    template<typename ChainType> void updateMBPF(ChainType& mbpf, Coefficients hpCoeff, Coefficients lpCoeff) {
        *mbpf.template get<0>().coefficients = *hpCoeff;
        *mbpf.template get<1>().coefficients = *lpCoeff;
        mbpf.template setBypassed<0>(false);
        mbpf.template setBypassed<1>(false);
    };
    
    template<typename ChainType, typename CoefficientType> void updateCF(ChainType& cf, Coefficients lsCoeff, Coefficients peakCoeff, CoefficientType lpCoeff) {
        *cf.template get<0>().coefficients = *lsCoeff;
        *cf.template get<1>().coefficients = *peakCoeff;
        *cf.template get<2>().coefficients = *lpCoeff[0];
        cf.template setBypassed<0>(false);
        cf.template setBypassed<1>(false);
        cf.template setBypassed<2>(false);
    };
    
    template<typename ChainType> void updateCompressor(ChainType& comp) {
        comp.setRatio(20.0);
        comp.setRelease(50.0);
        comp.setAttack(20.0);
        comp.setThreshold(-35.0);
    };
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RokmanAudioProcessor)
};
