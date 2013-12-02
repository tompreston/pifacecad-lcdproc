/** \file server/drivers/hd44780-pifacecad.c
 * \c PiFace Control and Display connection type (SPI) of \c hd44780 driver for Hitachi HD44780 based LCD displays.
 *
 * The LCD is operated in its 4 bit-mode to be connected to the 8 bit Port B
 * of a single MCP23S17 that is accessed by the server via the SPI bus.
 */

/* Copyright (c) 2013 Thomas Preston <thomas.preston@openlx.org.uk>
 *
 * The connections are (MCP23S17):
 * Port A Pin 0     Switch 0
 * Port A Pin 1     Switch 1
 * Port A Pin 2     Switch 2
 * Port A Pin 3     Switch 3
 * Port A Pin 4     Switch 4
 * Port A Pin 5     Switch 5
 * Port A Pin 6     Switch 6
 * Port A Pin 6     Switch 7
 * Port B Pin 0     D4
 * Port B Pin 1     D5
 * Port B Pin 2     D6
 * Port B Pin 3     D7
 * Port B Pin 4     EN
 * Port B Pin 5     RW
 * Port B Pin 6     RS
 *
 * Backlight
 * Port B Pin 7   Backlight (optional, active-high)
 *
 * Configuration:
 * device=/dev/spidev0.1   # the device file of the SPI bus
 * port=0x13   # Port B (GPIOB) on the MCP23S17
 *
 * Based mostly on the hd44780-i2c module, see there for a complete history.
 *
 * This file is released under the GNU General Public License. Refer to the
 * COPYING file distributed with this package.
 */

#include "hd44780-pifacecad.h"
#include "hd44780-low.h"

#include "report.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <stdint.h> /* For uint8_t */
#include <linux/spi/spidev.h>
#include "hd44780.h"


// Generally, any function that accesses the LCD control lines needs to be
// implemented separately for each HW design. This is typically (but not
// restricted to):
// HD44780_senddata
// HD44780_readkeypad

void pifacecad_HD44780_senddata(PrivateData * p,
                                unsigned char displayID,
                                unsigned char flags,
                                unsigned char ch);
void pifacecad_HD44780_backlight(PrivateData * p, unsigned char state);
void pifacecad_HD44780_close(PrivateData * p);
unsigned char pifacecad_HD44780_scankeypad(PrivateData * p);

// below bits are meant for the upper nibble of Port B on the MCP23S17
#define EN   0x10
#define RW   0x20
#define RS   0x40
#define BL   0x80

static const unsigned char spi_mode = 0;
static const unsigned char spi_bpw = 8; /* bits per word */
static const unsigned long spi_speed = 10000000L; /* 10MHz */
static const unsigned char spi_delay = 0;

#define SPI_HW_ADDR 0
#define DEFAULT_DEVICE      "/dev/spidev0.1"

#define DELAY_PULSE_US 1 // 1us
#define DELAY_SETTLE_US 40 // 40us
#define DELAY_SETUP_0_US 15000 // 15ms
#define DELAY_SETUP_1_US 5000 // 5ms
#define DELAY_SETUP_2_US 1000 // 1ms

// MCP23S17
#define WRITE_CMD 0
#define READ_CMD 1

// Register addresses
#define IODIRA 0x00  // I/O direction A
#define IODIRB 0x01  // I/O direction B
#define IPOLA 0x02  // I/O polarity A
#define IPOLB 0x03  // I/O polarity B
#define GPINTENA 0x04  // interupt enable A
#define GPINTENB 0x05  // interupt enable B
#define DEFVALA 0x06  // register default value A (interupts)
#define DEFVALB 0x07  // register default value B (interupts)
#define INTCONA 0x08  // interupt control A
#define INTCONB 0x09  // interupt control B
#define IOCON 0x0A  // I/O config (also 0x0B)
#define GPPUA 0x0C  // port A pullups
#define GPPUB 0x0D  // port B pullups
#define INTFA 0x0E  // interupt flag A (where the interupt came from)
#define INTFB 0x0F  // interupt flag B
#define INTCAPA 0x10  // interupt capture A (value at interupt is saved here)
#define INTCAPB 0x11  // interupt capture B
#define GPIOA 0x12  // port A
#define GPIOB 0x13  // port B
#define OLATA 0x14  // output latch A
#define OLATB 0x15  // output latch B

// I/O config
#define BANK_OFF 0x00  // addressing mode
#define BANK_ON 0x80
#define INT_MIRROR_ON 0x40  // interupt mirror (INTa|INTb)
#define INT_MIRROR_OFF 0x00
#define SEQOP_OFF 0x20  // incrementing address pointer
#define SEQOP_ON 0x00
#define DISSLW_ON 0x10  // slew rate
#define DISSLW_OFF 0x00
#define HAEN_ON 0x08  // hardware addressing
#define HAEN_OFF 0x00
#define ODR_ON 0x04  // open drain for interupts
#define ODR_OFF 0x00
#define INTPOL_HIGH 0x02  // interupt polarity
#define INTPOL_LOW 0x00

