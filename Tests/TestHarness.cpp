#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>

#include "../Source/Network/NetworkProtocol.h"
#include "../Source/Network/BeaconService.h"
#include "../Source/Network/AudioSender.h"
#include "../Source/Network/AudioReceiver.h"
#include "../Source/DSP/JitterBuffer.h"
#include "../Source/DSP/DriftCompensator.h"
#include "../Source/DSP/RoutingMatrix.h"

using namespace pluginbridge;

void testNetworkProtocol()
{
    std::cout << "[TEST] Running testNetworkProtocol()...\n";
    assert(sizeof(AudioPacketHeader) == 124);
    assert(sizeof(BeaconPacket) == 154);

    AudioPacketHeader header {};
    header.magic = AUDIO_MAGIC;
    header.version = PROTOCOL_VER;
    header.numChannels = 8;
    header.sampleRate = 96000;
    header.numFrames = 128;
    header.sequenceNumber = 42;

    assert(header.magic == 0x504C4742);
    assert(header.numChannels == 8);
    assert(header.sampleRate == 96000);
    std::cout << "  ✓ Protocol header packing verified.\n";
}

void testRoutingMatrix()
{
    std::cout << "[TEST] Running testRoutingMatrix()...\n";
    RoutingMatrix matrix;

    // Test 1:1 default routing
    assert(matrix.getInputToTx(0, 0) == true);
    assert(matrix.getInputToTx(0, 1) == false);
    assert(matrix.getRxToOutput(1, 1) == true);

    // Change crosspoints
    matrix.setInputToTx(0, 1, true);
    assert(matrix.getInputToTx(0, 1) == true);

    // Test processing audio block
    constexpr int frames = 64;
    std::vector<float> in0(frames, 0.5f);
    std::vector<float> in1(frames, -0.5f);
    const float* dawIn[2] = { in0.data(), in1.data() };

    std::vector<float> tx0(frames, 0.0f);
    std::vector<float> tx1(frames, 0.0f);
    float* netTx[2] = { tx0.data(), tx1.data() };

    std::vector<float> rx0(frames, 0.25f);
    std::vector<float> rx1(frames, -0.25f);
    const float* netRx[2] = { rx0.data(), rx1.data() };

    std::vector<float> out0(frames, 0.0f);
    std::vector<float> out1(frames, 0.0f);
    float* dawOut[2] = { out0.data(), out1.data() };

    matrix.process(dawIn, 2, netTx, 2, netRx, 2, dawOut, 2, frames);

    // TX0 should have received in0 (0.5f)
    assert(std::abs(tx0[0] - 0.5f) < 0.0001f);
    // TX1 should have received in0 (0.5f) + in1 (-0.5f) = 0.0f
    assert(std::abs(tx1[0] - 0.0f) < 0.0001f);
    // DAW Out 0 should have received rx0 (0.25f)
    assert(std::abs(out0[0] - 0.25f) < 0.0001f);

    // Check meters
    assert(matrix.getDawInPeak(0) > 0.49f);
    assert(matrix.getDawOutPeak(0) > 0.24f);

    // Check available channel auto-tracking
    assert(matrix.getNumAvailableDawInputs() == 2);
    assert(matrix.getNumAvailableDawOutputs() == 2);

    std::cout << "  ✓ RoutingMatrix matrix routing and metering verified.\n";
}

void testJitterBufferAndDrift()
{
    std::cout << "[TEST] Running testJitterBufferAndDrift()...\n";
    JitterBuffer jbuf(2, 2); // 2 channels, 2 packet preload
    AudioPacketHeader header {};
    header.magic = AUDIO_MAGIC;
    header.version = PROTOCOL_VER;
    header.numChannels = 2;
    header.sampleRate = 48000;
    header.numFrames = 128;

    std::vector<float> payload(128 * 2, 0.707f);

    // Push 3 packets
    for (uint32_t seq = 0; seq < 3; ++seq)
    {
        header.sequenceNumber = seq;
        jbuf.pushPacket(header, payload.data(), payload.size() * sizeof(float));
    }

    assert(jbuf.getPacketsReceived() == 3);
    assert(jbuf.getBufferFillFrames() == 128 * 3);

    // Read 128 frames
    std::vector<float> outLeft(128, 0.0f);
    std::vector<float> outRight(128, 0.0f);
    float* channels[2] = { outLeft.data(), outRight.data() };

    jbuf.readFrames(channels, 2, 128);

    assert(std::abs(outLeft[0] - 0.707f) < 0.001f);
    assert(std::abs(outRight[127] - 0.707f) < 0.001f);
    assert(jbuf.getBufferFillFrames() == 128 * 2);

    // Test DriftCompensator
    DriftCompensator comp(2);
    comp.setTargetBufferFrames(256);

    std::vector<float> compOutLeft(128, 0.0f);
    std::vector<float> compOutRight(128, 0.0f);
    float* compOut[2] = { compOutLeft.data(), compOutRight.data() };

    // When buffer fill is higher than target, speed ratio should increase (> 1.0)
    for (int i = 0; i < 20; ++i)
    {
        comp.process(channels, 128, compOut, 128, 2, 512 /* higher fill */);
    }
    assert(comp.getCurrentSpeedRatio() > 1.00001f);

    // When buffer fill is lower than target, speed ratio should decrease (< 1.0)
    for (int i = 0; i < 50; ++i)
    {
        comp.process(channels, 128, compOut, 128, 2, 64 /* lower fill */);
    }
    assert(comp.getCurrentSpeedRatio() < 1.0f);

    std::cout << "  ✓ JitterBuffer and DriftCompensator closed-loop convergence verified.\n";
}

