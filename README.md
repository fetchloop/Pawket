# Pawket 📦

Pawket is a raw-socket packet sniffer for Windows with a live ImGui frontend. It captures traffic at the IP layer, parses it by protocol, and lets you dig through payloads byte by byte. Small, portable and direct!

---

## Requirements

- Windows, run as **Administrator**, raw sockets won't work without it!
- C++20 compiler (written and tested on MSVC 2026)
- Direct3D 11

## Building

MSVC picks up the required libs automatically via `#pragma comment`:
`ws2_32`, `d3d11`, `dxgi`, `d3dcompiler`

---

## How it works

On startup, Pawket binds a raw socket to your machine's local IP and flips on promiscuous mode via `SIO_RCVALL`. A background thread sits in a `select` loop, wakes up when there's data, and hands each buffer off to a small parser that reads the IP header and figures out what to do with it.

Packets land in a mutex-guarded list. The GUI drains new entries each frame into its own local copy.

### Supported protocols

| Protocol | Ports | Notes |
|---|---|---|
| TCP | ✅ | Full port + payload extraction |
| UDP | ✅ | Full port + payload extraction |
| ICMP | — | No ports, full payload captured |
| IGMP | — | No ports, full payload captured |
| SCTP | ✅ | Ports read manually from the header |
| Other | — | Captured as-is, no port info |

---

## GUI

The render loop is Dear ImGui over DirectX 11. The main view is a live packet table, click a row to open a hex + ASCII dump of the payload in a resizable inspector panel below. The splitter is draggable.

- **Search bar** : filter by IP, port, protocol, or direction
- **Column headers** : click Protocol to cycle a protocol filter, click Direction to toggle sort
- **Stop / Start Recording** : pause and resume capture; starting clears the current list
- **Config panel** : adjust filter, max packets, and debug mode without touching the config file
- **Export panel** : export the current packet list to PCAP or JSON
- **Import PCAP** : load a previously exported PCAP file back into the viewer

---

## Export & Import

### Export PCAP
Writes a standard `.pcap` file (microsecond resolution, `LINKTYPE_RAW`) to `%APPDATA%\Pawket\Exports\`. The filename is timestamped automatically. The resulting file can be opened in Wireshark or any other tool that understands the PCAP format.

### Export JSON
Writes a JSON array to `%APPDATA%\Pawket\Exports\`, one object per packet. Each entry includes:

| Field | Description |
|---|---|
| `timestamp` | Human-readable capture time |
| `protocol` | TCP / UDP / ICMP / IGMP / SCTP / OTHER |
| `source_ip` | Source IP address |
| `source_port` | Source port (0 for protocols with no port) |
| `destination_ip` | Destination IP address |
| `destination_port` | Destination port (0 for protocols with no port) |
| `direction` | `incoming` or `outgoing` |
| `payload` | Hex dump of the payload bytes |

### Import PCAP
Opens a file picker filtered to `.pcap` files. Pawket validates the global header magic and link type (`LINKTYPE_RAW` only), then re-parses each record's IP header to reconstruct protocol, ports, direction, and payload — the same fields that live capture produces. Imported packets respect the `MAX_PACKETS` cap.

---

## Config

Saved to `%APPDATA%\Pawket\config.cfg` and tweakable at runtime from the Config panel in the GUI:

| Option | Default | Notes |
|---|---|---|
| `filter` | `ANY` | Lock to `INCOMING` or `OUTGOING` if you only care about one direction |
| `MAX_PACKETS` | `100` | Once the cap is hit, oldest packets get dropped to make room |
| `debug` | `false` | Spits packets and hex dumps to stdout |

---

## Project structure

```
pawket.cpp         > Entry point, elevation check, init & cleanup
handler.h          > Socket setup, SIO_RCVALL, capture thread
gui.h              > ImGui/DX11 render loop, packet table, hex inspector
util/
  config.h         > Config load/save, FilterType enum
  packet.h         > PACKET struct, Protocol/Direction/Endpoint types
  list.h           > Shared packet list + mutex
  ip_util.h        > Local IP resolution
  ip_header.h      > IP, TCP, UDP header structs
  io.h             > PCAP/JSON export and PCAP import
  elevation.h      > UAC elevation check and relaunch
```

---

## Current limitations

- IPv6 isn't supported
- No application-layer parsing (HTTP, DNS, etc.)
- PCAP import only accepts `LINKTYPE_RAW` (link type 101); captures from Wireshark using Ethernet encapsulation won't load

---

*We have Wireshark at home -fetch*
