// SPDX-License-Identifier: MIT

#include "displayif/i80bus_gpio.h"

#include <string.h>

static void displayif_i80bus_gpio_wr_inactive(displayif_i80bus_gpio_t *bus) {
    bus->wr.outclr[0] = bus->wr.pin_mask;
}

static void displayif_i80bus_gpio_wr_active(displayif_i80bus_gpio_t *bus) {
    bus->wr.outset[0] = bus->wr.pin_mask;
}

void displayif_i80bus_gpio_bind_port(displayif_i80bus_gpio_port_t *port,
    volatile uint32_t *outset, volatile uint32_t *outclr, uint32_t pin_mask) {
    port->outset = outset;
    port->outclr = outclr;
    port->pin_mask = pin_mask;
}

void displayif_i80bus_gpio_init_wr(displayif_i80bus_gpio_t *bus,
    volatile uint32_t *outset, volatile uint32_t *outclr, uint32_t pin_mask) {
    displayif_i80bus_gpio_bind_port(&bus->wr, outset, outclr, pin_mask);
    displayif_i80bus_gpio_wr_inactive(bus);
}

static bool displayif_i80bus_gpio_pins_sequential(const int *pin_ids, size_t pin_count) {
    if (pin_count != DISPLAYIF_I80BUS_GPIO_WIDTH) {
        return false;
    }
    uint8_t port_idx = (uint8_t)(pin_ids[0] / 32);
    for (size_t i = 1; i < pin_count; i++) {
        if (pin_ids[i] != pin_ids[i - 1] + 1) {
            return false;
        }
        if ((uint8_t)(pin_ids[i] / 32) != port_idx) {
            return false;
        }
    }
    return true;
}

static int displayif_i80bus_gpio_find_group(displayif_i80bus_gpio_t *bus, uint8_t port_idx) {
    for (uint8_t i = 0; i < bus->num_groups; i++) {
        if (bus->lut_group[i] == port_idx) {
            return (int)i;
        }
    }
    return -1;
}

bool displayif_i80bus_gpio_init_data(displayif_i80bus_gpio_t *bus,
    volatile uint32_t *group_outset[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS],
    volatile uint32_t *group_outclr[DISPLAYIF_I80BUS_GPIO_MAX_GROUPS],
    const int *pin_ids, size_t pin_count) {
    if (pin_count != DISPLAYIF_I80BUS_GPIO_WIDTH) {
        return false;
    }

    bus->num_groups = 0;
    bus->sequential = false;
    memset(bus->lut_table, 0, sizeof(bus->lut_table));

    if (displayif_i80bus_gpio_pins_sequential(pin_ids)) {
        uint8_t port_idx = (uint8_t)(pin_ids[0] / 32);
        bus->sequential = true;
        bus->seq_shift = (uint8_t)(pin_ids[0] & 31);
        uint32_t mask = 0;
        for (size_t i = 0; i < pin_count; i++) {
            mask |= 1u << (pin_ids[i] & 31u);
        }
        displayif_i80bus_gpio_bind_port(&bus->seq,
            group_outset[port_idx], group_outclr[port_idx], mask);
        return true;
    }

    for (size_t bit = 0; bit < pin_count; bit++) {
        uint8_t port_idx = (uint8_t)(pin_ids[bit] / 32);
        int group = displayif_i80bus_gpio_find_group(bus, port_idx);
        if (group < 0) {
            if (bus->num_groups >= DISPLAYIF_I80BUS_GPIO_MAX_GROUPS) {
                return false;
            }
            group = bus->num_groups++;
            bus->lut_group[group] = port_idx;
            displayif_i80bus_gpio_bind_port(&bus->lut_port[group],
                group_outset[port_idx], group_outclr[port_idx], 0);
        }
        bus->lut_port[group].pin_mask |= 1u << (pin_ids[bit] & 31u);
        for (uint16_t index = 0; index < 256; index++) {
            if (index & (1u << bit)) {
                bus->lut_table[group][index] |= 1u << (pin_ids[bit] & 31u);
            }
        }
    }

    return bus->num_groups > 0;
}

static void displayif_i80bus_gpio_write_byte_seq(displayif_i80bus_gpio_t *bus,
    uint8_t val, uint8_t *last) {
    displayif_i80bus_gpio_wr_inactive(bus);
    if (val != *last) {
        uint32_t tx = (uint32_t)val << bus->seq_shift;
        bus->seq.outset[0] = tx;
        bus->seq.outclr[0] = tx ^ bus->seq.pin_mask;
        *last = val;
    }
    displayif_i80bus_gpio_wr_active(bus);
}

static void displayif_i80bus_gpio_write_byte_lut(displayif_i80bus_gpio_t *bus,
    uint8_t val, uint8_t *last) {
    displayif_i80bus_gpio_wr_inactive(bus);
    if (val != *last) {
        for (uint8_t g = 0; g < bus->num_groups; g++) {
            uint32_t tx = bus->lut_table[g][val];
            bus->lut_port[g].outset[0] = tx;
            bus->lut_port[g].outclr[0] = tx ^ bus->lut_port[g].pin_mask;
        }
        *last = val;
    }
    displayif_i80bus_gpio_wr_active(bus);
}

void displayif_i80bus_gpio_write(displayif_i80bus_gpio_t *bus,
    const uint8_t *data, size_t len) {
    uint8_t last = 0xFF;
    if (bus->sequential) {
        for (size_t i = 0; i < len; i++) {
            displayif_i80bus_gpio_write_byte_seq(bus, data[i], &last);
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            displayif_i80bus_gpio_write_byte_lut(bus, data[i], &last);
        }
    }
    displayif_i80bus_gpio_wr_inactive(bus);
}
