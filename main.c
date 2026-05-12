//              Sukhraj Johal
// Tobia Albano
#include <stdio.h>
#include <string.h>
#include <stdlib.h>      // For atof(), abs()
#include <math.h>        // For sqrt(), atan2(), sin(), cos()
#include <stdbool.h>

// Simplelink includes
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include <stdint.h>
#include "hw_memmap.h"

//Common interface includes
#include "pinmux.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"
#include "i2c_if.h"
#include "utils/network_utils.h"

// OLED includes
#include "Adafruit_SSD1351.h"
#include "spi.h"
#include "oled_test.h"
#include "Adafruit_GFX.h"
#include "glcdfont.h"

// --- MACROS & DEFINES ---
#define BNO055_CALIB_STAT_ADDR 0x35
#define BNO055_I2C_ADDR             0x28
#define BNO055_OPR_MODE_ADDR        0x3D
#define OPERATION_MODE_NDOF         0x0C
#define BNO055_EUL_HEADING_LSB      0x1A

#define DC_GPIO_BASE    GPIOA0_BASE
#define DC_GPIO_PIN     0x80
#define CS_GPIO_BASE    GPIOA0_BASE
#define CS_GPIO_PIN     0x40
#define RST_GPIO_BASE   GPIOA3_BASE
#define RST_GPIO_PIN    0x10

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define IR_GPIO_PORT            GPIOA0_BASE
#define IR_GPIO_PIN             0x40
#define FOREVER                 1
#define TICKS_PER_US            80
#define SYSTICK_MAX             0xFFFFFF
#define SLOW_CLK_FREQ           32768
#define POLL_INTERVAL_TICKS     (5 * SLOW_CLK_FREQ) // 5 seconds

#define DATE                12    /* Current Date */
#define MONTH               3     /* Month 1-12 */
#define YEAR                2026  /* Current year */
#define HOUR                22    /* Time - hours */
#define MINUTE              28    /* Time - minutes */
#define SECOND              7     /* Time - seconds */

#define APPLICATION_NAME      "SSL"
#define APPLICATION_VERSION   "SQ24"
#define SERVER_NAME           "aw14k1hchhvbf-ats.iot.us-east-2.amazonaws.com"
#define GOOGLE_DST_PORT       8443

#define GETHEADER "GET /things/Sukhraj_920857647_CC3200/shadow HTTP/1.1\r\n"
#define HOSTHEADER "Host: aw14k1hchhvbf-ats.iot.us-east-2.amazonaws.com\r\n"
#define CHEADER "Connection: Keep-Alive\r\n"

// --- APP STATES ---
typedef enum {
    STATE_MENU,
    STATE_NAVIGATING
} AppState;

volatile AppState g_current_state = STATE_MENU;
volatile bool g_ui_needs_full_redraw = true;

// --- GLOBALS FOR IR DECODING ---
volatile uint64_t g_ulLastTick = 0;
volatile uint64_t g_ullFrameData = 0;
volatile int g_iBitCount = 0;
volatile int g_bCodeReady = 0;
volatile uint64_t g_ullFinalCode = 0;

// Destination name
volatile char g_dest_name[32] = "None";

// --- GLOBALS FOR NAVIGATION ---
volatile float g_dest_lat = 0.0;
volatile float g_dest_lon = 0.0;
volatile float g_current_lat = 0.0;
volatile float g_current_lon = 0.0;
volatile float g_current_heading = 0.0;
volatile float g_current_distance = 0.0;
volatile float g_current_angle_to = 0.0;

extern void(* const g_pfnVectors[])(void);

// --- FUNCTION PROTOTYPES ---
static int set_time();
static void BoardInit(void);
static int http_get(int);
void calculate_navigation(void);
void init_BNO055(void);
float get_BNO055_heading(void);
int check_calibration_status(void);
void float_to_string(float val, char* buffer);
void update_oled_ui(void);
void draw_menu(void);


