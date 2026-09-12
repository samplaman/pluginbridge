#pragma once

#include "NetworkProtocol.h"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

namespace pluginbridge
{

class BeaconService
{
public:
    using PeerCallback = std::function<void(const std::vector<DiscoveredPeer>&)>;

    BeaconService();
    ~BeaconService();

    void start(uint16_t beaconPort = DEFAULT_BEACON_PORT);
    void stop();

    void setInstanceDetails(const std::string& uuid,
                            const std::string& instanceName,
                            const std::string& streamName,
                            uint16_t audioPort,
                            uint16_t channels,
                            uint32_t sampleRate,
                            uint8_t role);

    std::vector<DiscoveredPeer> getActivePeers() const;
    void setOnPeersUpdated(PeerCallback callback);

    std::string getLocalIp() const;
    static std::vector<std::string> getLocalIpList();

private:
    void broadcastLoop();
    void listenLoop();
    void pruneStalePeers();

    std::atomic<bool> running_ { false };
    std::thread broadcastThread_;
    std::thread listenThread_;

    mutable std::mutex peerMutex_;
    std::vector<DiscoveredPeer> peers_;
    PeerCallback onPeersUpdated_;

    BeaconPacket localBeacon_ {};
    std::mutex beaconConfigMutex_;

    uint16_t beaconPort_ { DEFAULT_BEACON_PORT };
    int listenSocket_ { -1 };
    int sendSocket_ { -1 };
};

} // namespace pluginbridge

