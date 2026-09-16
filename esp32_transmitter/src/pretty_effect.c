#include <math.h>
#include <stdio.h>
#include <string.h>
#include "pretty_effect.h"
#include "sdkconfig.h"
#include "decode_image.h"
#include "transmit_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "joystick.h"
#include "metrics.h"
#include "font5x7.h"
#include "imu.h"
#include <stdbool.h>
#define SWAP16(c) (((c) >> 8) | (((c) & 0xFF) << 8))

uint16_t *pixels = NULL;
extern bool failsafe_flag;

#define COLOR_JOYSTICK      SWAP16(0xFD00)
#define COLOR_DOT_BORDER    SWAP16(0xC260)
#define COLOR_PULSE_YELLOW  SWAP16(0xFF66) // #FFEE33
#define COLOR_PULSE_GOLD    SWAP16(0xFD40) // #FFAA00
#define COLOR_TEXT_CYAN     SWAP16(0x07FF) // #00FFFF Electric Cyan

extern bool auto_running;
extern bool imu_running;

// Center Y of each pill button on the 240x320 screen
static const int pill_center_y[TOTAL_MODES] = {
    [MENU_MODE]   = 126,
    [MANUAL_MODE] = 126,
    [AUTO_MODE]   = 170,
    [IMU_MODE]    = 214
};

typedef struct {
    char str[12];
    int x0;
    int y0;
    int len;
} ui_text_item_t;

static ui_text_item_t s_ui_texts[11];

static void prepare_manual_ui_strings(void) {

    // 1. Joystick X (-100 to +100)
    int16_t jx = metrics_get_joy_x_val();
    snprintf(s_ui_texts[0].str, sizeof(s_ui_texts[0].str), "%+d", jx);
    s_ui_texts[0].len = strlen(s_ui_texts[0].str);
    s_ui_texts[0].x0 = 73 - (s_ui_texts[0].len * 6) / 2;
    s_ui_texts[0].y0 = 128;

    // 2. Joystick Y (-100 to +100)
    int16_t jy = metrics_get_joy_y_val();
    snprintf(s_ui_texts[1].str, sizeof(s_ui_texts[1].str), "%+d", jy);
    s_ui_texts[1].len = strlen(s_ui_texts[1].str);
    s_ui_texts[1].x0 = 73 - (s_ui_texts[1].len * 6) / 2;
    s_ui_texts[1].y0 = 145;

    // 3. Speed Setting (0-100%)
    uint8_t spd = metrics_get_speed_percent();
    snprintf(s_ui_texts[2].str, sizeof(s_ui_texts[2].str), "%u%%", spd);
    s_ui_texts[2].len = strlen(s_ui_texts[2].str);
    s_ui_texts[2].x0 = 193 - (s_ui_texts[2].len * 6) / 2;
    s_ui_texts[2].y0 = 58;

       //11 failsafe 
    //if we have the flag as true, then we must change the UI
    if (failsafe_flag == true) {
        //then we draw the word failsafe word tripped
        snprintf(s_ui_texts[3].str, sizeof(s_ui_texts[3].str), "TRIPPED");
    }
    else if (current_page == PAGE_MANUAL || current_page == PAGE_MANUAL_DATA) {
        snprintf(s_ui_texts[3].str, sizeof(s_ui_texts[3].str), "ARMED");
    } 
    else {
         snprintf(s_ui_texts[3].str, sizeof(s_ui_texts[3].str), "IDLE");
    }

    s_ui_texts[3].len = strlen(s_ui_texts[3].str);
    s_ui_texts[3].x0 = 193 - (s_ui_texts[3].len * 6) / 2;
    s_ui_texts[3].y0 = 99;

    // 5. Direction String
    const char *dir = metrics_get_direction_str();
    snprintf(s_ui_texts[4].str, sizeof(s_ui_texts[4].str), "%s", dir);
    s_ui_texts[4].len = strlen(s_ui_texts[4].str);
    s_ui_texts[4].x0 = 193 - (s_ui_texts[4].len * 6) / 2;
    s_ui_texts[4].y0 = 139;

    // 6. CPU Load %
    float cpu = metrics_get_cpu_load();
    snprintf(s_ui_texts[5].str, sizeof(s_ui_texts[5].str), "%.1f", cpu);
    s_ui_texts[5].len = strlen(s_ui_texts[5].str);
    s_ui_texts[5].x0 = 172 - (s_ui_texts[5].len * 6) / 2;
    s_ui_texts[5].y0 = 201;

    // 7. Latency (ms)
    float lat = metrics_get_latency_ms();
    snprintf(s_ui_texts[6].str, sizeof(s_ui_texts[6].str), "%.1f", lat);
    s_ui_texts[6].len = strlen(s_ui_texts[6].str);
    s_ui_texts[6].x0 = 172 - (s_ui_texts[6].len * 6) / 2;
    s_ui_texts[6].y0 = 220;

    // 8. Jitter (ms)
    float jit = metrics_get_jitter_ms();
    snprintf(s_ui_texts[7].str, sizeof(s_ui_texts[7].str), "%.1f", jit);
    s_ui_texts[7].len = strlen(s_ui_texts[7].str);
    s_ui_texts[7].x0 = 172 - (s_ui_texts[7].len * 6) / 2;
    s_ui_texts[7].y0 = 239;

    // 9. Missed Deadlines
    uint32_t mdl = metrics_get_missed_deadlines();
    snprintf(s_ui_texts[8].str, sizeof(s_ui_texts[8].str), "%lu", (unsigned long)mdl);
    s_ui_texts[8].len = strlen(s_ui_texts[8].str);
    s_ui_texts[8].x0 = 172 - (s_ui_texts[8].len * 6) / 2;
    s_ui_texts[8].y0 = 259;

    // 10. Control Rate (Hz)
    float hz = metrics_get_control_rate_hz();
    snprintf(s_ui_texts[9].str, sizeof(s_ui_texts[9].str), "%.1f", hz);
    s_ui_texts[9].len = strlen(s_ui_texts[9].str);
    s_ui_texts[9].x0 = 172 - (s_ui_texts[9].len * 6) / 2;
    s_ui_texts[9].y0 = 280;

 
}