//*****************************************************************************
//
//! Helper to convert float to string for the OLED (4 decimal places)
//
//*****************************************************************************
void float_to_string(float val, char* buffer) {
    int is_negative = (val < 0) ? 1 : 0;
    if (is_negative) val = -val;

    int int_part = (int)val;
    int frac_part = (int)((val - int_part) * 10000);

    if (is_negative) {
        sprintf(buffer, "-%d.%04d", int_part, frac_part);
    } else {
        sprintf(buffer, "%d.%04d", int_part, frac_part);
    }
}

//*****************************************************************************
//
//! Draw the Main Menu Screen
//
//*****************************************************************************
void draw_menu() {
    fillScreen(0x0000);
    drawText(0, 0,  "--- SELECT DEST ---", 0x07FF, 0x0000, 1);
    drawText(0, 12, "1: ARC", 0xFFFF, 0x0000, 1);
    drawText(0, 22, "2: Kemper Hall", 0xFFFF, 0x0000, 1);
    drawText(0, 32, "3: Bainer Hall", 0xFFFF, 0x0000, 1);
    drawText(0, 42, "4: Young Hall", 0xFFFF, 0x0000, 1);
    drawText(0, 52, "5: MU", 0xFFFF, 0x0000, 1);
    drawText(0, 62, "6: Cows :D", 0xFFFF, 0x0000, 1);
    drawText(0, 72, "7: Stockton :(", 0xFFFF, 0x0000, 1);
    drawText(0, 82, "8: Wellman Hall", 0xFFFF, 0x0000, 1);
    drawText(0, 92, "9: Raising Cane's", 0xFFFF, 0x0000, 1);
    drawText(0, 102,"0: 80 Freeway", 0xFFFF, 0x0000, 1);
    drawText(0, 118,"DEL: Compass Only", 0xF800, 0x0000, 1);
}

