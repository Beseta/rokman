/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RokmanAudioProcessor::RokmanAudioProcessor()
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

RokmanAudioProcessor::~RokmanAudioProcessor()
{
}

//==============================================================================
const juce::String RokmanAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RokmanAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RokmanAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RokmanAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RokmanAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RokmanAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RokmanAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RokmanAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String RokmanAudioProcessor::getProgramName (int index)
{
    return {};
}

void RokmanAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void RokmanAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    leftChannel.prepare(spec);
    rightChannel.prepare(spec);
    
    auto chainSettings = getChainSettings(apvts);
    
    // HPF 11
    auto hpf1Coeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, getFrequency(chainSettings));
    updateCoefficients(leftChannel.get<ChainPositions::HPF>().coefficients, hpf1Coeff);
    updateCoefficients(rightChannel.get<ChainPositions::HPF>().coefficients, hpf1Coeff);
    
    // Compressor 12
    auto comp = juce::dsp::Compressor<float> ();
    comp.setRatio(2.0);
    comp.setRelease(50);
    comp.setAttack(20);
    comp.setThreshold(20);
    //    leftChannel.get<ChainPositions::Comp>() = comp;
    //    rightChannel.get<ChainPositions::Comp>() = comp;
    
    // HPF 12.A & 13 Coefficients
    auto hbeqCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 4000.0, 1, 1.5);
    updateCoefficients(leftChannel.get<ChainPositions::HBEQ>().coefficients, hbeqCoeff);
    updateCoefficients(rightChannel.get<ChainPositions::HBEQ>().coefficients, hbeqCoeff);
    
    // MBPF 14 Coefficients
    auto mbpfHPCoeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(sampleRate, 800.0);
    auto mbpfLPCoeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(sampleRate, 5000.0);
    
    auto& leftMBPF = leftChannel.get<ChainPositions::MBPF>();
    auto& rightMBPF = rightChannel.get<ChainPositions::MBPF>();

    updateMBPF(leftMBPF, mbpfHPCoeff, mbpfLPCoeff);
    updateMBPF(rightMBPF, mbpfHPCoeff, mbpfLPCoeff);
    
    
    // LBEQ 15 Coefficients
    auto lbeqCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 50, 1, 1.5);
    updateCoefficients(leftChannel.get<ChainPositions::LBEQ>().coefficients, lbeqCoefficients);
    updateCoefficients(rightChannel.get<ChainPositions::LBEQ>().coefficients, lbeqCoefficients); 
    
    // CF 17 Coefficients
    auto cfLSCoeff = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sampleRate, 80, 1, 1.5);
    auto cfLPCoeff = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(4000, sampleRate, 2);
    auto cfPeakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, 1600, 1, 0.5);
    
    auto& leftCF = leftChannel.get<ChainPositions::CF>();
    auto& rightCF = rightChannel.get<ChainPositions::CF>();
    
    updateCF(leftCF, cfLSCoeff, cfPeakCoeff, cfLPCoeff);
    updateCF(rightCF, cfLSCoeff, cfPeakCoeff, cfLPCoeff);
    
    switch (chainSettings.mode) {
        case 0: // Dist
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(true);
            rightChannel.setBypassed<ChainPositions::HBEQ>(true);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 1: // Edge
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 2: // Cln1
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 3: // Cln2
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(false);
            rightChannel.setBypassed<ChainPositions::LBEQ>(false);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(true);
            rightChannel.setBypassed<ChainPositions::CF>(true);
            break;
    }
    
}

void RokmanAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RokmanAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void RokmanAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    
    auto chainSettings = getChainSettings(apvts);
    
    // HPF 11
    auto hpf1Coeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(getSampleRate(), getFrequency(chainSettings));
    updateCoefficients(leftChannel.get<ChainPositions::HPF>().coefficients, hpf1Coeff);
    updateCoefficients(rightChannel.get<ChainPositions::HPF>().coefficients, hpf1Coeff);
    
    // Compressor 12
    auto comp = juce::dsp::Compressor<float> ();
    comp.setRatio(2.0);
    comp.setRelease(50);
    comp.setAttack(20);
    comp.setThreshold(20);
    //    leftChannel.get<ChainPositions::Comp>() = comp;
    //    rightChannel.get<ChainPositions::Comp>() = comp;
    
    // HPF 12.A & 13 Coefficients
    auto hbeqCoeff = juce::dsp::IIR::Coefficients<float>::makeHighShelf(getSampleRate(), 4000.0, 1, 1.5);
    updateCoefficients(leftChannel.get<ChainPositions::HBEQ>().coefficients, hbeqCoeff);
    updateCoefficients(rightChannel.get<ChainPositions::HBEQ>().coefficients, hbeqCoeff);
    
    // MBPF 14 Coefficients
    auto mbpfHPCoeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass(getSampleRate(), 800.0);
    auto mbpfLPCoeff = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass(getSampleRate(), 5000.0);

    auto& leftMBPF = leftChannel.get<ChainPositions::MBPF>();
    auto& rightMBPF = rightChannel.get<ChainPositions::MBPF>();

    updateMBPF(leftMBPF, mbpfHPCoeff, mbpfLPCoeff);
    updateMBPF(rightMBPF, mbpfHPCoeff, mbpfLPCoeff);
    
    // LBEQ 15 Coefficients
    auto lbeqCoefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf(getSampleRate(), 50, 1, 1.5);
    updateCoefficients(leftChannel.get<ChainPositions::LBEQ>().coefficients, lbeqCoefficients);
    updateCoefficients(rightChannel.get<ChainPositions::LBEQ>().coefficients, lbeqCoefficients);
    
    // CF 17 Coefficients
    auto cfLSCoeff = juce::dsp::IIR::Coefficients<float>::makeLowShelf(getSampleRate(), 80, 1, 1.5);
    auto cfPeakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(), 1600, 1, 0.5);
    auto cfLPCoeff = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(4000, getSampleRate(), 2);
    
    auto& leftCF = leftChannel.get<ChainPositions::CF>();
    auto& rightCF = rightChannel.get<ChainPositions::CF>();
    
    updateCF(leftCF, cfLSCoeff, cfPeakCoeff, cfLPCoeff);
    updateCF(rightCF, cfLSCoeff, cfPeakCoeff, cfLPCoeff);
    
    switch (chainSettings.mode) {
        case 0: // Dist
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(true);
            rightChannel.setBypassed<ChainPositions::HBEQ>(true);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 1: // Edge
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 2: // Cln1
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(true);
            rightChannel.setBypassed<ChainPositions::LBEQ>(true);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(false);
            rightChannel.setBypassed<ChainPositions::CF>(false);
            break;
            
        case 3: // Cln2
            // HPF 12.A
            leftChannel.setBypassed<ChainPositions::HBEQ>(false);
            rightChannel.setBypassed<ChainPositions::HBEQ>(false);
            // LBEQ 15
            leftChannel.setBypassed<ChainPositions::LBEQ>(false);
            rightChannel.setBypassed<ChainPositions::LBEQ>(false);
            // CF 17
            leftChannel.setBypassed<ChainPositions::CF>(true);
            rightChannel.setBypassed<ChainPositions::CF>(true);
            break;
    }
    
    juce::dsp::AudioBlock<float> block(buffer);
    
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    
    leftChannel.process(leftContext);
    rightChannel.process(rightContext);
}

//==============================================================================
bool RokmanAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* RokmanAudioProcessor::createEditor()
{
//    return new RokmanAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void RokmanAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void RokmanAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState &apvts) {
    ChainSettings settings;
    settings.mode = apvts.getRawParameterValue("Mode")->load();
    return settings;
};

float RokmanAudioProcessor::getFrequency(const ChainSettings &chainSettings ) {
    if (chainSettings.mode == 0 || chainSettings.mode == 1) {
        return 10000.0;
    } else {
        return 5000.0;
    }
}

void RokmanAudioProcessor::updateCoefficients(Coefficients &old, const Coefficients &replacements) {
    *old = *replacements;
}

juce::AudioProcessorValueTreeState::ParameterLayout RokmanAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterChoice>("Mode", "Mode", juce::StringArray {"Dist", "Edge", "Cln1", "Cln2"}, 0));
    return layout;
}
//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RokmanAudioProcessor();
}
