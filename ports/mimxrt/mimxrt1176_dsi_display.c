// SPDX-License-Identifier: MIT
// MIMXRT1176 MIPI DSI (split driver) + LCDIFV2 video bridge.

#include <string.h>

#include "py/mphal.h"
#include "displayif/mp_helpers.h"
#include "mimxrt1176_dsi_display.h"

#if defined(MIMXRT1176_SERIES) || defined(CPU_MIMXRT1176) || defined(CPU_MIMXRT1176DVMAA_cm7)

#include "fsl_clock.h"
#include "fsl_gpio.h"
#include "fsl_lcdifv2.h"
#include "fsl_mipi_dsi.h"

#ifndef BOARD_XTAL0_CLK_HZ
#define BOARD_XTAL0_CLK_HZ 24000000U
#endif

#define DISPLAYIF_MIPI_DSI_LANE_NUM_MAX 4
#define DISPLAYIF_MIPI_DPHY_BIT_CLK_ENLARGE(origin) (((origin) / 8U) * 9U)
#define DISPLAYIF_DSI_INIT_DELAY_FLAG 0x80U

/* TC358762 register addresses (Linux panel-raspberrypi-touchscreen.c). */
#define DISPLAYIF_TC358762_DSI_LANEENABLE        0x0210U
#define DISPLAYIF_TC358762_PPI_D0S_CLRSIPOCOUNT 0x0164U
#define DISPLAYIF_TC358762_PPI_D1S_CLRSIPOCOUNT 0x0168U
#define DISPLAYIF_TC358762_PPI_D0S_ATMR          0x0144U
#define DISPLAYIF_TC358762_PPI_D1S_ATMR          0x0148U
#define DISPLAYIF_TC358762_PPI_LPTXTIMECNT     0x0114U
#define DISPLAYIF_TC358762_SPICMR                0x0450U
#define DISPLAYIF_TC358762_LCDCTRL               0x0420U
#define DISPLAYIF_TC358762_SYSCTRL               0x0464U
#define DISPLAYIF_TC358762_PPI_STARTPPI          0x0104U
#define DISPLAYIF_TC358762_DSI_STARTDSI          0x0204U

#define DISPLAYIF_TC358762_LANE_CLOCK 0x01U
#define DISPLAYIF_TC358762_LANE_D0     0x02U
#define DISPLAYIF_TC358762_LANE_D1     0x04U

static const MIPI_DSI_Type s_mipi_dsi = {
    .host = DSI_HOST,
    .apb = DSI_HOST_APB_PKT_IF,
    .dpi = DSI_HOST_DPI_INTFC,
    .dphy = DSI_HOST_DPHY_INTFC,
};

static bool s_dsi_bus_ready;
static bool s_dsi_display_ready;
static uint32_t s_mipi_esc_tx_clk_hz;
static uint32_t s_mipi_dphy_ref_clk_hz;
static uint32_t s_mipi_dphy_bit_clk_hz;
static uint32_t s_mipi_dpi_clk_hz;
static displayif_mimxrt1176_dsi_timings_t s_timings;

static void displayif_gpio_out(int pin, bool on) {
    if (pin < 0) {
        return;
    }
    displayif_machine_pin(pin, 1, on ? 1 : 0);
}

static void displayif_gpio_reset(int pin) {
    if (pin < 0) {
        return;
    }
    displayif_gpio_out(pin, false);
    mp_hal_delay_ms(10);
    displayif_gpio_out(pin, true);
    mp_hal_delay_ms(200);
}