//*****************************************************************************
//
//! Master OLED UI Update Function (Only runs during NAVIGATING state)
//
//*****************************************************************************
void update_oled_ui() {
    if (g_current_state != STATE_NAVIGATING) return;

    static float prev_c_lat = 999.0, prev_c_lon = 999.0;
    static float prev_d_lat = 999.0, prev_d_lon = 999.0;
    static float prev_dist = -1.0;
    static char prev_dest_name[32] = "";

    static int prev_nx = 64, prev_ny = 42;
    static int prev_tip_x = 64, prev_tip_y = 42;
    static int prev_tail_x = 64, prev_tail_y = 42;

    int cx = 64;
    int cy = 42;
    int r = 40;

    if (g_ui_needs_full_redraw) {
        fillScreen(0x0000);
        drawCircle(cx, cy, r, 0xFFFF);
        prev_c_lat = 999.0; prev_d_lat = 999.0; prev_dist = -1.0;
        prev_dest_name[0] = '\0';
        g_ui_needs_full_redraw = false;
    }

    fillCircle(prev_nx, prev_ny, 3, 0x0000);

    if (g_dest_lat != 0.0 && g_dest_lon != 0.0) {
        drawLine(cx, cy, prev_tip_x, prev_tip_y, 0x0000);
        drawLine(cx, cy, prev_tail_x, prev_tail_y, 0x0000);
        fillCircle(cx, cy, 2, 0x0000);
    }

    float north_dist = r - 6.0;
    float north_screen_angle = 360.0 - g_current_heading;
    float north_rad = (north_screen_angle - 90.0) * (M_PI / 180.0);

    int nx = cx + (north_dist * cos(north_rad));
    int ny = cy + (north_dist * sin(north_rad));
    fillCircle(nx, ny, 3, 0xF800);

    prev_nx = nx;
    prev_ny = ny;

    if (g_dest_lat != 0.0 && g_dest_lon != 0.0) {
        float arrow_rad = (g_current_angle_to - 90.0) * (M_PI / 180.0);
        float line_len = r - 10.0;

        int tip_x = cx + (line_len * cos(arrow_rad));
        int tip_y = cy + (line_len * sin(arrow_rad));
        int tail_x = cx - (line_len * cos(arrow_rad));
        int tail_y = cy - (line_len * sin(arrow_rad));

        drawLine(cx, cy, tip_x, tip_y, 0x001F);
        drawLine(cx, cy, tail_x, tail_y, 0xFFFF);
        fillCircle(cx, cy, 2, 0xF800);

        prev_tip_x = tip_x;
        prev_tip_y = tip_y;
        prev_tail_x = tail_x;
        prev_tail_y = tail_y;
    }

    char buffer[32];
    char temp_str[16];

    if (strcmp((char*)g_dest_name, prev_dest_name) != 0) {
        fillRect(0, 88, 128, 8, 0x0000);
        strcpy(buffer, "To: ");
        strcat(buffer, (char*)g_dest_name);
        drawText(0, 88, buffer, 0xFFE0, 0x0000, 1);
        strcpy(prev_dest_name, (char*)g_dest_name);
    }

    if (g_current_lat != prev_c_lat || g_current_lon != prev_c_lon) {
        fillRect(0, 98, 128, 8, 0x0000);
        strcpy(buffer, "C:");
        float_to_string(g_current_lat, temp_str);
        strcat(buffer, temp_str);
        strcat(buffer, ",");
        float_to_string(g_current_lon, temp_str);
        strcat(buffer, temp_str);
        drawText(0, 98, buffer, 0xFFFF, 0x0000, 1);
        prev_c_lat = g_current_lat;
        prev_c_lon = g_current_lon;
    }

    if (g_dest_lat != prev_d_lat || g_dest_lon != prev_d_lon) {
        fillRect(0, 108, 128, 8, 0x0000);
        if (g_dest_lat == 0.0 && g_dest_lon == 0.0) {
             drawText(0, 108, "D: None", 0xFFFF, 0x0000, 1);
        } else {
            strcpy(buffer, "D:");
            float_to_string(g_dest_lat, temp_str);
            strcat(buffer, temp_str);
            strcat(buffer, ",");
            float_to_string(g_dest_lon, temp_str);
            strcat(buffer, temp_str);
            drawText(0, 108, buffer, 0xFFFF, 0x0000, 1);
        }
        prev_d_lat = g_dest_lat;
        prev_d_lon = g_dest_lon;
    }

    if (g_current_distance != prev_dist) {
        fillRect(0, 118, 128, 8, 0x0000);
        if (g_dest_lat != 0.0 && g_dest_lon != 0.0) {
            strcpy(buffer, "Dist: ");
            float_to_string(g_current_distance, temp_str);
            strcat(buffer, temp_str);
            strcat(buffer, " mi");
            drawText(0, 118, buffer, 0x07E0, 0x0000, 1);
        }
        prev_dist = g_current_distance;
    }
}

//*****************************************************************************
//
//! BNO055 Compass Initialization and Reading Functions
//
//*****************************************************************************
int check_calibration_status() {
    unsigned char ucRegAddr = BNO055_CALIB_STAT_ADDR;
    unsigned char ucData = 0;

    I2C_IF_Write(BNO055_I2C_ADDR, &ucRegAddr, 1, 0);
    I2C_IF_Read(BNO055_I2C_ADDR, &ucData, 1);

    int mag = ucData & 0x03;
    return mag;
}

void init_BNO055() {
    unsigned char ucData[2];

    ucData[0] = BNO055_OPR_MODE_ADDR;
    ucData[1] = OPERATION_MODE_NDOF;

    I2C_IF_Write(BNO055_I2C_ADDR, ucData, 2, 1);
    MAP_UtilsDelay(800000); // 7ms wait required by datasheet
}

