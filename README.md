# PluginBridge - Low-Latency LAN Audio Bus & Patchbay

**PluginBridge** is a high-performance, real-time JUCE-based audio plugin and standalone application inspired by **Audiomovers OMNIBUS 3**. It allows you to route multi-channel audio between DAWs across **2 or more computers on a Local Area Network (LAN)** with ultra-low latency, zero configuration, and crystal-clear fidelity.

---

## Key Features

- **Multi-Computer LAN Audio Routing**:
  - Connect two or more DAWs (Ableton Live, Logic Pro, Pro Tools, Reaper, Cubase, Studio One, FL Studio, etc.) across physical computers via Ethernet or Wi-Fi.
  - Supports up to **48 input and 48 output channels** per instance (2,304 crosspoint matrix router) with simultaneous bidirectional streaming.
- **Zero-Config Auto-Discovery (mDNS / Multicast Beacons)**:
  - Plug-and-play operation using background UDP multicast beacons (`239.255.77.88:52800`).
  - Automatically identifies hostnames, instance aliases, IP addresses, listening ports, channel counts (up to 48ch), and active streams on the network.
  - Includes manual IP/port entry for enterprise routers that block multicast.
- **Low-Latency UDP Streaming Engine**:
  - Lock-free, real-time safe audio thread architecture (`juce::AbstractFifo` / ring buffers).
  - Configurable packet payload sizes (64, 128, 256 samples) enabling sub-5ms LAN transmission over 1GbE Ethernet.
  - Uncompressed 32-bit float bit-perfect transmission with generous 4MB socket buffers.
- **Adaptive Jitter Buffer & Hardware Clock Drift Compensation**:
  - Automatically compensates for unsynchronized physical audio hardware clocks between different computers (e.g. 48000.005 Hz vs 47999.992 Hz).
  - Employs a closed-loop PI controller tracking buffer occupancy coupled with Catmull-Rom cubic fractional resampling to smoothly adapt the sample rate with zero audible clicks, pitch jitter, or pops.
- **Direct Physical Audio Interface Channel Selection & Remapping**:
  - Remap any of the 48 matrix channels directly to physical audio interface hardware channels (e.g. `IN 01 [IF 17]`, `OUT 01 [IF 09]`).
  - Interactive row and column headers in the patchbay: click or right-click any channel header to directly select its audio interface channel from grouped bank menus (1-16, 17-32, 33-48).
  - Quick sequential and bank offset presets (`Offset +8`, `Offset +16`, `Bank 1 -> IF Ch 17-32`, etc.).
  - Dedicated **AUDIO IF MAP** button and interactive 48-channel modal dialog for remapping all hardware channels at once.
  - Dedicated **AUDIO I/O** button in the header displaying host sample rate, buffer size, discrete 48x48 bus topology, and direct access to Standalone soundcard settings.
- **Audiomovers OMNIBUS 3-Style Interactive Routing Matrix (48 Channels)**:
  - Clickable crosspoint matrix patchbay connecting DAW Inputs, DAW Outputs, LAN Transmit channels, and LAN Receive streams.
  - **Bank Navigation & All-48 Overview**: Switch between `1-16`, `17-32`, `33-48`, and `ALL 48` master matrix views with live crosshair guide beams and hover tooltips.
  - Clean, high-compatibility ASCII UI rendering with zero font glyph corruptions or broken boxes.
  - Real-time LED VU peak meters for all 48 channels with dB scale, peak hold indicators, and clip LEDs.
  - Quick action routing buttons (1:1 Patch across 48 channels, Stereo 1-2, Clear All, Thru monitor).
- **Diagnostics Dashboard**:
  - Live readout of LAN TX/RX bitrates (Mbps), packet loss percentage, jitter buffer depth, and hardware clock drift ratio.

---

## Architecture Overview