static void displayif_init_mipi_dsi_clock(void) {
    const clock_root_config_t mipi_esc_clock_config = {
        .clockOff = false,
        .mux = 4,
        .div = 11,
    };
    CLOCK_SetRootClock(kCLOCK_Root_Mipi_Esc, &mipi_esc_clock_config);
    uint32_t esc_clk_hz = CLOCK_GetRootClockFreq(kCLOCK_Root_Mipi_Esc);

    const clock_group_config_t mipi_esc_clock_group_config = {
        .clockOff = false,
        .resetDiv = 2,
        .div0 = 2,
    };
    CLOCK_SetGroupConfig(kCLOCK_Group_MipiDsi, &mipi_esc_clock_group_config);
    s_mipi_esc_tx_clk_hz = esc_clk_hz / 3U;

    const clock_root_config_t mipi_dphy_ref_clock_config = {
        .clockOff = false,
        .mux = 1,
        .div = 1,
    };
    CLOCK_SetRootClock(kCLOCK_Root_Mipi_Ref, &mipi_dphy_ref_clock_config);
    s_mipi_dphy_ref_clk_hz = BOARD_XTAL0_CLK_HZ;
}

static void displayif_init_lcdifv2_clock(uint32_t pixel_clock_hz) {
    (void)pixel_clock_hz;
    const clock_root_config_t lcdif_clock_config = {
        .clockOff = false,
        .mux = 4,
        .div = 15,
    };
    CLOCK_SetRootClock(kCLOCK_Root_Lcdifv2, &lcdif_clock_config);
    s_mipi_dpi_clk_hz = CLOCK_GetRootClockFreq(kCLOCK_Root_Lcdifv2);
}

static void displayif_set_mipi_dsi_config(uint8_t num_lanes, const displayif_mimxrt1176_dsi_timings_t *timings) {
    dsi_config_t dsi_config;
    dsi_dphy_config_t dphy_config;

    const dsi_dpi_config_t dpi_config = {
        .pixelPayloadSize = timings->width,
        .dpiColorCoding = kDSI_Dpi16BitConfig1,
        .pixelPacket = kDSI_PixelPacket16Bit,
        .videoMode = kDSI_DpiBurst,
        .bllpMode = kDSI_DpiBllpLowPower,
        .polarityFlags = kDSI_DpiVsyncActiveLow | kDSI_DpiHsyncActiveLow,
        .hfp = timings->hfp,
        .hbp = timings->hbp,
        .hsw = timings->hsw,
        .vfp = timings->vfp,
        .vbp = timings->vbp,
        .panelHeight = timings->height,
        .virtualChannel = 0,
    };

    DSI_GetDefaultConfig(&dsi_config);
    dsi_config.numLanes = num_lanes;
    dsi_config.autoInsertEoTp = true;
    DSI_Init(&s_mipi_dsi, &dsi_config);

    uint32_t dphy_bit_clk_hz = s_mipi_dpi_clk_hz * (16U / num_lanes);
    dphy_bit_clk_hz = DISPLAYIF_MIPI_DPHY_BIT_CLK_ENLARGE(dphy_bit_clk_hz);
    if (timings->lane_bit_rate_hz > 0) {
        dphy_bit_clk_hz = timings->lane_bit_rate_hz;
    }

    DSI_GetDphyDefaultConfig(&dphy_config, dphy_bit_clk_hz, s_mipi_esc_tx_clk_hz);
    s_mipi_dphy_bit_clk_hz = DSI_InitDphy(&s_mipi_dsi, &dphy_config, s_mipi_dphy_ref_clk_hz);
    DSI_SetDpiConfig(&s_mipi_dsi, &dpi_config, num_lanes, s_mipi_dpi_clk_hz, s_mipi_dphy_bit_clk_hz);
}

static status_t displayif_dsi_transfer_blocking(dsi_transfer_t *xfer) {
    return DSI_TransferBlocking(&s_mipi_dsi, xfer);
}

