#include "BeaconService.h"

#include <chrono>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <poll.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>

namespace pluginbridge
{

struct InterfaceInfo
{
    std::string name;
    in_addr ip;
    in_addr broadcast;
    bool hasBroadcast { false };
};

static std::vector<InterfaceInfo> getLocalInterfaces()
{
    std::vector<InterfaceInfo> list;
    ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) == 0 && ifap != nullptr)
    {
        for (ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            // Skip loopback or down interfaces
            if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP))
                continue;

            InterfaceInfo info;
            info.name = ifa->ifa_name ? ifa->ifa_name : "";
            info.ip = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr;

            if ((ifa->ifa_flags & IFF_BROADCAST) && ifa->ifa_broadaddr != nullptr)
            {
                info.broadcast = reinterpret_cast<sockaddr_in*>(ifa->ifa_broadaddr)->sin_addr;
                info.hasBroadcast = true;
            }
            list.push_back(info);
        }
        freeifaddrs(ifap);
    }
    return list;
}

static uint64_t getCurrentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::vector<std::string> BeaconService::getLocalIpList()
{
    std::vector<std::string> ips;
    auto ifaces = getLocalInterfaces();
    for (const auto& iface : ifaces)
    {
        char buf[INET_ADDRSTRLEN] = {0};
        inet_ntop(AF_INET, &iface.ip, buf, sizeof(buf));
        ips.push_back(buf);
    }
    return ips;
}

std::string BeaconService::getLocalIp() const
{
    auto list = getLocalIpList();
    if (!list.empty())
        return list[0];
    return "127.0.0.1";
}

BeaconService::BeaconService()
{
    std::memset(&localBeacon_, 0, sizeof(localBeacon_));
    localBeacon_.magic = BEACON_MAGIC;
    localBeacon_.version = PROTOCOL_VER;
    localBeacon_.audioPort = DEFAULT_AUDIO_PORT;
    localBeacon_.numChannels = 2;
    localBeacon_.sampleRate = 48000;
    localBeacon_.role = 0;

    char host[32] = {0};
    if (gethostname(host, sizeof(host) - 1) == 0)
    {
        std::strncpy(localBeacon_.hostName, host, sizeof(localBeacon_.hostName) - 1);
    }
    else
    {
        std::strncpy(localBeacon_.hostName, "AudioWorkstation", sizeof(localBeacon_.hostName) - 1);
    }

    std::strncpy(localBeacon_.instanceName, "PluginBridge", sizeof(localBeacon_.instanceName) - 1);
    std::strncpy(localBeacon_.streamName, "MainMix", sizeof(localBeacon_.streamName) - 1);
}

BeaconService::~BeaconService()
{
    stop();
}

void BeaconService::setInstanceDetails(const std::string& uuid,
                                       const std::string& instanceName,
                                       const std::string& streamName,
                                       uint16_t audioPort,
                                       uint16_t channels,
                                       uint32_t sampleRate,
                                       uint8_t role)
{
    std::lock_guard<std::mutex> lock(beaconConfigMutex_);
    std::strncpy(localBeacon_.instanceUuid, uuid.c_str(), sizeof(localBeacon_.instanceUuid) - 1);
    std::strncpy(localBeacon_.instanceName, instanceName.c_str(), sizeof(localBeacon_.instanceName) - 1);
    std::strncpy(localBeacon_.streamName, streamName.c_str(), sizeof(localBeacon_.streamName) - 1);
    localBeacon_.audioPort = audioPort;
    localBeacon_.numChannels = channels;
    localBeacon_.sampleRate = sampleRate;
    localBeacon_.role = role;
}

