// Standard includes
#include <string.h>

// Driverlib includes
#include "hw_types.h"
#include "oled_test.h"
#include "Adafruit_GFX.h"
#include "glcdfont.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "gpio.h"
#include "spi.h"
#include "rom.h"
#include "rom_map.h"
#include "utils.h"
#include "prcm.h"
#include "uart.h"
#include "interrupt.h"

// Common interface includes
#include "uart_if.h"

// INCLUDE YOUR SEPARATE PINMUX HEADER
#include "pinmux.h"

#include "Adafruit_SSD1351.h"

#define DC_GPIO_BASE    GPIOA0_BASE
#define DC_GPIO_PIN     0x80

// OLED Chip Select on Pin 58 (GPIO 3)
#define CS_GPIO_BASE    GPIOA0_BASE
#define CS_GPIO_PIN     0x08

#define RST_GPIO_BASE   GPIOA3_BASE
#define RST_GPIO_PIN    0x10

//*****************************************************************************

void writeCommand(unsigned char c) {
    unsigned long buffer;

    // Set DC LOW (Command Mode) - Writing to P62
    MAP_GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, 0);

    // Enable Chip Select (Active LOW) - Writing to P61
    MAP_GPIOPinWrite(CS_GPIO_BASE, CS_GPIO_PIN, 0);

    // Send Data via SPI
    MAP_SPIDataPut(GSPI_BASE, c);

    // Clean up SPI Buffer
    MAP_SPIDataGet(GSPI_BASE, &buffer);

    // Disable Chip Select (High)
    MAP_GPIOPinWrite(CS_GPIO_BASE, CS_GPIO_PIN, CS_GPIO_PIN);
}

//*****************************************************************************

void writeData(unsigned char c) {
    unsigned long buffer;

    // Set DC HIGH (Data Mode) - Writing to P62
    MAP_GPIOPinWrite(DC_GPIO_BASE, DC_GPIO_PIN, DC_GPIO_PIN);

    // Enable Chip Select (Active LOW) - Writing to P61
    MAP_GPIOPinWrite(CS_GPIO_BASE, CS_GPIO_PIN, 0);

    // Send Data via SPI
    MAP_SPIDataPut(GSPI_BASE, c);

    // Clean up SPI Buffer
    MAP_SPIDataGet(GSPI_BASE, &buffer);

    // Disable Chip Select (High)
    MAP_GPIOPinWrite(CS_GPIO_BASE, CS_GPIO_PIN, CS_GPIO_PIN);
}

//*****************************************************************************
void Adafruit_Init(void){
    volatile unsigned long delay;

    // Toggle Reset (using Pin 18)
    // NOTE: Ensure P18 is configured as Output in your pinmux.c
    MAP_GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, 0);            // RESET LOW
    for(delay=0; delay<500; delay=delay+1);                      // Delay
    MAP_GPIOPinWrite(RST_GPIO_BASE, RST_GPIO_PIN, RST_GPIO_PIN); // RESET HIGH
    for(delay=0; delay<500; delay=delay+1);

    // Initialization Sequence
    writeCommand(SSD1351_CMD_COMMANDLOCK);
    writeData(0x12);
    writeCommand(SSD1351_CMD_COMMANDLOCK);
    writeData(0xB1);
    writeCommand(SSD1351_CMD_DISPLAYOFF);
    writeCommand(SSD1351_CMD_CLOCKDIV);
    writeCommand(0xF1);
    writeCommand(SSD1351_CMD_MUXRATIO);
    writeData(127);
    writeCommand(SSD1351_CMD_SETREMAP);
    writeData(0x74);
    writeCommand(SSD1351_CMD_SETCOLUMN);
    writeData(0x00);
    writeData(0x7F);
    writeCommand(SSD1351_CMD_SETROW);
    writeData(0x00);
    writeData(0x7F);
    writeCommand(SSD1351_CMD_STARTLINE);
    writeData(0); // Assuming 128 height
    writeCommand(SSD1351_CMD_DISPLAYOFFSET);
    writeData(0x0);
    writeCommand(SSD1351_CMD_SETGPIO);
    writeData(0x00);
    writeCommand(SSD1351_CMD_FUNCTIONSELECT);
    writeData(0x01);
    writeCommand(SSD1351_CMD_PRECHARGE);
    writeCommand(0x32);
    writeCommand(SSD1351_CMD_VCOMH);
    writeCommand(0x05);
    writeCommand(SSD1351_CMD_NORMALDISPLAY);
    writeCommand(SSD1351_CMD_CONTRASTABC);
    writeData(0xC8);
    writeData(0x80);
    writeData(0xC8);
    writeCommand(SSD1351_CMD_CONTRASTMASTER);
    writeData(0x0F);
    writeCommand(SSD1351_CMD_SETVSL );
    writeData(0xA0);
    writeData(0xB5);
    writeData(0x55);
    writeCommand(SSD1351_CMD_PRECHARGE2);
    writeData(0x01);
    writeCommand(SSD1351_CMD_DISPLAYON);
}