status_t displayif_mimxrt1176_dsi_generic_write_reg(uint16_t reg, uint32_t value) {
    uint8_t payload[6] = {
        (uint8_t)(reg & 0xFFU),
        (uint8_t)(reg >> 8),
        (uint8_t)(value & 0xFFU),
        (uint8_t)((value >> 8) & 0xFFU),
        (uint8_t)((value >> 16) & 0xFFU),
        (uint8_t)((value >> 24) & 0xFFU),
    };

    dsi_transfer_t xfer = {0};
    xfer.virtualChannel = 0;
    xfer.flags = kDSI_TransferUseHighSpeed;
    xfer.txDataType = kDSI_TxDataGenLongWr;
    xfer.txDataSize = sizeof(payload);
    xfer.txData = payload;
    xfer.sendDscCmd = false;
    return displayif_dsi_transfer_blocking(&xfer);
}

status_t displayif_mimxrt1176_dsi_init_tc358762_bridge(uint8_t num_lanes) {
    if (num_lanes == 0 || num_lanes > DISPLAYIF_MIPI_DSI_LANE_NUM_MAX) {
        return kStatus_InvalidArgument;
    }

    uint32_t lane_enable = DISPLAYIF_TC358762_LANE_CLOCK;
    if (num_lanes >= 1) {
        lane_enable |= DISPLAYIF_TC358762_LANE_D0;
    }
    if (num_lanes >= 2) {
        lane_enable |= DISPLAYIF_TC358762_LANE_D1;
    }

    status_t status;

    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_DSI_LANEENABLE, lane_enable);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_D0S_CLRSIPOCOUNT, 0x05U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_D1S_CLRSIPOCOUNT, 0x05U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_D0S_ATMR, 0x00U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_D1S_ATMR, 0x00U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_LPTXTIMECNT, 0x03U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_SPICMR, 0x00U);
    if (status != kStatus_Success) {
        return status;
    }
    /* RGB888 DPI, HSYNC/VSYNC active low — matches vc4-kms-dsi-7inch / RPi firmware. */
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_LCDCTRL, 0x00100150U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_SYSCTRL, 0x040fU);
    if (status != kStatus_Success) {
        return status;
    }

    mp_hal_delay_ms(100);

    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_PPI_STARTPPI, 0x01U);
    if (status != kStatus_Success) {
        return status;
    }
    status = displayif_mimxrt1176_dsi_generic_write_reg(DISPLAYIF_TC358762_DSI_STARTDSI, 0x01U);
    if (status != kStatus_Success) {
        return status;
    }

    mp_hal_delay_ms(100);
    return kStatus_Success;
}

status_t displayif_mimxrt1176_dsi_send_init_sequence(const uint8_t *init_sequence, size_t init_len) {
    size_t i = 0;
    while (i < init_len) {
        if (i + 1 >= init_len) {
            return kStatus_InvalidArgument;
        }
        uint8_t command = init_sequence[i];
        int data_size = init_sequence[i + 1];
        bool delay = (data_size & DISPLAYIF_DSI_INIT_DELAY_FLAG) != 0;
        data_size &= ~DISPLAYIF_DSI_INIT_DELAY_FLAG;

        if (i + 2 + (size_t)data_size > init_len) {
            return kStatus_InvalidArgument;
        }

        dsi_transfer_t xfer = {0};
        xfer.virtualChannel = 0;
        xfer.flags = kDSI_TransferUseHighSpeed;
        const uint8_t *params = (data_size > 0) ? &init_sequence[i + 2] : NULL;

        if (data_size == 0) {
            xfer.txDataType = kDSI_TxDataDcsShortWrNoParam;
            xfer.txDataSize = 0;
            xfer.txData = NULL;
            xfer.sendDscCmd = true;
            xfer.dscCmd = command;
        } else if (data_size == 1) {
            xfer.txDataType = kDSI_TxDataDcsShortWrOneParam;
            xfer.txDataSize = 1;
            xfer.txData = params;
            xfer.sendDscCmd = true;
            xfer.dscCmd = command;
        } else {
            uint8_t stack_buf[64];
            if ((size_t)data_size > sizeof(stack_buf)) {
                return kStatus_InvalidArgument;
            }
            memcpy(stack_buf, params, (size_t)data_size);
            xfer.txDataType = kDSI_TxDataDcsLongWr;
            xfer.txDataSize = (uint16_t)data_size;
            xfer.txData = stack_buf;
            xfer.sendDscCmd = true;
            xfer.dscCmd = command;
        }

        status_t status = displayif_dsi_transfer_blocking(&xfer);
        if (status != kStatus_Success) {
            return status;
        }

        uint32_t delay_time_ms = 10;
        if (delay) {
            if (i + 2 + (size_t)data_size >= init_len) {
                return kStatus_InvalidArgument;
            }
            delay_time_ms = init_sequence[i + 2 + data_size];
            if (delay_time_ms == 255) {
                delay_time_ms = 500;
            }
            data_size += 1;
        }

        mp_hal_delay_ms(delay_time_ms);
        i += 2 + (size_t)data_size;
    }
    return kStatus_Success;
}

