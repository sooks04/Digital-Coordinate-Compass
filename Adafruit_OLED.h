#ifndef ADAFRUIT_OLED_H
#define ADAFRUIT_OLED_H

//*****************************************************************************
// Display Dimensions
//*****************************************************************************
#define SSD1351WIDTH  128
#define SSD1351HEIGHT 128

//*****************************************************************************
// Color Definitions (16-bit RGB 565)
//*****************************************************************************
#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF
#define ORANGE          0xFD20
#define GREENYELLOW     0xAFE5
#define PINK            0xF81F

//*****************************************************************************
// Function Prototypes
//*****************************************************************************

// Low-level SPI transmission functions
void writeCommand(unsigned char c);
void writeData(unsigned char c);

// Initialization
void Adafruit_Init(void);

// Cursor and Graphics Helper
void goTo(int x, int y);
unsigned int Color565(unsigned char r, unsigned char g, unsigned char b);

// Drawing Primitives
void fillRect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, unsigned int fillcolor);
void fillScreen(unsigned int fillcolor);
void drawFastVLine(int x, int y, int h, unsigned int color);
void drawFastHLine(int x, int y, int w, unsigned int color);
void drawPixel(int x, int y, unsigned int color);

// Text and Display Control
void invert(char v);
void drawText(int x, int y, char *text, unsigned int color, unsigned int bg, unsigned char size);

// Utility
void delay_ms(unsigned long ulCount);

// Note: drawChar() is called in your source but was not defined in the snippet.
// If it is an external function (from glcdfont.c), you may need to extern it here
// or include the appropriate GFX header.
// void drawChar(int x, int y, unsigned char c, unsigned int color, unsigned int bg, unsigned char size);

#endif // Adafruit_OLED_H
