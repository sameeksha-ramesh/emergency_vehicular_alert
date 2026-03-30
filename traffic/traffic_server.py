"""
traffic_server.py — Emergency Vehicle Alert System: Traffic Management Server
Author : Sameeksha R
 
Receives structured JSON alerts from roadside CAN-Ethernet gateways.
Processes alerts, controls traffic signals, and logs all events.
 
Architecture:
    Vehicle (CAN) → Gateway (Ethernet/TCP) → THIS SERVER → Signal Control
 
Run:
    pip install flask colorama
    python traffic_server.py
"""
 
import socket
import threading
import json
import csv
import os
from datetime import datetime
from colorama import Fore, Style, init
 
init(autoreset=True)
 
# ── Configuration ──────────────────────────────────────────────────────
TCP_HOST       = "0.0.0.0"
TCP_PORT       = 9090
LOG_FILE       = "traffic_alerts.csv"
SIGNAL_TIMEOUT = 30     # seconds to hold green for emergency vehicle
 
# ── Priority handling ──────────────────────────────────────────────────
VEHICLE_PRIORITY = {
    "AMBULANCE":   1,
    "FIRE_ENGINE": 1,
    "POLICE":      2,
}
 
# ── Signal state (simplified — replace with real GPIO/API calls) ───────
signal_state = {}   # gateway_id → {"color": "GREEN", "until": timestamp}
 
# ── Logging ────────────────────────────────────────────────────────────
def init_log():
    if not os.path.exists(LOG_FILE):
        with open(LOG_FILE, 'w', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([
                "server_time", "gateway", "vehicle_type",
                "vehicle_id", "speed_kmph", "direction",
                "priority", "signal_action"
            ])
 
def log_event(data: dict, action: str):
    with open(LOG_FILE, 'a', newline='') as f:
        writer = csv.writer(f)
        writer.writerow([
            datetime.now().isoformat(),
            data.get("gateway", "?"),
            data.get("vehicle_type", "?"),
            data.get("vehicle_id", "?"),
            data.get("speed_kmph", "?"),
            data.get("direction", "?"),
            data.get("priority", "?"),
            action,
        ])
 
# ── Signal control ─────────────────────────────────────────────────────
def control_signal(gateway_id: str, vehicle_type: str, direction: str):
    """
    Grant green light clearance for incoming emergency vehicle.
    In production: call traffic controller API or assert GPIO.
    """
    signal_state[gateway_id] = {
        "color":     "GREEN",
        "direction": direction,
        "until":     datetime.now().timestamp() + SIGNAL_TIMEOUT,
    }
    print(Fore.GREEN +
          f"  🚦 SIGNAL OVERRIDE: {gateway_id} → GREEN for {direction} "
          f"({SIGNAL_TIMEOUT}s) [{vehicle_type}]")
 
# ── Process incoming alert ─────────────────────────────────────────────
def process_alert(raw_json: str):
    try:
        data = json.loads(raw_json)
    except json.JSONDecodeError as e:
        print(Fore.RED + f"[SERVER] JSON parse error: {e}")
        return
 
    vtype   = data.get("vehicle_type", "UNKNOWN")
    vid     = data.get("vehicle_id", "?")
    speed   = data.get("speed_kmph", 0)
    dirn    = data.get("direction", "?")
    gateway = data.get("gateway", "?")
    prio    = VEHICLE_PRIORITY.get(vtype, 3)
 
    tag = Fore.RED + "🚨 PRIORITY-1" if prio == 1 else Fore.YELLOW + "⚠️  PRIORITY-2"
 
    print(f"\n{'='*60}")
    print(f"  {tag}{Style.RESET_ALL}  |  {datetime.now().strftime('%H:%M:%S.%f')}")
    print(f"  Vehicle   : {vtype}  (ID: {vid})")
    print(f"  Speed     : {speed:.1f} km/h  →  {dirn}")
    print(f"  Gateway   : {gateway}")
    print(f"{'='*60}")
 
    # Trigger signal override
    control_signal(gateway, vtype, dirn)
    log_event(data, f"SIGNAL_GREEN_{dirn}")
 
# ── Handle one gateway connection ──────────────────────────────────────
def handle_gateway(conn: socket.socket, addr):
    print(f"[SERVER] Gateway connected: {addr}")
    buffer = ""
 
    try:
        while True:
            chunk = conn.recv(1024).decode("utf-8", errors="ignore")
            if not chunk:
                break
            buffer += chunk
 
            # Length-prefixed framing: 4-digit length prefix + JSON body
            while len(buffer) >= 4:
                try:
                    msg_len = int(buffer[:4])
                except ValueError:
                    buffer = ""
                    break
 
                if len(buffer) < 4 + msg_len:
                    break   # wait for more data
 
                json_str = buffer[4:4 + msg_len]
                buffer   = buffer[4 + msg_len:]
                process_alert(json_str)
 
    except Exception as e:
        print(Fore.RED + f"[SERVER] Connection error: {e}")
    finally:
        conn.close()
        print(f"[SERVER] Gateway disconnected: {addr}")
 
# ── TCP server ─────────────────────────────────────────────────────────
def run_server():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((TCP_HOST, TCP_PORT))
    srv.listen(10)
 
    print(f"[SERVER] Listening on {TCP_HOST}:{TCP_PORT}")
 
    while True:
        conn, addr = srv.accept()
        t = threading.Thread(target=handle_gateway, args=(conn, addr), daemon=True)
        t.start()
 
# ── Main ───────────────────────────────────────────────────────────────
if __name__ == "__main__":
    init_log()
    print("=== Traffic Management Server — Sameeksha R ===")
    print(f"TCP Port : {TCP_PORT}  |  Log: {LOG_FILE}\n")
    run_server()
