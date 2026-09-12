#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace pluginbridge
{

// Protocol constants
constexpr uint32_t AUDIO_MAGIC   = 0x504C4742; // 'PLGB'
constexpr uint32_t BEACON_MAGIC  = 0x50424443; // 'PBDC'
constexpr uint8_t  PROTOCOL_VER  = 1;

constexpr const char* DEFAULT_MULTICAST_GROUP = "239.255.77.88";
constexpr uint16_t    DEFAULT_BEACON_PORT     = 52800;
constexpr uint16_t    DEFAULT_AUDIO_PORT      = 52801;

constexpr int MAX_CHANNELS = 48;
constexpr int MAX_FRAMES_PER_PACKET = 256;
constexpr int MAX_PAYLOAD_BYTES = MAX_CHANNELS * MAX_FRAMES_PER_PACKET * sizeof(float);

#pragma pack(push, 1)

/** Header prepended to every UDP audio packet */
struct AudioPacketHeader
{
    uint32_t magic;            // AUDIO_MAGIC ('PLGB')
    uint8_t  version;          // PROTOCOL_VER (1)
    uint8_t  flags;            // Bit 0: Muted, Bit 1: ClockSync, Bit 2: StreamStart
    uint16_t numChannels;      // 1 to 32
    uint32_t sampleRate;       // 44100, 48000, 88200, 96000, 192000
    uint16_t numFrames;        // Frames per channel in this packet (e.g., 64, 128, 256)
    uint8_t  bitDepth;         // 32 (float), 24 (int24), 16 (int16)
    uint8_t  channelOffset;    // Starting channel offset (0 to 47)
    uint32_t sequenceNumber;   // Monotonically increasing packet sequence counter
    uint64_t timestampUs;      // Sender timestamp in microseconds
    char     streamId[32];     // Unique stream identifier
    char     streamName[32];   // Friendly stream name (e.g., "MixBus-1-2", "Drums")
    char     senderHost[32];   // Sender computer name (e.g., "MacBook-Pro", "Studio-PC")
};

constexpr uint8_t BEACON_ROLE_MASK   = 0x0F;
constexpr uint8_t BEACON_FLAG_LINK   = 0x10;
constexpr uint8_t BEACON_FLAG_UNLINK = 0x20;
constexpr uint8_t BEACON_FLAG_REPLY  = 0x40;

/** LAN discovery beacon packet broadcasted periodically */
struct BeaconPacket
{
    uint32_t magic;            // BEACON_MAGIC ('PBDC')
    uint8_t  version;          // PROTOCOL_VER
    uint8_t  role;             // 0: Matrix/Duplex, 1: Sender only, 2: Receiver only | flags
    uint16_t audioPort;        // UDP port receiver is listening on
    uint16_t numChannels;      // Number of audio channels supported
    uint32_t sampleRate;       // Current DAW sample rate
    char     instanceUuid[36]; // Unique instance identifier
    char     instanceName[32]; // User-configured instance name
    char     hostName[32];     // OS machine hostname
    char     streamName[32];   // Primary broadcast/send stream name
    uint64_t uptimeMs;         // Instance uptime in milliseconds
};

#pragma pack(pop)

static_assert(sizeof(AudioPacketHeader) == 124, "AudioPacketHeader unexpected size");
static_assert(sizeof(BeaconPacket) == 154, "BeaconPacket unexpected size");

/** Discovered LAN Peer information stored in memory */
struct DiscoveredPeer
{
    std::string uuid;
    std::string instanceName;
    std::string hostName;
    std::string ipAddress;
    uint16_t    audioPort { DEFAULT_AUDIO_PORT };
    uint8_t     role { 0 };
    uint16_t    numChannels { 2 };
    uint32_t    sampleRate { 48000 };
    std::string streamName;
    uint64_t    lastSeenMs { 0 };
    float       rttMs { 0.0f };
    bool        isConnected { false };
};

} // namespace pluginbridge