void testLiveSenderReceiverLoopback()
{
    std::cout << "[TEST] Running testLiveSenderReceiverLoopback()...\n";

    AudioReceiver receiver;
    const uint16_t testPort = 54321;
    bool rxStarted = receiver.start(testPort);
    assert(rxStarted);

    AudioSender sender;
    sender.start();
    sender.setPacketFrames(128);
    sender.setStreamInfo("test-uuid-1", "LoopbackTest", 48000, 2);
    sender.addTarget("127.0.0.1", testPort);

    // Generate a 440 Hz test sine wave into sender
    constexpr int totalBlocks = 30;
    constexpr int blockSize = 128;
    std::vector<float> sineL(blockSize);
    std::vector<float> sineR(blockSize);

    for (int b = 0; b < totalBlocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            float t = static_cast<float>(b * blockSize + i) / 48000.0f;
            sineL[i] = std::sin(2.0f * 3.14159265f * 440.0f * t);
            sineR[i] = std::cos(2.0f * 3.14159265f * 440.0f * t);
        }
        const float* inCh[2] = { sineL.data(), sineR.data() };
        sender.pushAudio(inCh, 2, blockSize);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Wait for network packets to be received
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    uint32_t received = receiver.getPacketsReceived();
    std::cout << "  Packets sent: " << sender.getPacketsSent()
              << ", Packets received: " << received << "\n";
    assert(received > 10);

    // Pull audio from receiver
    std::vector<float> rxOutL(blockSize, 0.0f);
    std::vector<float> rxOutR(blockSize, 0.0f);
    float* rxCh[2] = { rxOutL.data(), rxOutR.data() };
    receiver.readAudio(rxCh, 2, blockSize);

    // Check that we received actual sine wave non-zero audio
    float maxVal = 0.0f;
    for (int i = 0; i < blockSize; ++i)
    {
        maxVal = std::max(maxVal, std::abs(rxOutL[i]));
    }
    std::cout << "  Max received sample amplitude: " << maxVal << "\n";
    assert(maxVal > 0.1f);

    sender.stop();
    receiver.stop();
    std::cout << "  ✓ Live UDP sender/receiver loopback audio streaming verified.\n";
}

void testBeaconDiscovery()
{
    std::cout << "[TEST] Running testBeaconDiscovery()...\n";

    BeaconService peer1;
    BeaconService peer2;

    peer1.setInstanceDetails("peer-1-uuid", "StudioMacMaster", "MasterBus", 52801, 8, 48000, 0);
    peer2.setInstanceDetails("peer-2-uuid", "MixingPCFollower", "MixBus", 52802, 8, 48000, 0);

    peer1.start(52890);
    peer2.start(52890);

    // Wait for discovery beacons to cross
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    auto peersSeenBy1 = peer1.getActivePeers();
    auto peersSeenBy2 = peer2.getActivePeers();

    std::cout << "  Peer 1 saw " << peersSeenBy1.size() << " peers.\n";
    std::cout << "  Peer 2 saw " << peersSeenBy2.size() << " peers.\n";

    assert(!peersSeenBy1.empty());
    assert(!peersSeenBy2.empty());

    bool found2In1 = false;
    for (const auto& p : peersSeenBy1)
    {
        if (p.instanceName == "MixingPCFollower")
            found2In1 = true;
    }
    assert(found2In1);

    peer1.stop();
    peer2.stop();
    std::cout << "  ✓ LAN beacon auto-discovery verified.\n";
}

