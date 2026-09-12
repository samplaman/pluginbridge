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
    uint64_t iterationCount = 0;

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

            // 5. Send direct unicast beacons to explicitly configured peers AND all discovered peers
            // This guarantees connectivity even when Wi-Fi routers block multicast/broadcast!
            {
                std::vector<UnicastTarget> allTargets;
                {
                    std::lock_guard<std::mutex> lock(unicastMutex_);
                    allTargets = unicastTargets_;
                }
                {
                    std::lock_guard<std::mutex> lock(peerMutex_);
                    for (const auto& peer : peers_)
                    {
                        if (!peer.ipAddress.empty())
                        {
                            bool alreadyIn = false;
                            for (const auto& t : allTargets)
                            {
                                if (t.ip == peer.ipAddress && t.port == beaconPort_)
                                {
                                    alreadyIn = true;
                                    break;
                                }
                            }
                            if (!alreadyIn)
                                allTargets.push_back({ peer.ipAddress, beaconPort_ });
                        }
                    }
                }

                for (const auto& target : allTargets)
                {
                    sockaddr_in directTarget {};
                    directTarget.sin_family = AF_INET;
                    directTarget.sin_port = htons(target.port);
                    directTarget.sin_addr.s_addr = inet_addr(target.ip.c_str());
                    sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                           reinterpret_cast<sockaddr*>(&directTarget), sizeof(directTarget));
                }
            }

            // 6. Subnet sweep: sweep on startup, every 4s if no peers discovered, or every 30s as maintenance
            bool hasPeers = false;
            {
                std::lock_guard<std::mutex> lock(peerMutex_);
                hasPeers = !peers_.empty();
            }

            if (iterationCount == 0 || (!hasPeers && (iterationCount % 4 == 0)) || (hasPeers && (iterationCount % 30 == 0)))
            {
                scanSubnet();
            }
            iterationCount++;
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

            // Immediately send our beacon directly back to the sender so discovery is instant and two-way.
            // Crucial: send to beaconPort_ (52800), NOT the sender's ephemeral port in senderAddr.sin_port!
            // Do not reply if the packet itself is already a reply (BEACON_FLAG_REPLY), preventing feedback storms.
            bool isReply = (packet.role & BEACON_FLAG_REPLY) != 0;
            if (!isReply && sendSocket_ >= 0)
            {
                uint64_t now = getCurrentTimeMs();
                bool shouldReply = true;
                {
                    std::lock_guard<std::mutex> lock(replyRateMutex_);
                    auto it = lastReplyTimeByIp_.find(senderIp);
                    if (it != lastReplyTimeByIp_.end() && (now - it->second) < 400)
                    {
                        shouldReply = false;
                    }
                    else
                    {
                        lastReplyTimeByIp_[senderIp] = now;
                    }
                }

                if (shouldReply)
                {
                    BeaconPacket replyPacket;
                    {
                        std::lock_guard<std::mutex> lock(beaconConfigMutex_);
                        replyPacket = localBeacon_;
                    }
                    replyPacket.role = (replyPacket.role & BEACON_ROLE_MASK) | BEACON_FLAG_REPLY;

                    sockaddr_in replyAddr = senderAddr;
                    replyAddr.sin_family = AF_INET;
                    replyAddr.sin_port = htons(beaconPort_);
                    sendto(sendSocket_, &replyPacket, sizeof(replyPacket), 0,
                           reinterpret_cast<sockaddr*>(&replyAddr), sizeof(replyAddr));
                }
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

            uint8_t flag = packet.role & (BEACON_FLAG_LINK | BEACON_FLAG_UNLINK);
            if (flag != 0 && onLinkCommand_)
            {
                DiscoveredPeer cmdPeer;
                cmdPeer.uuid = peerUuid;
                cmdPeer.instanceName = packet.instanceName;
                cmdPeer.hostName = packet.hostName;
                cmdPeer.ipAddress = senderIp;
                cmdPeer.audioPort = packet.audioPort;
                cmdPeer.numChannels = packet.numChannels;
                cmdPeer.sampleRate = packet.sampleRate;
                onLinkCommand_(cmdPeer, (flag & BEACON_FLAG_LINK) != 0);
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

void BeaconService::setOnLinkCommand(LinkCommandCallback callback)
{
    onLinkCommand_ = std::move(callback);
}

void BeaconService::sendLinkCommand(const std::string& ipAddress, uint16_t beaconPort, bool connect)
{
    if (sendSocket_ < 0 || ipAddress.empty())
        return;

    BeaconPacket packetCopy;
    {
        std::lock_guard<std::mutex> lock(beaconConfigMutex_);
        packetCopy = localBeacon_;
    }
    packetCopy.role = (packetCopy.role & BEACON_ROLE_MASK) | (connect ? BEACON_FLAG_LINK : BEACON_FLAG_UNLINK);

    sockaddr_in directTarget {};
    directTarget.sin_family = AF_INET;
    directTarget.sin_port = htons(beaconPort);
    directTarget.sin_addr.s_addr = inet_addr(ipAddress.c_str());
    sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
           reinterpret_cast<sockaddr*>(&directTarget), sizeof(directTarget));
}

void BeaconService::pingPeer(const std::string& ipAddress, uint16_t beaconPort)
{
    if (sendSocket_ < 0 || ipAddress.empty())
        return;

    BeaconPacket packetCopy;
    {
        std::lock_guard<std::mutex> lock(beaconConfigMutex_);
        packetCopy = localBeacon_;
    }

    sockaddr_in directTarget {};
    directTarget.sin_family = AF_INET;
    directTarget.sin_port = htons(beaconPort);
    directTarget.sin_addr.s_addr = inet_addr(ipAddress.c_str());
    sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
           reinterpret_cast<sockaddr*>(&directTarget), sizeof(directTarget));
}