status_t displayif_mimxrt1176_dsi_bus_init(uint8_t num_lanes, uint32_t lane_bit_rate_hz) {
    if (s_dsi_bus_ready) {
        return kStatus_Success;
    }
    if (num_lanes == 0 || num_lanes > DISPLAYIF_MIPI_DSI_LANE_NUM_MAX) {
        return kStatus_InvalidArgument;
    }

    CLOCK_EnableClock(kCLOCK_Video_Mux);
    VIDEO_MUX->VID_MUX_CTRL.SET = VIDEO_MUX_VID_MUX_CTRL_MIPI_DSI_SEL_MASK;

    PGMC_BPC4->BPC_POWER_CTRL |= (PGMC_BPC_BPC_POWER_CTRL_PSW_ON_SOFT_MASK | PGMC_BPC_BPC_POWER_CTRL_ISO_OFF_SOFT_MASK);

    IOMUXC_GPR->GPR62 &= ~(IOMUXC_GPR_GPR62_MIPI_DSI_PCLK_SOFT_RESET_N_MASK
        | IOMUXC_GPR_GPR62_MIPI_DSI_ESC_SOFT_RESET_N_MASK
        | IOMUXC_GPR_GPR62_MIPI_DSI_BYTE_SOFT_RESET_N_MASK
        | IOMUXC_GPR_GPR62_MIPI_DSI_DPI_SOFT_RESET_N_MASK);

    displayif_init_mipi_dsi_clock();

    IOMUXC_GPR->GPR62 |= (IOMUXC_GPR_GPR62_MIPI_DSI_PCLK_SOFT_RESET_N_MASK
        | IOMUXC_GPR_GPR62_MIPI_DSI_ESC_SOFT_RESET_N_MASK);

    (void)lane_bit_rate_hz;
    s_dsi_bus_ready = true;
    return kStatus_Success;
}

void displayif_mimxrt1176_dsi_bus_deinit(void) {
    if (!s_dsi_bus_ready) {
        return;
    }
    DSI_DeinitDphy(&s_mipi_dsi);
    DSI_Deinit(&s_mipi_dsi);
    s_dsi_bus_ready = false;
}