#define SWITCH_PORT 0x12 // GPIOA
#define LCD_PORT 0x13 // GPIOB

#define SWITCH_LEFT 0x41 // Left nav (6) and button 0 (0) 0b01000001
#define SWITCH_DOWN 0x02 // Button 1 (1) 0b00000010
#define SWITCH_UP 0x04 // Button 2 (2) 0b00000100
#define SWITCH_RIGHT 0x88 // Right nav (7) and button 3 (3) 0b10001000
#define SWITCH_ENTER 0x24 // In nav (5) and button 4 (4) 0b00100100

/**
 * Writes to a register on the MCP23S17.
 *
 * \param p     Pointer to PrivateData structure.
 * \param reg   Register address to write to.
 * \param data  Data to write
 */
static void
mcp23s17_write_reg(PrivateData * p, unsigned char reg, unsigned char data)
{
    unsigned char tx_buf[3] = {0x40 | SPI_HW_ADDR | WRITE_CMD, reg, data};
    unsigned char rx_buf[sizeof tx_buf];

    struct spi_ioc_transfer spi;
    spi.tx_buf = (unsigned long) tx_buf;
    spi.rx_buf = (unsigned long) rx_buf;
    spi.len = sizeof tx_buf;
    spi.delay_usecs = spi_delay;
    spi.speed_hz = spi_speed;
    spi.bits_per_word = spi_bpw;

    /* do the SPI transaction */
    if (ioctl(p->fd, SPI_IOC_MESSAGE(1), &spi) < 1) {
        p->hd44780_functions->drv_report(
            RPT_ERR, "HD44780: PiFaceCAD: mcp23s17_write_reg: There was "
                     "a error during the SPI transaction: %s",
            strerror(errno));
    }
}

/**
 * Reads from a register on the MCP23S17.
 *
 * \param p     Pointer to PrivateData structure.
 * \param reg   Register address to read from.
 */
static unsigned char
mcp23s17_read_reg(PrivateData * p, unsigned char reg)
{
    unsigned char tx_buf[3] = {0x40 | SPI_HW_ADDR | READ_CMD, reg, 0};
    unsigned char rx_buf[sizeof tx_buf];

    struct spi_ioc_transfer spi;
    spi.tx_buf = (unsigned long) tx_buf;
    spi.rx_buf = (unsigned long) rx_buf;
    spi.len = sizeof tx_buf;
    spi.delay_usecs = spi_delay;
    spi.speed_hz = spi_speed;
    spi.bits_per_word = spi_bpw;

    /* do the SPI transaction */
    if (ioctl(p->fd, SPI_IOC_MESSAGE(1), &spi) < 1) {
        p->hd44780_functions->drv_report(
            RPT_ERR, "HD44780: PiFaceCAD: mcp23s17_write_reg: There was"
                     "a error during the SPI transaction: %s",
            strerror(errno));
    }
    return rx_buf[2]; /* return data */
}

/**
 * Writes control and data to HD44780 through Port B on MCP23S17.
 *
 * \param p     Pointer to the PrivateData structure.
 * \param data  The data to write.
 */
static void
write_and_pulse(PrivateData * p, unsigned char data)
{
    /* Enable: Off/On/Off - Doesn't seem to work any other way. */
    mcp23s17_write_reg(p, LCD_PORT, data);
    if (p->delayBus)
        p->hd44780_functions->uPause(p, DELAY_PULSE_US);
    mcp23s17_write_reg(p, LCD_PORT, EN | data);
    if (p->delayBus)
        p->hd44780_functions->uPause(p, DELAY_PULSE_US);
    mcp23s17_write_reg(p, LCD_PORT, data);

    p->hd44780_functions->uPause(p, DELAY_SETTLE_US);
}

/**
 * Initialises the HD44780.
 *
 * \param p     Pointer to the PrivateData structure.
 */
static void
init_lcd(PrivateData * p)
{
    p->hd44780_functions->uPause(p, DELAY_SETUP_0_US);
    write_and_pulse(p, 0x3);

    p->hd44780_functions->uPause(p, DELAY_SETUP_1_US);
    write_and_pulse(p, 0x3);

    p->hd44780_functions->uPause(p, DELAY_SETUP_2_US);
    write_and_pulse(p, 0x3);

    write_and_pulse(p, 0x2);

    pifacecad_HD44780_senddata(p, 0, RS_INSTR,
        FUNCSET | IF_4BIT | TWOLINE | SMALLCHAR);

    pifacecad_HD44780_senddata(p, 0, RS_INSTR,
        ONOFFCTRL| DISPOFF | CURSOROFF | CURSORNOBLINK);

    pifacecad_HD44780_senddata(p, 0, RS_INSTR, CLEAR);

    pifacecad_HD44780_senddata(p, 0, RS_INSTR,
        ENTRYMODE | E_MOVERIGHT | NOSCROLL);
}

/**
 * Initialize the driver.
 *
 * \param drvthis  Pointer to driver structure.
 * \retval 0       Success.
 * \retval -1      Error.
 */