void BeaconService::addUnicastTarget(const std::string& ipAddress, uint16_t beaconPort)
{
    if (ipAddress.empty())
        return;

    {
        std::lock_guard<std::mutex> lock(unicastMutex_);
        for (const auto& t : unicastTargets_)
        {
            if (t.ip == ipAddress && t.port == beaconPort)
                return;
        }
        unicastTargets_.push_back({ ipAddress, beaconPort });
    }

    // Ping immediately
    pingPeer(ipAddress, beaconPort);
}

void BeaconService::removeUnicastTarget(const std::string& ipAddress, uint16_t beaconPort)
{
    std::lock_guard<std::mutex> lock(unicastMutex_);
    unicastTargets_.erase(std::remove_if(unicastTargets_.begin(), unicastTargets_.end(),
        [&](const UnicastTarget& t) {
            return t.ip == ipAddress && t.port == beaconPort;
        }), unicastTargets_.end());
}

void BeaconService::scanSubnet()
{
    if (sendSocket_ < 0)
        return;

    BeaconPacket packetCopy;
    {
        std::lock_guard<std::mutex> lock(beaconConfigMutex_);
        packetCopy = localBeacon_;
    }

    auto ifaces = getLocalInterfaces();
    for (const auto& iface : ifaces)
    {
        uint32_t ipHost = ntohl(iface.ip.s_addr);
        uint32_t netPrefix = ipHost & 0xFFFFFF00; // /24 subnet

        for (uint32_t host = 1; host < 255; ++host)
        {
            uint32_t targetIp = netPrefix | host;
            if (targetIp == ipHost)
                continue;

            sockaddr_in directTarget {};
            directTarget.sin_family = AF_INET;
            directTarget.sin_port = htons(beaconPort_);
            directTarget.sin_addr.s_addr = htonl(targetIp);
            sendto(sendSocket_, &packetCopy, sizeof(packetCopy), 0,
                   reinterpret_cast<sockaddr*>(&directTarget), sizeof(directTarget));
        }
    }
}

} // namespace pluginbridge