static void displayif_lcdifv2_start(uint8_t *buf) {
    LCDIFV2_Init(LCDIFV2);

    lcdifv2_display_config_t display_config;
    LCDIFV2_DisplayGetDefaultConfig(&display_config);
    display_config.panelWidth = s_timings.width;
    display_config.panelHeight = s_timings.height;
    display_config.hsw = s_timings.hsw;
    display_config.hfp = s_timings.hfp;
    display_config.hbp = s_timings.hbp;
    display_config.vsw = s_timings.vsw;
    display_config.vfp = s_timings.vfp;
    display_config.vbp = s_timings.vbp;
    display_config.polarityFlags = kLCDIFV2_DataEnableActiveHigh | kLCDIFV2_VsyncActiveLow
        | kLCDIFV2_HsyncActiveLow | kLCDIFV2_DriveDataOnFallingClkEdge;
    display_config.lineOrder = kLCDIFV2_LineOrderRGB;
    LCDIFV2_SetDisplayConfig(LCDIFV2, &display_config);

    lcdifv2_buffer_config_t buffer_config = {
        .strideBytes = s_timings.width * sizeof(uint16_t),
        .pixelFormat = kLCDIFV2_PixelFormatRGB565,
    };
    LCDIFV2_SetLayerSize(LCDIFV2, 0, s_timings.width, s_timings.height);
    LCDIFV2_SetLayerOffset(LCDIFV2, 0, 0, 0);
    LCDIFV2_SetLayerBufferConfig(LCDIFV2, 0, &buffer_config);
    LCDIFV2_SetLayerBufferAddr(LCDIFV2, 0, LCDIFV2_ADDR_CPU_2_IP((uint32_t)buf));

    lcdifv2_blend_config_t blend_config = {
        .alphaMode = kLCDIFV2_AlphaDisable,
    };
    LCDIFV2_SetLayerBlendConfig(LCDIFV2, 0, &blend_config);
    LCDIFV2_EnableLayer(LCDIFV2, 0, true);
    LCDIFV2_EnableDisplay(LCDIFV2, true);
}

status_t displayif_mimxrt1176_dsi_display_start(const displayif_mimxrt1176_dsi_timings_t *timings,
    const uint8_t *init_sequence, size_t init_len, int reset_pin, int backlight_pin, bool backlight_on_high) {
    if (!s_dsi_bus_ready || timings == NULL) {
        return kStatus_Fail;
    }
    if (s_dsi_display_ready) {
        return kStatus_Success;
    }

    s_timings = *timings;
    displayif_init_lcdifv2_clock(timings->pixel_clock_hz);
    displayif_set_mipi_dsi_config(timings->num_lanes, timings);

    IOMUXC_GPR->GPR62 |= (IOMUXC_GPR_GPR62_MIPI_DSI_BYTE_SOFT_RESET_N_MASK
        | IOMUXC_GPR_GPR62_MIPI_DSI_DPI_SOFT_RESET_N_MASK);

    displayif_gpio_reset(reset_pin);
    if (init_sequence != NULL && init_len > 0) {
        status_t status = displayif_mimxrt1176_dsi_send_init_sequence(init_sequence, init_len);
        if (status != kStatus_Success) {
            return status;
        }
    } else {
        /* Waveshare 50H-800480-IPS / RPi 7" class: TC358762 DSI-to-DPI bridge. */
        status_t status = displayif_mimxrt1176_dsi_init_tc358762_bridge(timings->num_lanes);
        if (status != kStatus_Success) {
            return status;
        }
    }

    if (backlight_pin >= 0) {
        bool on = backlight_on_high;
        displayif_gpio_out(backlight_pin, on);
    }

    s_dsi_display_ready = true;
    return kStatus_Success;
}

void displayif_mimxrt1176_dsi_display_stop(void) {
    if (!s_dsi_display_ready) {
        return;
    }
    LCDIFV2_EnableDisplay(LCDIFV2, false);
    LCDIFV2_EnableLayer(LCDIFV2, 0, false);
    LCDIFV2_Deinit(LCDIFV2);
    s_dsi_display_ready = false;
}

void displayif_mimxrt1176_dsi_lcdifv2_start(uint8_t *buf) {
    if (!s_dsi_bus_ready || buf == NULL) {
        return;
    }
    displayif_lcdifv2_start(buf);
}

void displayif_mimxrt1176_dsi_set_framebuffer(uint8_t *buf) {
    if (!s_dsi_display_ready || buf == NULL) {
        return;
    }
    LCDIFV2_SetLayerBufferAddr(LCDIFV2, 0, LCDIFV2_ADDR_CPU_2_IP((uint32_t)buf));
}

#endif /* MIMXRT1176 */