```
 Computer 1 (Control Room Mac)             Computer 2 (Synth / Mix PC)
┌──────────────────────────────┐          ┌──────────────────────────────┐
│  DAW (Ableton / Logic Pro)   │          │   DAW (Reaper / Cubase)      │
│  Outputs 1-16   Inputs 1-16  │          │  Outputs 1-16   Inputs 1-16  │
└──────┬───────────────▲───────┘          └──────┬───────────────▲───────┘
       │               │                         │               │
┌──────▼───────────────┴───────┐          ┌──────▼───────────────┴───────┐
│      PluginBridge (AU/VST3)  │          │      PluginBridge (VST3)     │
│  ┌────────────────────────┐  │          │  ┌────────────────────────┐  │
│  │ Interactive Patchbay   │  │          │  │ Interactive Patchbay   │  │
│  └───────┬────────▲───────┘  │          │  └───────┬────────▲───────┘  │
│          │        │          │          │          │        │          │
│       TX FIFO   RX FIFO      │          │       TX FIFO   RX FIFO      │
│          │        │          │          │          │        │          │
│       UDP TX    Jitter+Drift │          │       UDP TX    Jitter+Drift │
└──────────┼────────▲──────────┘          └──────────┼────────▲──────────┘
           │        │                                │        │
           ▼        │           LOCAL AREA NETWORK   ▼        │
           ═════════╧════════════════════════════════╧════════
                            Low-Latency UDP Packets
```

---

## Building the Project

### Prerequisites
- macOS (Apple Silicon or Intel) or Windows
- C++20 compatible compiler (Apple Clang / GCC / MSVC)
- CMake 3.22+ and Ninja (or Xcode / Visual Studio)

### Build Commands (macOS)
```bash
# Configure with Ninja and Release mode
.tools/cmake/data/bin/cmake -B build -G Ninja \
    -DCMAKE_MAKE_PROGRAM=.tools/bin/ninja \
    -DCMAKE_BUILD_TYPE=Release

# Build all targets (Standalone, VST3, AU, and test harness)
.tools/bin/ninja -C build
```

The built binaries will be located in:
- **Standalone Application**: `build/PluginBridge_artefacts/Release/Standalone/PluginBridge.app`
- **VST3 Plugin**: `build/PluginBridge_artefacts/Release/VST3/PluginBridge.vst3`
- **AU Component**: `build/PluginBridge_artefacts/Release/AU/PluginBridge.component`
- **Test Suite**: `build/test_harness`

### Installing the Plugins on macOS
To make the plugins available to all DAWs on your system:
```bash
# Copy VST3 plugin
cp -R build/PluginBridge_artefacts/Release/VST3/PluginBridge.vst3 ~/Library/Audio/Plug-Ins/VST3/

# Copy Audio Unit (AU) component
cp -R build/PluginBridge_artefacts/Release/AU/PluginBridge.component ~/Library/Audio/Plug-Ins/Components/
```

---

## How to Route Audio Between 2 Computers

1. **Insert PluginBridge**:
   - Insert `PluginBridge` onto a track, master bus, or auxiliary send in your DAW on **Computer 1**.
   - Insert `PluginBridge` on a track in your DAW on **Computer 2**.
2. **Auto-Discovery**:
   - Both computers on the same LAN will immediately detect each other and show up in the **Discovered LAN Peers** list on the left panel.
3. **Connect**:
   - Click the green **Connect** button next to the remote computer in the peer browser.
4. **Patch the Channels (Matrix Router)**:
   - On **Computer 1**: Select the **DAW IN ➔ LAN TX** tab, then click the matrix dots to route DAW channels (e.g. channels 1 & 2 or 1 through 8) to LAN Transmit channels.
   - On **Computer 2**: Select the **LAN RX ➔ DAW OUT** tab, then route the incoming LAN Receive channels into your DAW output tracks.
5. **Bidirectional Streaming**:
   - Both machines can simultaneously transmit and receive audio in real time with sub-5ms latency and full clock drift synchronization.