int
hd_init_pifacecad(Driver *drvthis)
{
    PrivateData *p = (PrivateData*) drvthis->private_data;
    HD44780_functions *hd44780_functions = p->hd44780_functions;

    char device[256] = DEFAULT_DEVICE;

    p->backlight_bit = BL;

    /* READ CONFIG FILE */
    /* Get serial device to use (copied from i2c) */
    strncpy(device,
            drvthis->config_get_string(drvthis->name,
                                       "Device",
                                       0,
                                       DEFAULT_DEVICE),
            sizeof(device));
    device[sizeof(device)-1] = '\0';
    report(RPT_INFO, "HD44780: PiFaceCAD: Using device '%s'", device);

    /* Open the SPI device */
    if ((p->fd = open(device, O_RDWR)) < 0) {
        report(RPT_ERR,
               "HD44780: PiFaceCAD: open SPI device '%s' failed: %s",
               device,
               strerror(errno));
        return -1;
    }

    /* configure SPI device */
    if (ioctl(p->fd, SPI_IOC_WR_MODE, &spi_mode) < 0) {
        report(RPT_ERR, "HD44780: PiFaceCAD: Could not set SPI mode.");
        return -1;
    }
    if (ioctl(p->fd, SPI_IOC_WR_BITS_PER_WORD, &spi_bpw) < 0) {
        report(RPT_ERR, "HD44780: PiFaceCAD Could not set SPI bits per word.");
        return -1;
    }
    if (ioctl(p->fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed) < 0) {
        report(RPT_ERR, "HD44780: PiFaceCAD: Could not set SPI speed.");
        return -1;
    }

    /* Set IO config */
    const unsigned char ioconfig = BANK_OFF | \
                                   INT_MIRROR_OFF | \
                                   SEQOP_ON | \
                                   DISSLW_OFF | \
                                   HAEN_ON | \
                                   ODR_OFF | \
                                   INTPOL_LOW;
    mcp23s17_write_reg(p, IOCON, ioconfig);
    mcp23s17_write_reg(p, IODIRB, 0); /* Set GPIOB (LCD port) to output */
    mcp23s17_write_reg(p, IODIRA, 0xff); /* Set GPIOA (switches) to input */

    hd44780_functions->senddata = pifacecad_HD44780_senddata;
    hd44780_functions->backlight = pifacecad_HD44780_backlight;
    hd44780_functions->close = pifacecad_HD44780_close;
    hd44780_functions->scankeypad = pifacecad_HD44780_scankeypad;

    init_lcd(p);

    hd44780_functions->uPause(p, DELAY_SETTLE_US);

    common_init(p, IF_4BIT);

    report(RPT_INFO, "HD44780: pifacecad: initialised");

    return 0;
}

/**
 * Closes the file descriptor of the HD44780 (SPI bus to MCP23S17).
 *
 * \param p  Pointer to driver's private data structure.
 */
void
pifacecad_HD44780_close(PrivateData * p) {
    if (p->fd >= 0) {
        close(p->fd);
    }
}


/**
 * Send data or commands to the display.
 *
 * \param p          Pointer to driver's private data structure.
 * \param displayID  ID of the display (or 0 for all) to send data to.
 * \param flags      Defines whether to end a command or data.
 * \param ch         The value to send.
 */
void
pifacecad_HD44780_senddata(PrivateData * p,
                           unsigned char displayID,
                           unsigned char flags,
                           unsigned char ch)
{
    unsigned char portControl = 0;
    unsigned char h = (ch >> 4) & 0x0f; // high and low nibbles
    unsigned char l = ch & 0x0f;

    if (flags == RS_INSTR)
        portControl = 0;
    else //if (flags == RS_DATA)
        portControl = RS;

    portControl |= p->backlight_bit;

    write_and_pulse(p, portControl | h);
    write_and_pulse(p, portControl | l);
}


/**
 * Turn display backlight on or off.
 *
 * \param p      Pointer to driver's private data structure.
 * \param state  New backlight status.
 */
void pifacecad_HD44780_backlight(PrivateData *p, unsigned char state)
{
    p->backlight_bit = ((p->have_backlight && state) ? BL : 0);
    unsigned char port_state = mcp23s17_read_reg(p, LCD_PORT);
    if (p->backlight_bit)
        port_state |= BL; // set
    else
        port_state &= 0xff ^ BL;
    mcp23s17_write_reg(p, LCD_PORT, port_state);
}


/**
 * Scan the LCD keys
 *
 * \param p      Pointer to driver's private data structure.
 * \retval key   Key number
 */
unsigned char
pifacecad_HD44780_scankeypad(PrivateData *p)
{
    unsigned char switch_state = mcp23s17_read_reg(p, SWITCH_PORT);

    if (switch_state == 0)
        return 0;

    if (switch_state & SWITCH_LEFT)
        return 1;

    if (switch_state & SWITCH_DOWN)
        return 2;

    if (switch_state & SWITCH_UP)
        return 3;

    if (switch_state & SWITCH_RIGHT)
        return 4;

    if (switch_state & SWITCH_ENTER)
        return 5;

    return 0;
}
