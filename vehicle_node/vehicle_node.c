/**
 * @file    vehicle_node.c
 * @brief   Emergency Vehicle Alert System — Vehicle Node Firmware
 * @author  Sameeksha R
 *
 * Runs on each emergency vehicle (ambulance, fire engine, police).
 * Detects siren/emergency activation, broadcasts a structured alert
 * frame over the CAN bus at 500 kbps.
 *
 * CAN Frame Format (11-bit ID):
 *   ID  0x100 — Emergency Alert
 *   DLC 8 bytes:
 *     [0]   Vehicle type  (0x01=Ambulance, 0x02=Fire, 0x03=Police)
 *     [1]   Priority      (0x01=High, 0x02=Medium)
 *     [2-3] Vehicle ID    (uint16_t, little-endian)
 *     [4-5] Speed km/h    (uint16_t, little-endian, x10 fixed-point)
 *     [6]   Direction     (0=North,1=South,2=East,3=West)
 *     [7]   Checksum      (XOR bytes 0–6)
 *
 * Hardware: STM32F103 (or any CAN-capable MCU)
 * CAN transceiver: MCP2551 or SN65HVD230
 */
 
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "can_driver.h"
#include "gps_driver.h"
 
/* ── Vehicle configuration ──────────────────────────────────────────── */
#define VEHICLE_TYPE        0x01        /* Ambulance */
#define VEHICLE_ID          0x0042      /* Unique fleet ID */
#define VEHICLE_PRIORITY    0x01        /* High */
#define CAN_ALERT_ID        0x100       /* Emergency alert CAN ID */
#define BROADCAST_INTERVAL_MS  200      /* 5 Hz broadcast when active */
#define SIREN_PIN           5           /* GPIO pin — siren active-high */
 
/* ── Vehicle direction from GPS heading (degrees → enum) ────────────── */
typedef enum {
    DIR_NORTH = 0,
    DIR_SOUTH = 1,
    DIR_EAST  = 2,
    DIR_WEST  = 3,
} direction_t;
 
static direction_t heading_to_direction(float heading_deg)
{
    if (heading_deg >= 315.0f || heading_deg < 45.0f)  return DIR_NORTH;
    if (heading_deg >= 45.0f  && heading_deg < 135.0f) return DIR_EAST;
    if (heading_deg >= 135.0f && heading_deg < 225.0f) return DIR_SOUTH;
    return DIR_WEST;
}
 
/* ── Build CAN alert frame ──────────────────────────────────────────── */
static void build_alert_frame(can_frame_t *frame,
                               float speed_kmph,
                               float heading_deg)
{
    uint16_t speed_fixed = (uint16_t)(speed_kmph * 10.0f);
    direction_t dir = heading_to_direction(heading_deg);
 
    frame->id  = CAN_ALERT_ID;
    frame->dlc = 8;
 
    frame->data[0] = VEHICLE_TYPE;
    frame->data[1] = VEHICLE_PRIORITY;
    frame->data[2] = (VEHICLE_ID >> 0) & 0xFF;   /* LSB */
    frame->data[3] = (VEHICLE_ID >> 8) & 0xFF;   /* MSB */
    frame->data[4] = (speed_fixed >> 0) & 0xFF;
    frame->data[5] = (speed_fixed >> 8) & 0xFF;
    frame->data[6] = (uint8_t)dir;
 
    /* XOR checksum over bytes 0–6 */
    uint8_t chk = 0;
    for (int i = 0; i < 7; i++) chk ^= frame->data[i];
    frame->data[7] = chk;
}
 
/* ── Main ───────────────────────────────────────────────────────────── */
int main(void)
{
    printf("=== Emergency Vehicle Node — Sameeksha R ===\n");
    printf("Vehicle Type: 0x%02X  ID: 0x%04X\n\n", VEHICLE_TYPE, VEHICLE_ID);
 
    /* Init peripherals */
    can_init(CAN_BAUD_500K);
    gps_init();
    gpio_input_init(SIREN_PIN);
 
    can_frame_t frame;
    gps_data_t  gps;
    uint32_t    last_tx = 0;
 
    while (1) {
        uint32_t now = millis();
 
        /* Read GPS */
        gps_read(&gps);
 
        /* Only broadcast when siren is active */
        bool siren_active = gpio_read(SIREN_PIN);
 
        if (siren_active && (now - last_tx) >= BROADCAST_INTERVAL_MS) {
            build_alert_frame(&frame, gps.speed_kmph, gps.heading_deg);
 
            if (can_transmit(&frame) == CAN_OK) {
                printf("[CAN TX] Alert ID=0x%03X spd=%.1fkm/h dir=%d chk=0x%02X\n",
                       frame.id, gps.speed_kmph,
                       frame.data[6], frame.data[7]);
            } else {
                printf("[CAN TX] Transmit failed — retrying\n");
            }
            last_tx = now;
        }
 
        delay_ms(10);
    }
 
    return 0;
}
