# emergency_vehicular_alert
Emergency Vehicle Alert System
Author: Sameeksha R
Year: 2025
Tech: CAN Bus | Ethernet | TCP | Embedded C | Python

Overview
End-to-end emergency vehicle priority system. Vehicles broadcast alerts over CAN bus → roadside gateway relays over Ethernet → traffic management server grants green light clearance automatically.

System Architecture
┌─────────────────┐     CAN Bus      ┌─────────────────┐
│  Ambulance Node │ ──── 0x100 ────► │ Roadside Gateway│
│  (vehicle_node) │                  │  (gateway.c)    │
└─────────────────┘                  └────────┬────────┘
                                              │ Ethernet/TCP
                                              ▼
                                     ┌─────────────────┐
                                     │ Traffic Server  │
                                     │(traffic_server) │
                                     │  🚦 Signal Ctrl │
                                     └─────────────────┘

Repository Structure
├── vehicle_node/
│   ├── vehicle_node.c    — CAN broadcast firmware (runs on vehicle MCU)
│   └── can_driver.h      — CAN bus driver interface
│
├── gateway/
│   └── gateway.c         — Roadside CAN→Ethernet relay (Linux/RPi)
│
└── traffic_server/
    └── traffic_server.py — Central traffic management server (Python)

CAN Frame Format
ByteFieldValues0Vehicle type0x01=Ambulance, 0x02=Fire, 0x03=Police1Priority0x01=High, 0x02=Medium2-3Vehicle IDuint16_t LE4-5Speed (×10)uint16_t LE (e.g. 456 = 45.6 km/h)6Direction0=N, 1=S, 2=E, 3=W7ChecksumXOR of bytes 0–6
CAN ID: 0x100 | Baud rate: 500 kbps | Broadcast rate: 5 Hz

JSON Alert (Gateway → Server)
json{
  "gateway": "GW_NODE_01",
  "timestamp": 1711800000,
  "vehicle_type": "AMBULANCE",
  "vehicle_id": 66,
  "priority": 1,
  "speed_kmph": 78.3,
  "direction": "NORTH",
  "can_id": "0x100"
}

Build & Run
Vehicle Node (C)
gcc vehicle_node/vehicle_node.c -o vehicle_node
./vehicle_node

Gateway (C)
gcc gateway/gateway.c -o gateway
./gateway

Traffic Server (Python)
pip install colorama
python traffic_server/traffic_server.py