static inline bool check_text_pixel(int x, int y) {
    for (int i = 0; i < 11; i++) {
        int y0 = s_ui_texts[i].y0;
        if (y >= y0 && y < y0 + 7) {
            int x0 = s_ui_texts[i].x0;
            int total_w = s_ui_texts[i].len * 6;
            if (x >= x0 && x < x0 + total_w) {
                int char_idx = (x - x0) / 6;
                int char_x0 = x0 + char_idx * 6;
                char c = s_ui_texts[i].str[char_idx];
                if (font5x7_get_pixel(c, char_x0, y0, x, y)) {
                    return true;
                }
            }
        }
    }
    return false;
}

static ui_text_item_t s_diag_texts[17];

static void prepare_diag_ui_strings(void) {
    // Read REAL physical joystick ADC values
    uint16_t raw_x = 2000, raw_y = 2000;
    get_joystick_raw_values(&raw_x, &raw_y);
    int16_t jx = metrics_get_joy_x_val();
    int16_t jy = metrics_get_joy_y_val();

    // === 1. JOYSTICK DATA (Top Left: Box Center X = 89) ===
    // Line 1: X ADC
    snprintf(s_diag_texts[0].str, sizeof(s_diag_texts[0].str), "%u", raw_x);
    s_diag_texts[0].len = strlen(s_diag_texts[0].str);
    s_diag_texts[0].x0 = 89 - (s_diag_texts[0].len * 6) / 2;
    s_diag_texts[0].y0 = 86;

    // Line 2: Y ADC
    snprintf(s_diag_texts[1].str, sizeof(s_diag_texts[1].str), "%u", raw_y);
    s_diag_texts[1].len = strlen(s_diag_texts[1].str);
    s_diag_texts[1].x0 = 89 - (s_diag_texts[1].len * 6) / 2;
    s_diag_texts[1].y0 = 104;

    // Line 3: X norm
    snprintf(s_diag_texts[2].str, sizeof(s_diag_texts[2].str), "%+d%%", jx);
    s_diag_texts[2].len = strlen(s_diag_texts[2].str);
    s_diag_texts[2].x0 = 89 - (s_diag_texts[2].len * 6) / 2;
    s_diag_texts[2].y0 = 121;

    // Line 4: Y norm
    snprintf(s_diag_texts[3].str, sizeof(s_diag_texts[3].str), "%+d%%", jy);
    s_diag_texts[3].len = strlen(s_diag_texts[3].str);
    s_diag_texts[3].x0 = 89 - (s_diag_texts[3].len * 6) / 2;
    s_diag_texts[3].y0 = 138;

    // === 2. MOTOR DATA (Top Right: Box Center X = 207) ===
    uint8_t spd = robot_packet_received ? robot_packet.speedSetting : metrics_get_speed_percent();
    // Line 1: Left PWM
    snprintf(s_diag_texts[4].str, sizeof(s_diag_texts[4].str), "%u%%", spd);
    s_diag_texts[4].len = strlen(s_diag_texts[4].str);
    s_diag_texts[4].x0 = 207 - (s_diag_texts[4].len * 6) / 2;
    s_diag_texts[4].y0 = 86;

    // Line 2: Right PWM
    snprintf(s_diag_texts[5].str, sizeof(s_diag_texts[5].str), "%u%%", spd);
    s_diag_texts[5].len = strlen(s_diag_texts[5].str);
    s_diag_texts[5].x0 = 207 - (s_diag_texts[5].len * 6) / 2;
    s_diag_texts[5].y0 = 104;

    // === 3. LINK DATA (Bottom Left: Box Center X = 94) ===
    // Line 1: Packets Rx
    uint32_t rx_pkts = metrics_get_rx_count();
    snprintf(s_diag_texts[6].str, sizeof(s_diag_texts[6].str), "%lu", (unsigned long)rx_pkts);
    s_diag_texts[6].len = strlen(s_diag_texts[6].str);
    s_diag_texts[6].x0 = 94 - (s_diag_texts[6].len * 6) / 2;
    s_diag_texts[6].y0 = 207;

    // Line 2: Packets Lost (%)
    float loss = metrics_get_packet_loss_pct();
    snprintf(s_diag_texts[7].str, sizeof(s_diag_texts[7].str), "%.1f%%", loss);
    s_diag_texts[7].len = strlen(s_diag_texts[7].str);
    s_diag_texts[7].x0 = 94 - (s_diag_texts[7].len * 6) / 2;
    s_diag_texts[7].y0 = 224;

    // Line 3: Last packet (ms)
    uint32_t tx_ms_ago = metrics_get_last_tx_ms_ago();
    snprintf(s_diag_texts[8].str, sizeof(s_diag_texts[8].str), "%lums", (unsigned long)tx_ms_ago);
    s_diag_texts[8].len = strlen(s_diag_texts[8].str);
    s_diag_texts[8].x0 = 94 - (s_diag_texts[8].len * 6) / 2;
    s_diag_texts[8].y0 = 242;

    // Line 4: RSSI (dBm)
    int8_t rssi = metrics_get_rssi();
    snprintf(s_diag_texts[9].str, sizeof(s_diag_texts[9].str), "%d dBm", rssi);
    s_diag_texts[9].len = strlen(s_diag_texts[9].str);
    s_diag_texts[9].x0 = 94 - (s_diag_texts[9].len * 6) / 2;
    s_diag_texts[9].y0 = 260;

    // === 4. STM32 PERFORMANCE (Bottom Right: Box Center X = 207) ===
    unsigned int cpu = robot_packet_received ? robot_packet.cpuLoad : 0;
    // Line 1: CPU LOAD
    snprintf(s_diag_texts[12].str, sizeof(s_diag_texts[12].str), "%u%%", cpu);
    s_diag_texts[12].len = strlen(s_diag_texts[12].str);
    s_diag_texts[12].x0 = 207 - (s_diag_texts[12].len * 6) / 2;
    s_diag_texts[12].y0 = 207;

    float lat = robot_packet_received ? robot_packet.latencyMs : 0.0f;
    // Line 2: LATENCY (ms)
    snprintf(s_diag_texts[13].str, sizeof(s_diag_texts[13].str), "%.2f", lat);
    s_diag_texts[13].len = strlen(s_diag_texts[13].str);
    s_diag_texts[13].x0 = 207 - (s_diag_texts[13].len * 6) / 2;
    s_diag_texts[13].y0 = 224;

    float jit = robot_packet_received ? robot_packet.jitterMs : 0.0f;
    // Line 3: JITTER
    snprintf(s_diag_texts[14].str, sizeof(s_diag_texts[14].str), "%.1f", jit);
    s_diag_texts[14].len = strlen(s_diag_texts[14].str);
    s_diag_texts[14].x0 = 207 - (s_diag_texts[14].len * 6) / 2;
    s_diag_texts[14].y0 = 241;

    unsigned int mdl = robot_packet_received ? robot_packet.missedDeadlines : 0;
    // Line 4: MISSED DL
    snprintf(s_diag_texts[15].str, sizeof(s_diag_texts[15].str), "%u", mdl);
    s_diag_texts[15].len = strlen(s_diag_texts[15].str);
    s_diag_texts[15].x0 = 207 - (s_diag_texts[15].len * 6) / 2;
    s_diag_texts[15].y0 = 258;

    float hz = robot_packet_received ? robot_packet.controlRate : 0.0f;
    // Line 5: LOOP RATE
    snprintf(s_diag_texts[16].str, sizeof(s_diag_texts[16].str), "%.1f", hz);
    s_diag_texts[16].len = strlen(s_diag_texts[16].str);
    s_diag_texts[16].x0 = 207 - (s_diag_texts[16].len * 6) / 2;
    s_diag_texts[16].y0 = 275;
}

