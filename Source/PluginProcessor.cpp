#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <unistd.h>

namespace pluginbridge
{

juce::AudioProcessorValueTreeState::ParameterLayout PluginBridgeAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "role", 1 }, "Role",
        juce::StringArray { "Matrix / Duplex", "Sender Only", "Receiver Only" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "packetFrames", 1 }, "Packet Latency",
        juce::StringArray { "64 samples (1.3ms)", "128 samples (2.7ms)", "256 samples (5.3ms)" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { "jitterPackets", 1 }, "Jitter Buffer Depth", 1, 16, 3));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "masterMute", 1 }, "Master Mute", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "masterGain", 1 }, "Master Gain",
        juce::NormalisableRange<float>(-60.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

PluginBridgeAudioProcessor::PluginBridgeAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::discreteChannels(RoutingMatrix::MATRIX_SIZE), true)
                     .withOutput("Output", juce::AudioChannelSet::discreteChannels(RoutingMatrix::MATRIX_SIZE), true)),
      apvts_(*this, nullptr, "Parameters", createParameterLayout())
{
    instanceUuid_ = juce::Uuid().toString().toStdString();

    char host[32] = { 0 };
    if (gethostname(host, sizeof(host) - 1) == 0)
        instanceName_ = host;
    else
        instanceName_ = "DAW-Station";

    // Allocate scratch DSP buffers
    netTxBuffers_.resize(RoutingMatrix::MATRIX_SIZE);
    netTxPointers_.resize(RoutingMatrix::MATRIX_SIZE);
    netRxBuffers_.resize(RoutingMatrix::MATRIX_SIZE);
    netRxPointers_.resize(RoutingMatrix::MATRIX_SIZE);
    dawInPointers_.resize(RoutingMatrix::MATRIX_SIZE);
    dawOutPointers_.resize(RoutingMatrix::MATRIX_SIZE);

    for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        netTxBuffers_[i].resize(2048, 0.0f);
        netTxPointers_[i] = netTxBuffers_[i].data();
        netRxBuffers_[i].resize(2048, 0.0f);
        netRxPointers_[i] = netRxBuffers_[i].data();
    }

    // Start networking subsystems
    receiver_.start(audioPort_);
    sender_.start();
    sender_.setStreamInfo(instanceUuid_, streamName_, 48000, RoutingMatrix::MATRIX_SIZE);

    beacon_.setInstanceDetails(instanceUuid_, instanceName_, streamName_, audioPort_,
                              RoutingMatrix::MATRIX_SIZE, 48000, 0);
    beacon_.start();
}

PluginBridgeAudioProcessor::~PluginBridgeAudioProcessor()
{
    beacon_.stop();
    sender_.stop();
    receiver_.stop();
}

void PluginBridgeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    int maxBlock = std::max(samplesPerBlock * 2, 2048);
    for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        netTxBuffers_[i].resize(maxBlock, 0.0f);
        netTxPointers_[i] = netTxBuffers_[i].data();
        netRxBuffers_[i].resize(maxBlock, 0.0f);
        netRxPointers_[i] = netRxBuffers_[i].data();
    }

    sender_.setStreamInfo(instanceUuid_, streamName_, static_cast<uint32_t>(sampleRate), RoutingMatrix::MATRIX_SIZE);
    beacon_.setInstanceDetails(instanceUuid_, instanceName_, streamName_, audioPort_,
                              RoutingMatrix::MATRIX_SIZE, static_cast<uint32_t>(sampleRate), 0);
}

void PluginBridgeAudioProcessor::releaseResources()
{
}

bool PluginBridgeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support stereo up to 16 channels discrete
    int inChannels = layouts.getMainInputChannels();
    int outChannels = layouts.getMainOutputChannels();

    if (inChannels <= 0 && outChannels <= 0)
        return false;

    return inChannels <= RoutingMatrix::MATRIX_SIZE && outChannels <= RoutingMatrix::MATRIX_SIZE;
}

void PluginBridgeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalInChannels = getTotalNumInputChannels();
    const int totalOutChannels = getTotalNumOutputChannels();
    const int numFrames = buffer.getNumSamples();

    if (numFrames <= 0)
        return;

    // Update settings from APVTS
    int roleIdx = static_cast<int>(*apvts_.getRawParameterValue("role"));
    int packetFramesIdx = static_cast<int>(*apvts_.getRawParameterValue("packetFrames"));
    int packetSizes[] = { 64, 128, 256 };
    sender_.setPacketFrames(packetSizes[std::clamp(packetFramesIdx, 0, 2)]);

    int jitterPackets = static_cast<int>(*apvts_.getRawParameterValue("jitterPackets"));
    receiver_.setJitterBufferPackets(jitterPackets);

    bool masterMute = *apvts_.getRawParameterValue("masterMute") > 0.5f;
    float masterGainDb = *apvts_.getRawParameterValue("masterGain");
    float masterGain = juce::Decibels::decibelsToGain(masterGainDb);

    // Setup input pointers
    for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        dawInPointers_[i] = (i < totalInChannels) ? buffer.getReadPointer(i) : nullptr;
        dawOutPointers_[i] = (i < totalOutChannels) ? buffer.getWritePointer(i) : nullptr;
    }

    // 1. Fetch Network RX audio if in Receiver or Duplex role
    if (roleIdx != 1) // Not Sender Only
    {
        receiver_.readAudio(netRxPointers_.data(), RoutingMatrix::MATRIX_SIZE, numFrames);
    }
    else
    {
        for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
            std::fill(netRxBuffers_[i].begin(), netRxBuffers_[i].begin() + numFrames, 0.0f);
    }

    // 2. Process routing matrix
    matrix_.process(dawInPointers_.data(), totalInChannels,
                    netTxPointers_.data(), RoutingMatrix::MATRIX_SIZE,
                    netRxPointers_.data(), RoutingMatrix::MATRIX_SIZE,
                    dawOutPointers_.data(), totalOutChannels,
                    numFrames);

    // 3. Send Network TX audio if in Sender or Duplex role
    if (roleIdx != 2) // Not Receiver Only
    {
        sender_.pushAudio(netTxPointers_.data(), RoutingMatrix::MATRIX_SIZE, numFrames);
    }

    // 4. Apply master gain/mute to DAW output channels
    for (int ch = 0; ch < totalOutChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        if (masterMute)
        {
            juce::FloatVectorOperations::clear(channelData, numFrames);
        }
        else if (std::abs(masterGain - 1.0f) > 0.001f)
        {
            juce::FloatVectorOperations::multiply(channelData, masterGain, numFrames);
        }
    }
}

void PluginBridgeAudioProcessor::connectToPeer(const DiscoveredPeer& peer)
{
    sender_.addTarget(peer.ipAddress, peer.audioPort);
}

void PluginBridgeAudioProcessor::disconnectPeer(const DiscoveredPeer& peer)
{
    sender_.removeTarget(peer.ipAddress, peer.audioPort);
}

void PluginBridgeAudioProcessor::setInstanceName(const std::string& name)
{
    instanceName_ = name;
    beacon_.setInstanceDetails(instanceUuid_, instanceName_, streamName_, audioPort_,
                              RoutingMatrix::MATRIX_SIZE, 48000, 0);
}

void PluginBridgeAudioProcessor::setAudioPort(uint16_t port)
{
    if (port != audioPort_)
    {
        audioPort_ = port;
        receiver_.stop();
        receiver_.start(audioPort_);
        beacon_.setInstanceDetails(instanceUuid_, instanceName_, streamName_, audioPort_,
                                  RoutingMatrix::MATRIX_SIZE, 48000, 0);
    }
}

void PluginBridgeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    xml->setAttribute("instanceName", juce::String(instanceName_));
    xml->setAttribute("audioPort", static_cast<int>(audioPort_));

    // Save routing matrix crosspoints
    auto* matrixXml = xml->createNewChildElement("RoutingMatrix");
    for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        for (int j = 0; j < RoutingMatrix::MATRIX_SIZE; ++j)
        {
            if (matrix_.getInputToTx(i, j))
            {
                auto* e = matrixXml->createNewChildElement("InToTx");
                e->setAttribute("in", i);
                e->setAttribute("tx", j);
            }
            if (matrix_.getRxToOutput(i, j))
            {
                auto* e = matrixXml->createNewChildElement("RxToOut");
                e->setAttribute("rx", i);
                e->setAttribute("out", j);
            }
        }
    }

    // Save direct audio interface channel mappings
    for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        auto* inMap = matrixXml->createNewChildElement("InMap");
        inMap->setAttribute("ch", i);
        inMap->setAttribute("hw", matrix_.getInputChannelMap(i));

        auto* outMap = matrixXml->createNewChildElement("OutMap");
        outMap->setAttribute("ch", i);
        outMap->setAttribute("hw", matrix_.getOutputChannelMap(i));
    }

    copyXmlToBinary(*xml, destData);
}

void PluginBridgeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts_.state.getType()))
    {
        apvts_.replaceState(juce::ValueTree::fromXml(*xmlState));
        if (xmlState->hasAttribute("instanceName"))
            setInstanceName(xmlState->getStringAttribute("instanceName").toStdString());
        if (xmlState->hasAttribute("audioPort"))
            setAudioPort(static_cast<uint16_t>(xmlState->getIntAttribute("audioPort")));

        if (auto* matrixXml = xmlState->getChildByName("RoutingMatrix"))
        {
            matrix_.reset();
            for (auto* e : matrixXml->getChildIterator())
            {
                if (e->hasTagName("InToTx"))
                    matrix_.setInputToTx(e->getIntAttribute("in"), e->getIntAttribute("tx"), true);
                else if (e->hasTagName("RxToOut"))
                    matrix_.setRxToOutput(e->getIntAttribute("rx"), e->getIntAttribute("out"), true);
                else if (e->hasTagName("InMap"))
                    matrix_.setInputChannelMap(e->getIntAttribute("ch"), e->getIntAttribute("hw"));
                else if (e->hasTagName("OutMap"))
                    matrix_.setOutputChannelMap(e->getIntAttribute("ch"), e->getIntAttribute("hw"));
            }
        }
    }
}

juce::AudioProcessorEditor* PluginBridgeAudioProcessor::createEditor()
{
    return new PluginBridgeAudioProcessorEditor(*this);
}

} // namespace pluginbridge

// JUCE AudioProcessor entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new pluginbridge::PluginBridgeAudioProcessor();
}