float get_BNO055_heading() {
    unsigned char ucRegAddr = BNO055_EUL_HEADING_LSB;
    unsigned char ucData[2] = {0};

    I2C_IF_Write(BNO055_I2C_ADDR, &ucRegAddr, 1, 0);
    I2C_IF_Read(BNO055_I2C_ADDR, &ucData[0], 2);

    int16_t raw_heading = ((int16_t)ucData[1] << 8) | ucData[0];
    return (float)raw_heading / 16.0;
}

//*****************************************************************************
//
//! Navigation Math Logic (Calculates distance in Miles)
//
//*****************************************************************************
void calculate_navigation() {
    float lat_rad = g_current_lat * (M_PI / 180.0);

    float del_x_miles = (g_dest_lon - g_current_lon) * 69.172 * cos(lat_rad);
    float del_y_miles = (g_dest_lat - g_current_lat) * 69.172;

    g_current_distance = sqrt((del_x_miles * del_x_miles) + (del_y_miles * del_y_miles));

    if (g_current_distance == 0.0) return;

    float rad_angle_from_0 = atan2(del_y_miles, del_x_miles);
    float deg_angle_from_0 = (rad_angle_from_0 * 180.0) / M_PI;

    float compass_angle_to_dest = 90.0 - deg_angle_from_0;
    if (compass_angle_to_dest < 0) {
        compass_angle_to_dest += 360.0;
    }

    g_current_angle_to = compass_angle_to_dest - g_current_heading;

    if (g_current_angle_to < 0) {
        g_current_angle_to += 360.0;
    } else if (g_current_angle_to >= 360.0) {
        g_current_angle_to -= 360.0;
    }
}

//*****************************************************************************
//
//! Interrupt Handlers & Board Setup
//
//*****************************************************************************
void GPIOIntHandler(void)
{
    unsigned long status;
    uint32_t currTick, deltaTicks, deltaUs;

    status = MAP_GPIOIntStatus(IR_GPIO_PORT, true);
    MAP_GPIOIntClear(IR_GPIO_PORT, status);

    if(status & IR_GPIO_PIN)
    {
        currTick = MAP_SysTickValueGet();
        deltaTicks = (g_ulLastTick - currTick) & SYSTICK_MAX;
        g_ulLastTick = currTick;
        deltaUs = deltaTicks / TICKS_PER_US;

        if (deltaUs > 10000) {
            if (g_iBitCount > 50) {
                g_ullFinalCode = g_ullFrameData;
                g_bCodeReady = 1;
            }
            g_iBitCount = 0;
            g_ullFrameData = 0;
        }
        else {
            int totalSlots = (deltaUs + 68) / 136;
            if (totalSlots > 0 && totalSlots < 20) {
                g_ullFrameData <<= totalSlots;
                uint64_t pulsePattern = (1ULL << (totalSlots - 1)) - 1;
                g_ullFrameData |= pulsePattern;
                g_iBitCount += totalSlots;
            }
        }
    }
}

static void BoardInit(void)
{
    MAP_IntMasterEnable();
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
    PRCMCC3200MCUInit();

    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
    MAP_SPIReset(GSPI_BASE);
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI), 10000000, SPI_MODE_MASTER, SPI_SUB_MODE_0,
                           (SPI_HW_CTRL_CS |
                            SPI_4PIN_MODE |
                            SPI_TURBO_OFF |
                            SPI_CS_ACTIVELOW |
                            SPI_WL_8));
    MAP_SPIEnable(GSPI_BASE);
}

static int set_time() {
    long retVal;
    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}