static inline bool check_diag_text_pixel(int x, int y) {
    for (int i = 0; i < 17; i++) {
        int y0 = s_diag_texts[i].y0;
        if (y >= y0 && y < y0 + 7) {
            int x0 = s_diag_texts[i].x0;
            int total_w = s_diag_texts[i].len * 6;
            if (x >= x0 && x < x0 + total_w) {
                int char_idx = (x - x0) / 6;
                int char_x0 = x0 + char_idx * 6;
                char c = s_diag_texts[i].str[char_idx];
                if (font5x7_get_pixel(c, char_x0, y0, x, y)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Grab an rgb16 pixel from decoded JPEG buffer
static inline uint16_t get_bgnd_pixel(int x, int y)
{
    x = (x < 0) ? 0 : (x >= IMAGE_W) ? IMAGE_W - 1 : x;
    y = (y < 0) ? 0 : (y >= IMAGE_H) ? IMAGE_H - 1 : y;
    return (uint16_t) * (pixels + (y * IMAGE_W) + x);
}

// Determine the pixel color based on current page and overlay elements
static inline uint16_t apply_overlay(int x, int y, uint16_t bg_pixel, uint16_t hover_color) {
    // 1. Menu mode cursor
    if (current_page == PAGE_MENU) {
        int cy = pill_center_y[hovered_mode];
        if (y >= cy - 22 && y <= cy + 21 && x >= 51 && x <= 190) {
            int dx = (x < 78) ? (78 - x) : (x > 170) ? (x - 170) : 0;
            int dy = (y < cy) ? (y - cy + 1) : (y - cy);
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq >= 335 && dist_sq <= 415) {
                return hover_color;
            }
        }
        return bg_pixel;
    }

    // 2. Manual mode: dynamic joystick dot & telemetry text
    if (current_page == PAGE_MANUAL) {
        static int dot_x = GRID_CENTER_X;
        static int dot_y = GRID_CENTER_Y;
        if (y == 0 && x == 0) {
            get_joystick_screen_coords(&dot_x, &dot_y);
            prepare_manual_ui_strings();
        }

        if (check_text_pixel(x, y)) {
            return COLOR_TEXT_CYAN;
        }

        if (y >= 35 && y <= 120 && x >= 18 && x <= 108) {
            int dx = x - dot_x;
            int dy = y - dot_y;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 16) {
                return (dist_sq >= 10) ? COLOR_DOT_BORDER : COLOR_JOYSTICK;
            }
        }
        return bg_pixel;
    }

    // 3. Diagnostics Page (PAGE_MANUAL_DATA)
    if (current_page == PAGE_MANUAL_DATA) {
        if (y == 0 && x == 0) {
            prepare_diag_ui_strings();
        }

        if (check_diag_text_pixel(x, y)) {
            return COLOR_TEXT_CYAN;
        }
        return bg_pixel;
    }




    // 4. Auto Mode Page dynamic button
    if (current_page == PAGE_AUTO) {


        //need to add the speed changing as we change the numbers
        char speed_str[8];
        uint8_t spd_pct = metrics_get_speed_percent();

        snprintf(speed_str, sizeof(speed_str), "%u%%", spd_pct);

        int spd_len = strlen(speed_str);
        int spd_x0 = 65-(spd_len*6)/2;
        int spd_y0 = 90;

          if (y >= spd_y0 && y < spd_y0 + 7 && x >= spd_x0 && x < spd_x0 + (spd_len * 6)) {
            int char_idx = (x - spd_x0) / 6;
            int char_x0 = spd_x0 + char_idx * 6;
            char c = speed_str[char_idx];

            if (font5x7_get_pixel(c, char_x0, spd_y0, x, y)) {
                return COLOR_TEXT_CYAN; // #00FFFF Electric Cyan text
            }
        }


        // Shifted down by +10 pixels: X [126 to 228], Y [54 to 98]
        if (x >= 126 && x <= 228 && y >= 54 && y <= 98) {
            
            // 1. Text: "RUNNING" vs "STOPPED" (Centered at Y=76, X=156)
            const char *label = auto_running ? "RUNNING" : "STOPPED";
            int text_x0 = 156;
            int text_y0 = 73;
            
            if (y >= text_y0 && y < text_y0 + 7 && x >= text_x0 && x < text_x0 + 42) {
                int char_idx = (x - text_x0) / 6;
                int char_x0 = text_x0 + char_idx * 6;
                char c = label[char_idx];
                if (font5x7_get_pixel(c, char_x0, text_y0, x, y)) {
                    return auto_running ? SWAP16(0x07E0) : SWAP16(0xF800);
                }
            }

            // 2. Icon: Play Arrow ▶ vs Stop Square ■ (Centered at Y=76)
            if (auto_running) {
                if (x >= 142 && x <= 148 && y >= 72 && y <= 80) {
                    int dy = (y >= 76) ? (y - 76) : (76 - y);
                    if ((x - 142) <= (4 - dy)) {
                        return SWAP16(0x07E0);
                    }
                }
            } 
            else {
                if (x >= 142 && x <= 148 && y >= 73 && y <= 79) {
                    return SWAP16(0xF800);
                }
            }

            // 3. Border vs Background Fill
            bool is_border = (x == 126 || x == 228 || y == 54 || y == 98 ||
                              x == 127 || x == 227 || y == 55 || y == 97);
            
            if (auto_running) {
                return is_border ? SWAP16(0x07E0) : SWAP16(0x01E0);
            } 
            else {
                return is_border ? SWAP16(0xF800) : SWAP16(0x3800);
            }
        }

        // === 1. 8 Vertical Reflectance Sensor Boxes on Auto Page ===
        // Channels 0 to 7 arranged across X [20 to 226], Y [181 to 248] (Shifted down by 6px)
        if (y >= 181 && y <= 248 && x >= 20 && x <= 226) {
            for (int ch = 0; ch < 8; ch++) {
                int box_x0 = 20 + (ch * 26);
                int box_w  = 20;
                int box_y0 = 181;
                int box_h  = 68;

                if (x >= box_x0 && x < box_x0 + box_w) {
                    bool is_border = (x == box_x0 || x == box_x0 + box_w - 1 || 
                                      y == box_y0 || y == box_y0 + box_h - 1);
                    
                    // If telemetry received, check bit 'ch'. If not connected yet, test pattern!
                    bool is_active = robot_packet_received ? 
                                     ((robot_packet.lineSensors & (1 << ch)) != 0) : 
                                     (ch == 3 || ch == 4); // Default test highlight middle 2 sensors

                    if (is_border) {
                        return SWAP16(0xFFE0); // Bright Yellow Outline Box
                    }
                    if (is_active) {
                        return SWAP16(0x0000); // Solid Pitch Black when line detected
                    } else {
                        return SWAP16(0xFFFF); // Solid White when ground is white
                    }
                }
            }
        }

        // === 2. 8 Bottom Ground Perspective Tiles on Auto Page ===
        // Channels 0 to 7 arranged across Y [260 to 282]
        if (y >= 260 && y <= 282 && x >= 10 && x <= 230) {
            static const int16_t top_xl[8] = {29, 50, 73, 97, 121, 145, 169, 193};
            static const int16_t top_xr[8] = {46, 70, 94, 118, 142, 165, 189, 209};
            static const int16_t bot_xl[8] = {11, 41, 69, 95, 121, 147, 174, 201};
            static const int16_t bot_xr[8] = {38, 65, 92, 118, 144, 171, 198, 227};

            int dy = y - 260;
            for (int ch = 0; ch < 8; ch++) {
                int xl = top_xl[ch] + ((bot_xl[ch] - top_xl[ch]) * dy) / 22;
                int xr = top_xr[ch] + ((bot_xr[ch] - top_xr[ch]) * dy) / 22;

                if (x >= xl && x <= xr) {
                    bool is_border = (x == xl || x == xr || y == 260 || y == 282);
                    bool is_active = robot_packet_received ? 
                                     ((robot_packet.lineSensors & (1 << ch)) != 0) : 
                                     (ch == 3 || ch == 4);

                    if (is_border) {
                        return SWAP16(0xFFE0); // Bright Yellow Outline
                    }
                    if (is_active) {
                        return SWAP16(0x0000); // Solid Pitch Black
                    } else {
                        return SWAP16(0xFFFF); // Solid White
                    }
                }
            }
        }

        return bg_pixel;
    }

    // 5. IMU Mode Page: Dynamic Tilt Dot, PWM Speed, Start/Stop Button & Live Values
    if (current_page == PAGE_IMU) {
        static int imu_dot_x = 88;
        static int imu_dot_y = 183;
        static int pitch_val = 0;
        static int roll_val = 0;

        static char speed_str[8];
        static char pitch_str[8];
        static char roll_str[8];

        static int spd_len = 0, spd_x0 = 0, pitch_len = 0, pitch_x0 = 0, roll_len = 0, roll_x0 = 0;


        if (y == 0 && x == 0) {
            imu_get_screen_coords(&imu_dot_x, &imu_dot_y);
            imu_get_tilt_deg(&pitch_val, &roll_val);

            //calculate strings once per frame here;
            snprintf(speed_str, sizeof(speed_str), "%u%%", metrics_get_speed_percent());
            spd_len = strlen(speed_str);
            spd_x0 = 66-(spd_len*6)/2;

            snprintf(pitch_str, sizeof(pitch_str), "%+d*", pitch_val);
            pitch_len = strlen(pitch_str);
            pitch_x0 = 203 - (pitch_len * 6) / 2;

            snprintf(roll_str, sizeof(roll_str), "%+d*", roll_val);
            roll_len = strlen(roll_str);
            roll_x0 = 203 - (roll_len * 6) / 2; //moves roll to the right
        }

        // A. Dynamic Tilt Dot in TILT / CALIBRATION Crosshair Box [X: 30..112, Y: 146..224]
        if (y >= 139 && y <= 224 && x >= 40 && x <= 130) {
            int dx = x - imu_dot_x;
            int dy = y - imu_dot_y;
            int dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 16) {
                return (dist_sq >= 10) ? COLOR_DOT_BORDER : COLOR_JOYSTICK;
            }
        }

        // B. PWM (%) Value in Top-Left SPEED MODE Card (Moved down by 5px to Y=58)
        int spd_y0 = 63; 

        if (y >= spd_y0 && y < spd_y0 + 7 && x >= spd_x0 && x < spd_x0 + (spd_len * 6)) {
            int char_idx = (x - spd_x0) / 6;
            int char_x0 = spd_x0 + char_idx * 6;
            char c = speed_str[char_idx];
            if (font5x7_get_pixel(c, char_x0, spd_y0, x, y)) {
                return COLOR_TEXT_CYAN;
            }
        }

        // C. Start / Stop Button (Narrower width: X [122 to 218], Y [46 to 78])
        if (x >= 126 && x <= 221 && y >= 40 && y <= 70) {
            // Text: "RUNNING" vs "STOPPED"
            const char *label = imu_running ? "RUNNING" : "STOPPED";
            int label_len = strlen(label);
            int text_x0 = 170 - (label_len * 6) / 2;
            int text_y0 = 52;
            if (y >= text_y0 && y < text_y0 + 7 && x >= text_x0 && x < text_x0 + (label_len * 6)) {
                int char_idx = (x - text_x0) / 6;
                int char_x0 = text_x0 + char_idx * 6;
                char c = label[char_idx];
                if (font5x7_get_pixel(c, char_x0, text_y0, x, y)) {
                    return imu_running ? SWAP16(0x07E0) : SWAP16(0xF800);
                }
            }

            // Icon: Play Arrow ▶ vs Stop Square ■ (Centered at X=138, Y=62)
            if (imu_running) {
                if (x >= 134 && x <= 140 && y >= 51 && y <= 59) {
                    int dy = (y >= 55) ? (y - 55) : (55 - y);
                    if ((x - 134) <= (4 - dy)) {
                        return SWAP16(0x07E0);
                    }
                }
            } else {
                if (x >= 134 && x <= 140 && y >= 52 && y <= 58) {
                    return SWAP16(0xF800);
                }
            }

            // Border vs Background Fill
            bool is_border = (x == 126 || x == 221 || y == 40 || y == 70 ||
                              x == 127 || x == 220 || y == 41 || y == 69);
            if (imu_running) {
                return is_border ? SWAP16(0x07E0) : SWAP16(0x01E0);
            } else {
                return is_border ? SWAP16(0xF800) : SWAP16(0x3800);
            }
        }

        // D. STATUS Box (Right of crosshair: Box Center X: 172, Y: 153)
        const char *status_str = imu_running ? "ARMED" : "IDLE";
        int stat_len = strlen(status_str);
        int stat_x0 = 186 - (stat_len * 6) / 2; //was 172
        int stat_y0 = 154;
        if (y >= stat_y0 && y < stat_y0 + 7 && x >= stat_x0 && x < stat_x0 + (stat_len * 6)) {
            int char_idx = (x - stat_x0) / 6;
            int char_x0 = stat_x0 + char_idx * 6;
            char c = status_str[char_idx];
            if (font5x7_get_pixel(c, char_x0, stat_y0, x, y)) {
                return imu_running ? SWAP16(0x07E0) : COLOR_TEXT_CYAN;
            }
        }

        // E. Pitch (°) Value inside Pitch Box (Box Center X: 196, Y: 199)
        int pitch_y0 = 217; //shifted down 7 pixels
        if (y >= pitch_y0 && y < pitch_y0 + 7 && x >= pitch_x0 && x < pitch_x0 + (pitch_len * 6)) {
            int char_idx = (x - pitch_x0) / 6;
            int char_x0 = pitch_x0 + char_idx * 6;
            char c = pitch_str[char_idx];
            if (font5x7_get_pixel(c, char_x0, pitch_y0, x, y)) {
                return COLOR_TEXT_CYAN;
            }
        }

        // F. Roll (°) Value inside Roll Box (Box Center X: 196, Y: 223)
        int roll_y0 = 237;
        if (y >= roll_y0 && y < roll_y0 + 7 && x >= roll_x0 && x < roll_x0 + (roll_len * 6)) {
            int char_idx = (x - roll_x0) / 6;
            int char_x0 = roll_x0 + char_idx * 6;
            char c = roll_str[char_idx];
            if (font5x7_get_pixel(c, char_x0, roll_y0, x, y)) {
                return COLOR_TEXT_CYAN;
            }
        }

        return bg_pixel;
    }

    return bg_pixel;
}

void pretty_effect_calc_lines(uint16_t *dest, int line, int frame, int linect)
{
    if (!pixels) return;

    bool pulse = ((xTaskGetTickCount() / pdMS_TO_TICKS(250)) % 2) == 0;
    uint16_t hover_color = pulse ? COLOR_PULSE_YELLOW : COLOR_PULSE_GOLD;

    for (int y = line; y < line + linect; y++) {
        for (int x = 0; x < 240; x++) {
            uint16_t bg = get_bgnd_pixel(x, y);
            *dest++ = apply_overlay(x, y, bg, hover_color);
        }
    }
}

esp_err_t pretty_effect_init(void)
{
    return decode_image(0, &pixels);
}