/***********************************/

void goTo(int x, int y) {
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(SSD1351WIDTH-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(SSD1351HEIGHT-1);
  writeCommand(SSD1351_CMD_WRITERAM);
}

unsigned int Color565(unsigned char r, unsigned char g, unsigned char b) {
  unsigned int c;
  c = r >> 3;
  c <<= 6;
  c |= g >> 2;
  c <<= 5;
  c |= b >> 3;
  return c;
}

void fillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int fillcolor)
{
  unsigned int i;
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if (y+h > SSD1351HEIGHT) h = SSD1351HEIGHT - y - 1;
  if (x+w > SSD1351WIDTH) w = SSD1351WIDTH - x - 1;

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w*h; i++) {
    writeData(fillcolor >> 8);
    writeData(fillcolor);
  }
}

void fillScreen(unsigned int fillcolor) {
  fillRect(0, 0, SSD1351WIDTH, SSD1351HEIGHT, fillcolor);
}

void drawFastVLine(int x, int y, int h, unsigned int color) {
  unsigned int i;
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if (y+h > SSD1351HEIGHT) h = SSD1351HEIGHT - y - 1;
  if (h < 0) return;

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y+h-1);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < h; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}

void drawFastHLine(int x, int y, int w, unsigned int color) {
  unsigned int i;
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if (x+w > SSD1351WIDTH) w = SSD1351WIDTH - x - 1;
  if (w < 0) return;

  writeCommand(SSD1351_CMD_SETCOLUMN);
  writeData(x);
  writeData(x+w-1);
  writeCommand(SSD1351_CMD_SETROW);
  writeData(y);
  writeData(y);
  writeCommand(SSD1351_CMD_WRITERAM);

  for (i=0; i < w; i++) {
    writeData(color >> 8);
    writeData(color);
  }
}

void drawPixel(int x, int y, unsigned int color)
{
  if ((x >= SSD1351WIDTH) || (y >= SSD1351HEIGHT)) return;
  if ((x < 0) || (y < 0)) return;
  goTo(x, y);
  writeData(color >> 8);
  writeData(color);
}

void invert(char v) {
   if (v) {
     writeCommand(SSD1351_CMD_INVERTDISPLAY);
   } else {
     writeCommand(SSD1351_CMD_NORMALDISPLAY);
   }
}

// Print a string to the display
void drawText(int x, int y, char *text, unsigned int color, unsigned int bg, unsigned char size) {
    while (*text) {
        if (x + (size*6) >= SSD1351WIDTH) {
            x = 0; // Wrap to new line
            y += size * 8;
        }
        drawChar(x, y, *text, color, bg, size);
        x += size * 6; // Advance cursor (5 pixels char + 1 pixel space)
        text++;
    }
}

void delay_ms(unsigned long ulCount) {
    int i;
    do {
        ulCount--;
        for (i = 0; i < 65535; i++) ;
    } while (ulCount);
}
