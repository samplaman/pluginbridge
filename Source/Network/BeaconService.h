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
    using LinkCommandCallback = std::function<void(const DiscoveredPeer&, bool connect)>;

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
    void setOnLinkCommand(LinkCommandCallback callback);

    std::string getLocalIp() const;
    static std::vector<std::string> getLocalIpList();

    void pingPeer(const std::string& ipAddress, uint16_t beaconPort = DEFAULT_BEACON_PORT);
    void addUnicastTarget(const std::string& ipAddress, uint16_t beaconPort = DEFAULT_BEACON_PORT);
    void removeUnicastTarget(const std::string& ipAddress, uint16_t beaconPort = DEFAULT_BEACON_PORT);
    void scanSubnet();
    void sendLinkCommand(const std::string& ipAddress, uint16_t beaconPort, bool connect);

private:
    struct UnicastTarget
    {
        std::string ip;
        uint16_t port;
    };
    void broadcastLoop();
    void listenLoop();
    void pruneStalePeers();

    std::atomic<bool> running_ { false };
    std::thread broadcastThread_;
    std::thread listenThread_;

    mutable std::mutex peerMutex_;
    std::vector<DiscoveredPeer> peers_;
    PeerCallback onPeersUpdated_;
    LinkCommandCallback onLinkCommand_;

    BeaconPacket localBeacon_ {};
    std::mutex beaconConfigMutex_;

    uint16_t beaconPort_ { DEFAULT_BEACON_PORT };
    int listenSocket_ { -1 };
    int sendSocket_ { -1 };

    mutable std::mutex unicastMutex_;
    std::vector<UnicastTarget> unicastTargets_;
};

} // namespace pluginbridge

