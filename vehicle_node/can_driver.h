/**
 * @file    can_driver.h
 * @brief   CAN bus driver interface — Emergency Vehicle Alert System
 * @author  Sameeksha R
 */
 
#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H
 
#include <stdint.h>
 
#define CAN_OK       0
#define CAN_ERR     -1
#define CAN_BAUD_500K  500000
 
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} can_frame_t;
 
int  can_init(uint32_t baud_rate);
int  can_transmit(const can_frame_t *frame);
int  can_receive(can_frame_t *frame, uint32_t timeout_ms);
void gpio_input_init(int pin);
int  gpio_read(int pin);
void delay_ms(uint32_t ms);
uint32_t millis(void);
 
#endif /* CAN_DRIVER_H */