//*****************************************************************************
//
//! HTTP GET - Fetches Shadow Data & Parses Coordinates
//
//*****************************************************************************
static int http_get(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        sl_Close(iTLSSockID);
        return lRetVal;
    }

    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';

        char *ptr_lat = strstr(acRecvbuff, "\"latitude\":");
        char *ptr_lon = strstr(acRecvbuff, "\"longitude\":");

        if (ptr_lat != NULL && ptr_lon != NULL) {
            char lat_str[32] = {0};
            char lon_str[32] = {0};

            ptr_lat += strlen("\"latitude\":");
            ptr_lon += strlen("\"longitude\":");

            while(*ptr_lat == ' ') ptr_lat++;
            while(*ptr_lon == ' ') ptr_lon++;

            int i = 0;
            while(ptr_lat[i] != ',' && ptr_lat[i] != '}' && ptr_lat[i] != ' ' && ptr_lat[i] != '\r' && ptr_lat[i] != '\n' && i < 31) {
                lat_str[i] = ptr_lat[i];
                i++;
            }
            lat_str[i] = '\0';

            i = 0;
            while(ptr_lon[i] != ',' && ptr_lon[i] != '}' && ptr_lon[i] != ' ' && ptr_lon[i] != '\r' && ptr_lon[i] != '\n' && i < 31) {
                lon_str[i] = ptr_lon[i];
                i++;
            }
            lon_str[i] = '\0';

            g_current_lat = atof(lat_str);
            g_current_lon = atof(lon_str);

            if(g_dest_lat != 0.0 && g_dest_lon != 0.0) {
                calculate_navigation();
            }
        }
    }
    return 0;
}

