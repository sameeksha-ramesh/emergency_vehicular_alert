/**
 * @file    gateway.c
 * @brief   Emergency Vehicle Alert System — Roadside CAN-to-Ethernet Gateway
 * @author  Sameeksha R
 *
 * The roadside gateway unit:
 *  1. Listens on CAN bus for emergency alert frames (ID 0x100)
 *  2. Validates frame checksum
 *  3. Decodes payload (vehicle type, ID, speed, direction)
 *  4. Forwards structured JSON alert to the Traffic Management Server
 *     over Ethernet via TCP socket
 *
 * Hardware: Raspberry Pi / STM32 + W5500 Ethernet shield
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "can_driver.h"
 
/* ── Configuration ──────────────────────────────────────────────────── */
#define SERVER_IP           "192.168.1.200"   /* Traffic management server */
#define SERVER_PORT         9090
#define CAN_ALERT_ID        0x100
#define GATEWAY_ID          "GW_NODE_01"      /* Unique gateway identifier */
#define RECONNECT_DELAY_S   5
 
/* ── Vehicle type lookup ─────────────────────────────────────────────── */
static const char* vehicle_type_str(uint8_t type)
{
    switch (type) {
        case 0x01: return "AMBULANCE";
        case 0x02: return "FIRE_ENGINE";
        case 0x03: return "POLICE";
        default:   return "UNKNOWN";
    }
}
 
static const char* direction_str(uint8_t dir)
{
    switch (dir) {
        case 0: return "NORTH";
        case 1: return "SOUTH";
        case 2: return "EAST";
        case 3: return "WEST";
        default: return "UNKNOWN";
    }
}
 
/* ── Checksum validation ─────────────────────────────────────────────── */
static bool validate_checksum(const can_frame_t *frame)
{
    if (frame->dlc != 8) return false;
    uint8_t chk = 0;
    for (int i = 0; i < 7; i++) chk ^= frame->data[i];
    return chk == frame->data[7];
}
 
/* ── Decode CAN frame → JSON string ────────────────────────────────── */
static int decode_to_json(const can_frame_t *frame, char *buf, int maxlen)
{
    uint8_t  vtype    = frame->data[0];
    uint8_t  priority = frame->data[1];
    uint16_t vid      = (frame->data[3] << 8) | frame->data[2];
    uint16_t spd_f    = (frame->data[5] << 8) | frame->data[4];
    float    speed    = spd_f / 10.0f;
    uint8_t  dir      = frame->data[6];
 
    time_t now = time(NULL);
 
    return snprintf(buf, maxlen,
        "{"
        "\"gateway\":\"%s\","
        "\"timestamp\":%ld,"
        "\"vehicle_type\":\"%s\","
        "\"vehicle_id\":%u,"
        "\"priority\":%u,"
        "\"speed_kmph\":%.1f,"
        "\"direction\":\"%s\","
        "\"can_id\":\"0x%03X\""
        "}",
        GATEWAY_ID,
        (long)now,
        vehicle_type_str(vtype),
        vid,
        priority,
        speed,
        direction_str(dir),
        frame->id
    );
}
 
/* ── TCP connection to traffic server ──────────────────────────────── */
static int connect_to_server(void)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("[GW] socket"); return -1; }
 
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
 
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[GW] connect");
        close(sock);
        return -1;
    }
 
    printf("[GW] Connected to Traffic Server %s:%d\n", SERVER_IP, SERVER_PORT);
    return sock;
}
 
/* ── Main gateway loop ──────────────────────────────────────────────── */
int main(void)
{
    printf("=== Roadside CAN-Ethernet Gateway — Sameeksha R ===\n");
    printf("CAN Alert ID : 0x%03X\n", CAN_ALERT_ID);
    printf("Server       : %s:%d\n\n", SERVER_IP, SERVER_PORT);
 
    can_init(CAN_BAUD_500K);
 
    int    sock    = -1;
    char   json[512];
    char   msg[600];
 
    while (1) {
        /* Maintain TCP connection */
        if (sock < 0) {
            sock = connect_to_server();
            if (sock < 0) {
                printf("[GW] Retrying in %ds...\n", RECONNECT_DELAY_S);
                sleep(RECONNECT_DELAY_S);
                continue;
            }
        }
 
        /* Listen for CAN frames */
        can_frame_t frame;
        if (can_receive(&frame, 100) != CAN_OK) continue;
 
        /* Filter for emergency alert ID only */
        if (frame.id != CAN_ALERT_ID) continue;
 
        /* Validate checksum */
        if (!validate_checksum(&frame)) {
            printf("[GW] Checksum FAIL — frame dropped\n");
            continue;
        }
 
        /* Decode to JSON */
        decode_to_json(&frame, json, sizeof(json));
 
        /* Wrap in simple framing protocol: length-prefixed */
        int jlen = strlen(json);
        snprintf(msg, sizeof(msg), "%04d%s", jlen, json);
 
        /* Forward to traffic server over Ethernet */
        ssize_t sent = send(sock, msg, strlen(msg), 0);
        if (sent < 0) {
            perror("[GW] send");
            close(sock);
            sock = -1;
            continue;
        }
 
        printf("[GW] Forwarded: %s\n", json);
    }
 
    close(sock);
    return 0;
}