void BeaconService::start(uint16_t beaconPort)
{
    if (running_.exchange(true))
        return;

    beaconPort_ = beaconPort;

    // Create UDP send socket
    sendSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSocket_ >= 0)
    {
        int broadcastEnable = 1;
        setsockopt(sendSocket_, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

        // Multicast loopback enabled so local instances on same computer can discover each other
        uint8_t loop = 1;
        setsockopt(sendSocket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

        // Multicast TTL = 2 for local subnet traversal
        uint8_t ttl = 2;
        setsockopt(sendSocket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }

    // Create UDP listen socket
    listenSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenSocket_ >= 0)
    {
        int reuse = 1;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

        sockaddr_in bindAddr {};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_port = htons(beaconPort_);
        bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == 0)
        {
            // Join multicast group on default interface
            ip_mreq mreq {};
            mreq.imr_multiaddr.s_addr = inet_addr(DEFAULT_MULTICAST_GROUP);
            mreq.imr_interface.s_addr = htonl(INADDR_ANY);
            setsockopt(listenSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

            // Also join explicitly on each physical interface (vital for Linux multi-interface / wifi)
            auto ifaces = getLocalInterfaces();
            for (const auto& iface : ifaces)
            {
                ip_mreq ifMreq {};
                ifMreq.imr_multiaddr.s_addr = inet_addr(DEFAULT_MULTICAST_GROUP);
                ifMreq.imr_interface = iface.ip;
                setsockopt(listenSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ifMreq, sizeof(ifMreq));
            }
        }
        else
        {
            close(listenSocket_);
            listenSocket_ = -1;
        }
    }

    broadcastThread_ = std::thread(&BeaconService::broadcastLoop, this);
    listenThread_ = std::thread(&BeaconService::listenLoop, this);
}

void BeaconService::stop()
{
    if (!running_.exchange(false))
        return;

    if (listenSocket_ >= 0)
    {
        close(listenSocket_);
        listenSocket_ = -1;
    }
    if (sendSocket_ >= 0)
    {
        close(sendSocket_);
        sendSocket_ = -1;
    }

    if (broadcastThread_.joinable())
        broadcastThread_.join();
    if (listenThread_.joinable())
        listenThread_.join();

    std::lock_guard<std::mutex> lock(peerMutex_);
    peers_.clear();
}

void BeaconService::broadcastLoop()
{
    sockaddr_in mcastTarget {};
    mcastTarget.sin_family = AF_INET;
    mcastTarget.sin_port = htons(beaconPort_);
    mcastTarget.sin_addr.s_addr = inet_addr(DEFAULT_MULTICAST_GROUP);

    sockaddr_in bcastTarget {};
    bcastTarget.sin_family = AF_INET;
    bcastTarget.sin_port = htons(beaconPort_);
    bcastTarget.sin_addr.s_addr = inet_addr("255.255.255.255");

    auto startTime = getCurrentTimeMs();

    while (running_)
    {
        BeaconPacket packetCopy;
        {
            std::lock_guard<std::mutex> lock(beaconConfigMutex_);
            localBeacon_.uptimeMs = getCurrentTimeMs() - startTime;
            packetCopy = localBeacon_;
        }

        if (sendSocket_ >= 0)
        {
            auto ifaces = getLocalInterfaces();

            // 1. Send standard multicast beacon
            sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                   reinterpret_cast<sockaddr*>(&mcastTarget), sizeof(mcastTarget));

            // 2. Transmit multicast pinned to each active network interface
            for (const auto& iface : ifaces)
            {
                setsockopt(sendSocket_, IPPROTO_IP, IP_MULTICAST_IF, &iface.ip, sizeof(iface.ip));
                sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                       reinterpret_cast<sockaddr*>(&mcastTarget), sizeof(mcastTarget));
            }

            // 3. Send directed broadcast to each local subnet (e.g. 192.168.1.255)
            for (const auto& iface : ifaces)
            {
                if (iface.hasBroadcast)
                {
                    sockaddr_in subBcast {};
                    subBcast.sin_family = AF_INET;
                    subBcast.sin_port = htons(beaconPort_);
                    subBcast.sin_addr = iface.broadcast;
                    sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                           reinterpret_cast<sockaddr*>(&subBcast), sizeof(subBcast));
                }
            }

            // 4. Send fallback 255.255.255.255
            sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                   reinterpret_cast<sockaddr*>(&bcastTarget), sizeof(bcastTarget));
        }

        pruneStalePeers();

        // Sleep 1 second in small increments so shutdown is prompt
        for (int i = 0; i < 20 && running_; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

void BeaconService::listenLoop()
{
    if (listenSocket_ < 0)
        return;

    while (running_)
    {
        pollfd pfd {};
        pfd.fd = listenSocket_;
        pfd.events = POLLIN;

        int res = poll(&pfd, 1, 200); // 200ms timeout
        if (res <= 0 || !(pfd.revents & POLLIN))
            continue;

        BeaconPacket packet;
        sockaddr_in senderAddr {};
        socklen_t addrLen = sizeof(senderAddr);

        ssize_t bytesRead = recvfrom(listenSocket_, &packet, sizeof(packet), 0,
                                     reinterpret_cast<sockaddr*>(&senderAddr), &addrLen);

        if (bytesRead == sizeof(BeaconPacket) && packet.magic == BEACON_MAGIC && packet.version == PROTOCOL_VER)
        {
            char ipBuf[INET_ADDRSTRLEN] = {0};
            inet_ntop(AF_INET, &senderAddr.sin_addr, ipBuf, sizeof(ipBuf));
            std::string senderIp(ipBuf);

            std::string peerUuid(packet.instanceUuid);
            // Ignore our own beacon
            {
                std::lock_guard<std::mutex> lock(beaconConfigMutex_);
                if (!peerUuid.empty() && peerUuid == localBeacon_.instanceUuid)
                    continue;
            }

            bool updated = false;
            {
                std::lock_guard<std::mutex> lock(peerMutex_);
                auto it = std::find_if(peers_.begin(), peers_.end(), [&](const DiscoveredPeer& p) {
                    return (!p.uuid.empty() && p.uuid == peerUuid) ||
                           (p.ipAddress == senderIp && p.audioPort == packet.audioPort);
                });

                uint64_t now = getCurrentTimeMs();
                if (it != peers_.end())
                {
                    it->instanceName = packet.instanceName;
                    it->hostName = packet.hostName;
                    it->ipAddress = senderIp;
                    it->audioPort = packet.audioPort;
                    it->role = packet.role;
                    it->numChannels = packet.numChannels;
                    it->sampleRate = packet.sampleRate;
                    it->streamName = packet.streamName;
                    it->lastSeenMs = now;
                }
                else
                {
                    DiscoveredPeer newPeer;
                    newPeer.uuid = peerUuid;
                    newPeer.instanceName = packet.instanceName;
                    newPeer.hostName = packet.hostName;
                    newPeer.ipAddress = senderIp;
                    newPeer.audioPort = packet.audioPort;
                    newPeer.role = packet.role;
                    newPeer.numChannels = packet.numChannels;
                    newPeer.sampleRate = packet.sampleRate;
                    newPeer.streamName = packet.streamName;
                    newPeer.lastSeenMs = now;
                    peers_.push_back(newPeer);
                    updated = true;
                }
            }

            if (updated && onPeersUpdated_)
            {
                auto currentPeers = getActivePeers();
                onPeersUpdated_(currentPeers);
            }
        }
    }
}

void BeaconService::pruneStalePeers()
{
    uint64_t now = getCurrentTimeMs();
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(peerMutex_);
        auto it = peers_.begin();
        while (it != peers_.end())
        {
            // Prune if no beacon heard in 4.5 seconds
            if (now > it->lastSeenMs && (now - it->lastSeenMs) > 4500)
            {
                it = peers_.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
    }

    if (changed && onPeersUpdated_)
    {
        auto currentPeers = getActivePeers();
        onPeersUpdated_(currentPeers);
    }
}

std::vector<DiscoveredPeer> BeaconService::getActivePeers() const
{
    std::lock_guard<std::mutex> lock(peerMutex_);
    return peers_;
}

void BeaconService::setOnPeersUpdated(PeerCallback callback)
{
    onPeersUpdated_ = std::move(callback);
}

} // namespace pluginbridge