//*****************************************************************************
//
//! Main
//
//*****************************************************************************
void main() {
    long lRetVal = -1;

    BoardInit();
    PinMuxConfig();
    InitTerm();
    ClearTerm();

    // Initialize Hardware
    I2C_IF_Open(I2C_MASTER_MODE_FST);
    init_BNO055();
    Adafruit_Init();

    fillScreen(0x0000);
    drawText(10, 40, "Hardware OK!", 0x07E0, 0x0000, 1);
    drawText(10, 60, "Connecting Wi-Fi...", 0xFFFF, 0x0000, 1);

    MAP_SysTickPeriodSet(SYSTICK_MAX);
    MAP_SysTickEnable();
    g_ulLastTick = MAP_SysTickValueGet();

    GPIO_IF_ConfigureNIntEnable((unsigned long) IR_GPIO_PORT,
                                      (unsigned char) IR_GPIO_PIN,
                                      0,
                                      GPIOIntHandler);

    uint32_t debounce = 0;

    g_app_config.host = SERVER_NAME;
    g_app_config.port = GOOGLE_DST_PORT;

    // Connect to Wi-Fi and time servers (Blocking code)
    lRetVal = connectToAccessPoint();
    lRetVal = set_time();

    int tls_socket = tls_connect();

    // --- ADDED TERMINAL TRACKING HERE ---
    UART_PRINT("\n\r[DEBUG] Sending HTTP GET to AWS...\n\r");

    // --- INITIAL FETCH ---
    fillScreen(0x0000);
    drawText(10, 60, "Connecting to AWS...", 0xFFFF, 0x0000, 1);
    http_get(tls_socket);

    UART_PRINT("[DEBUG] HTTP GET Complete!\n\r");

    // --- CALIBRATION STATE ---
    fillScreen(0x0000);
    drawText(10, 50, "Calibrating Mag...", 0xF800, 0x0000, 1);
    drawText(10, 65, "Do Figure 8!", 0xFFFF, 0x0000, 1);

    UART_PRINT("[DEBUG] Entering Compass Calibration. WAVE THE BOARD IN A FIGURE 8!\n\r");

    while (check_calibration_status() < 3) {
        UART_PRINT("."); // Prints dots while waiting for you to wave it
        MAP_UtilsDelay(8000000); // 0.5s pause
    }

    UART_PRINT("\n\r[DEBUG] Calibration Complete! Entering Menu.\n\r");

    // Enter initial state and draw menu
    g_current_state = STATE_MENU;
    draw_menu();

    uint32_t last_poll_time = MAP_PRCMSlowClkCtrGet();
    uint32_t last_compass_poll = MAP_PRCMSlowClkCtrGet();
    uint32_t current_time = 0;

    // --- MAIN EXECUTION LOOP ---
    while(FOREVER)
    {
        current_time = MAP_PRCMSlowClkCtrGet();

        // 1. --- NON-BLOCKING COMPASS & UI UPDATE (10 Hz) ---
        if ((current_time - last_compass_poll) >= (SLOW_CLK_FREQ / 10)) {
            g_current_heading = get_BNO055_heading();

            if(g_dest_lat != 0.0 && g_dest_lon != 0.0) {
                 calculate_navigation();
            }

            // UI will only redraw if state == STATE_NAVIGATING
            update_oled_ui();
            last_compass_poll = current_time;
        }

        // 2. --- NON-BLOCKING 5-SECOND SHADOW POLLING ---
        if ((current_time - last_poll_time) >= POLL_INTERVAL_TICKS) {
            http_get(tls_socket);
            last_poll_time = current_time;
        }

        // 3. --- CHECK FOR IR REMOTE PRESSES ---
        MAP_GPIOIntDisable(IR_GPIO_PORT, IR_GPIO_PIN);
        uint64_t code = g_ullFinalCode;
        g_ullFinalCode = 0;
        int ready = g_bCodeReady;
        g_bCodeReady = 0;
        MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);

        if(ready && code != 0 && code != 0xFFFFFFFFFFFFFFFFULL)
        {
            if (((debounce - MAP_SysTickValueGet()) & SYSTICK_MAX) < 12000000) {
                continue;
            }
            debounce = MAP_SysTickValueGet();
            code &= 0x0000000000000fff; // Apply mask

            // --- STATE: MENU ---
            if (g_current_state == STATE_MENU) {
                bool selected = false;

                // [1] ARC
                if (code == 0xb6f || code == 0xdb7 || code == 0xb2f
                    || code == 0xbde || code == 0x2de || code == 0xcb3
                    || code == 0xb2b || code == 0xb66 || code == 0x6de
                    || code == 0x926  || code == 0x65e || code == 0x4b3) {
                    g_dest_lat = 38.542920; g_dest_lon = -121.759146;
                    strcpy((char*)g_dest_name, "ARC");
                    selected = true;
                }
                // [2] Kemper Hall
                else if (code == 0x6df || code == 0x96f || code == 0x92f
                        || code == 0xdb3 || code == 0xc93 || code == 0xdbe
                        || code == 0x65f || code == 0xb67 || code == 0x64e
                        || code == 0xcb6 || code == 0x25f || code == 0xb27
                        || code == 0x967 || code == 0x927 || code == 0x2df) {
                    g_dest_lat = 38.537223; g_dest_lon = -121.754982;
                    strcpy((char*)g_dest_name, "Kemper");
                    selected = true;
                }
                // [3] Bainer Hall
                else if (code == 0xdbf || code == 0xb7e || code == 0xb3e
                        || code == 0x5bf || code == 0x97e || code == 0xc9f
                        || code == 0x59f || code == 0xcbf || code == 0x4bf
                        || code == 0xd9f) {
                    g_dest_lat = 38.537223; g_dest_lon = -121.753030;
                    strcpy((char*)g_dest_name, "Bainer");
                    selected = true;
                }
                // [4] Young Hall
                else if (code == 0xb7b || code == 0xdbb || code == 0x97b
                        || code == 0xd99 || code == 0xd9b || code == 0x2f2
                        || code == 0x2f6 || code == 0x59b || code == 0x6f2
                        || code == 0x932 || code == 0x936 || code == 0x979
                        || code == 0xb36 || code == 0xc99 || code == 0x6f6) {
                    g_dest_lat = 38.542406; g_dest_lon = -121.748253;
                    strcpy((char*)g_dest_name, "Young Hall");
                    selected = true;
                }
                // [5] MU
                else if (code == 0xdef || code == 0x6ef || code == 0x6f7
                        || code == 0x66f || code == 0x6f3 || code == 0x5ef
                        || code == 0x2f3 || code == 0xf7d  || code == 0xde6
                        || code == 0x266 || code == 0xcde || code == 0xeff) {
                    g_dest_lat = 38.542413; g_dest_lon = -121.749545;
                    strcpy((char*)g_dest_name, "MU");
                    selected = true;
                }
                // [6] Cows
                else if (code == 0xbdf || code == 0xddf || code == 0x5e7
                        || code == 0xcdf || code == 0xde7 || code == 0xbce
                        || code == 0x7be || code == 0xcce) {
                    g_dest_lat = 38.536106; g_dest_lon = -121.759507;
                    strcpy((char*)g_dest_name, "Cows");
                    selected = true;
                }
                // [7] Stockton
                else if (code == 0x7bf || code == 0xbbf || code == 0xf7e
                        || code == 0x79f || code == 0x9bf || code == 0xf3e
                        || code == 0x99f || code == 0x37e) {
                    g_dest_lat = 37.957500; g_dest_lon = -121.292500;
                    strcpy((char*)g_dest_name, "Stockton");
                    selected = true;
                }
                // [8] Wellman Hall
                else if (code == 0x6fb || code == 0x6f9 || code == 0x2fb
                        || code == 0x672 || code == 0xdf6 || code == 0xb39
                        || code == 0xdf2 || code == 0x2f9 || code == 0x676
                        || code == 0xb3b || code == 0x93b) {
                    g_dest_lat = 38.541524; g_dest_lon = -121.751514;
                    strcpy((char*)g_dest_name, "Wellman Hall");
                    selected = true;
                }
                // [9] Raising Cane's
                else if (code == 0xbef || code == 0xce6 || code == 0x7de
                        || code == 0xdf3 || code == 0x273 || code == 0xbe6
                        || code == 0x5f3 || code == 0x673 || code == 0xcef
                        || code == 0x9de || code == 0x4e6) {
                    g_dest_lat = 38.543740; g_dest_lon = -121.741516;
                    strcpy((char*)g_dest_name, "Cane's");
                    selected = true;
                }
                // [0] Middle of the 80 Freeway
                else if (code == 0x6db || code == 0x6cb || code == 0xedb
                        || code == 0x2c9 || code == 0x2db || code == 0x649
                        || code == 0x25b || code == 0x496 || code == 0xdb2
                        || code == 0x65b || code == 0x6c9) {
                    g_dest_lat = 38.547904; g_dest_lon = -121.716231;
                    strcpy((char*)g_dest_name, "80 Freeway");
                    selected = true;
                }
                // [DELETE] Pure Compass Mode
                else if (code == 0xfdf || code == 0xfbe || code == 0x7e7
                        || code == 0xfce || code == 0xfcf || code == 0x7df
                        || code == 0x9e7 || code == 0x3e7) {
                    g_dest_lat = 0.0; g_dest_lon = 0.0;
                    strcpy((char*)g_dest_name, "Compass Only");
                    selected = true;
                }

                // If a valid selection was made, transition to Navigation state
                if (selected) {
                    if (g_dest_lat != 0.0 && g_dest_lon != 0.0) {
                        calculate_navigation();
                    }
                    g_current_state = STATE_NAVIGATING;
                    g_ui_needs_full_redraw = true; // Tell UI to clear screen and draw compass
                }
            }
            // --- STATE: NAVIGATING ---
            else if (g_current_state == STATE_NAVIGATING) {
                // [ENTER] Return to Menu
                if (code == 0xf6f || code == 0xfb7 || code == 0xf77
                        || code == 0xeef || code == 0xfb3 || code == 0xdde
                        || code == 0x7b3) {
                    g_current_state = STATE_MENU;
                    draw_menu();
                }
            }
        }
    }
}