void test48ChannelMatrixAndStreaming()
{
    std::cout << "[TEST] Running test48ChannelMatrixAndStreaming()...\n";
    RoutingMatrix matrix;
    assert(RoutingMatrix::MATRIX_SIZE == 48);

    // Test 1:1 routing across all 48 channels
    for (int i = 0; i < 48; ++i)
    {
        assert(matrix.getInputToTx(i, i) == true);
        assert(matrix.getRxToOutput(i, i) == true);
    }

    constexpr int frames = 128;
    std::vector<std::vector<float>> inBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<const float*> inPointers(48);
    for (int ch = 0; ch < 48; ++ch)
    {
        std::fill(inBuffers[ch].begin(), inBuffers[ch].end(), static_cast<float>(ch + 1) * 0.01f);
        inPointers[ch] = inBuffers[ch].data();
    }

    std::vector<std::vector<float>> txBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<float*> txPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        txPointers[ch] = txBuffers[ch].data();

    std::vector<std::vector<float>> rxBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<const float*> rxPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        rxPointers[ch] = rxBuffers[ch].data();

    std::vector<std::vector<float>> outBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<float*> outPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        outPointers[ch] = outBuffers[ch].data();

    matrix.process(inPointers.data(), 48, txPointers.data(), 48, rxPointers.data(), 48, outPointers.data(), 48, frames);

    assert(std::abs(txBuffers[47][0] - 48.0f * 0.01f) < 0.0001f);
    assert(matrix.getNetworkTxPeak(47) > 0.4f);

    std::cout << "  ✓ 48-channel routing matrix DSP and metering verified.\n";
}

void testAudioInterfaceChannelMapping()
{
    std::cout << "[TEST] Running testAudioInterfaceChannelMapping()...\n";
    RoutingMatrix matrix;

    // Test default 1:1 hardware mapping
    for (int i = 0; i < 48; ++i)
    {
        assert(matrix.getInputChannelMap(i) == i);
        assert(matrix.getOutputChannelMap(i) == i);
    }

    // Remap Matrix In 0 to physical Audio Interface Channel 16
    matrix.setInputChannelMap(0, 16);
    assert(matrix.getInputChannelMap(0) == 16);

    // Remap Matrix Out 0 to physical Audio Interface Channel 24
    matrix.setOutputChannelMap(0, 24);
    assert(matrix.getOutputChannelMap(0) == 24);

    // Test offset shift (+8)
    matrix.setInputChannelOffset(8);
    for (int i = 0; i < 48; ++i)
    {
        assert(matrix.getInputChannelMap(i) == (i + 8) % 48);
    }

    // Test bank mapping (Map Bank 1 [0..15] to HW [16..31])
    matrix.setInputBank(0, 16, 16);
    for (int i = 0; i < 16; ++i)
    {
        assert(matrix.getInputChannelMap(i) == 16 + i);
    }

    // Test reset
    matrix.resetInputChannelMap();
    matrix.resetOutputChannelMap();
    for (int i = 0; i < 48; ++i)
    {
        assert(matrix.getInputChannelMap(i) == i);
        assert(matrix.getOutputChannelMap(i) == i);
    }

    // Verify audio signal routing through remapped channels
    // Remap Matrix In 0 to take audio from HW In Channel 10
    matrix.setInputChannelMap(0, 10);

    constexpr int frames = 64;
    std::vector<std::vector<float>> inBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<const float*> inPointers(48);
    for (int ch = 0; ch < 48; ++ch)
    {
        // Each HW channel gets a distinct sample value: (ch + 1) * 0.1f
        std::fill(inBuffers[ch].begin(), inBuffers[ch].end(), static_cast<float>(ch + 1) * 0.1f);
        inPointers[ch] = inBuffers[ch].data();
    }

    std::vector<std::vector<float>> txBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<float*> txPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        txPointers[ch] = txBuffers[ch].data();

    std::vector<std::vector<float>> rxBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<const float*> rxPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        rxPointers[ch] = rxBuffers[ch].data();

    std::vector<std::vector<float>> outBuffers(48, std::vector<float>(frames, 0.0f));
    std::vector<float*> outPointers(48);
    for (int ch = 0; ch < 48; ++ch)
        outPointers[ch] = outBuffers[ch].data();

    // Process audio: Matrix In 0 is connected to Tx 0 (default 1:1 matrix crosspoint)
    matrix.process(inPointers.data(), 48, txPointers.data(), 48, rxPointers.data(), 48, outPointers.data(), 48, frames);

    // Because Matrix In 0 was mapped to HW In 10 (value 11 * 0.1 = 1.1f),
    // txBuffers[0] must contain 1.1f, NOT 0.1f!
    assert(std::abs(txBuffers[0][0] - 1.1f) < 0.0001f);

    std::cout << "  ✓ Direct audio interface channel mapping and re-routed DSP verified.\n";
}

int main()
{
    std::cout << "=================================================\n";
    std::cout << "    PLUGINBRIDGE ENGINE TEST SUITE               \n";
    std::cout << "=================================================\n";

    testNetworkProtocol();
    testRoutingMatrix();
    testJitterBufferAndDrift();
    testLiveSenderReceiverLoopback();
    testBeaconDiscovery();
    test48ChannelMatrixAndStreaming();
    testAudioInterfaceChannelMapping();

    std::cout << "=================================================\n";
    std::cout << "    ALL ENGINE TESTS PASSED SUCCESSFULLY!        \n";
    std::cout << "=================================================\n";
    return 0;
}
