/* Shared GPIO bit-bang backend for Intel 8080 parallel display buses.
 *
 * Used on ports without dedicated I80 hardware (e.g. SAMD51). Accelerated
 * ports (esp32, rp2 PIO, mimxrt FlexIO) keep their own mod_i80bus.c.
 */

#ifndef DISPLAYIF_I80BUS_GPIO_H
#define DISPLAYIF_I80BUS_GPIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DISPLAYIF_I80BUS_GPIO_WIDTH 8
#define DISPLAYIF_I80BUS_GPIO_MAX_GROUPS 4

typedef struct _displayif_i80bus_gpio_port_t {
    volatile uint32_t *outset;
    volatile uint32_t *outclr;
    uint32_t pin_mask;
} displayif_i80bus_gpio_port_t;

typedef struct _displayif_i80bus_gpio_t {
    displayif_i80bus_gpio_port_t wr;
    bool sequential;
    uint8_t num_groups;
    uint8_t lut_group[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS];
    displayif_i80bus_gpio_port_t seq;
    uint8_t seq_shift;
    displayif_i80bus_gpio_port_t lut_port[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS];
    uint32_t lut_table[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS][256];
} displayif_i80bus_gpio_t;

void displayif_i80bus_gpio_bind_port(displayif_i80bus_gpio_port_t *port,
    volatile uint32_t *outset, volatile uint32_t *outclr, uint32_t pin_mask);

void displayif_i80bus_gpio_init_wr(displayif_i80bus_gpio_t *bus,
    volatile uint32_t *outset, volatile uint32_t *outclr, uint32_t pin_mask);

bool displayif_i80bus_gpio_init_data(displayif_i80bus_gpio_t *bus,
    volatile uint32_t *group_outset[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS],
    volatile uint32_t *group_outclr[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS],
    const int *pin_ids, size_t pin_count);

void displayif_i80bus_gpio_write(displayif_i80bus_gpio_t *bus,
    const uint8_t *data, size_t len);

#endif /* DISPLAYIF_I80BUS_GPIO_H */
