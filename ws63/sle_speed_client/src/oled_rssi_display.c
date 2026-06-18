#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "pinctrl.h"
#include "soc_osal.h"
#include "i2c.h"

#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "oled_rssi_display.h"

/*
 * 宸茬粡閫氳繃 OLED Hello World 瀹炵墿娴嬭瘯鐨勯厤缃€? */
#define OLED_I2C_SCL_PIN        15
#define OLED_I2C_SDA_PIN        16
#define OLED_I2C_PIN_MODE       2
#define OLED_I2C_BUS_ID         1
#define OLED_I2C_BAUDRATE       400000
#define OLED_I2C_HSCODE         0

#define OLED_TASK_STACK_SIZE    0x1800
#define OLED_TASK_PRIORITY      28

/*
 * 浣跨敤 4 涓?RSSI 鏍锋湰鍋氭粦鍔ㄥ钩鍧囥€? * 鐩告瘮鍘熸潵鐨?8 涓牱鏈紝鍝嶅簲閫熷害鏇村揩銆? */
#define RSSI_WINDOW_SIZE        2
#define RSSI_INVALID_VALUE      0x7F

/*
 * OLED 姣?200 ms 妫€鏌ヤ竴娆℃柊鏁版嵁銆? * 杩炵画 10 娆℃病鏈夋柊鏁版嵁锛屽嵆绾?2 绉掕秴鏃躲€? */
#define OLED_REFRESH_MS         100
#define RSSI_TIMEOUT_CYCLES     30

static volatile int8_t g_latest_rssi = -127;
static volatile int32_t g_rssi_sum = 0;
static volatile uint8_t g_rssi_count = 0;
static volatile uint8_t g_rssi_index = 0;
static volatile uint8_t g_rssi_valid = 0;
static volatile uint8_t g_oled_started = 0;

/*
 * 姣忔敹鍒颁竴涓柊鐨?RSSI锛屽簭鍙峰鍔犱竴娆°€? * OLED 浠诲姟鏍规嵁搴忓彿鍒ゆ柇鏁版嵁鏄笉鏄湡姝ｆ洿鏂颁簡銆? */
static volatile uint32_t g_rssi_seq = 0;

static int8_t g_rssi_window[RSSI_WINDOW_SIZE] = {0};

/*
 * 璺濈妯″瀷锛? *
 * A = -106 dBm
 * N = 2.8
 *
 * d = 10 ^ ((A - RSSI) / (10N))
 *
 * 琛ㄦ牸鑼冨洿锛? * RSSI -120 dBm 鑷?-70 dBm
 * 鍗曚綅锛氬帢绫? */
static const uint16_t g_distance_cm_table[] = {
    316, 291, 268, 247, 228, 210, 193, 178, 164, 151,
    139, 128, 118, 109, 100, 92, 85, 78, 72, 66,
    61, 56, 52, 48, 44, 40, 37, 34, 32, 29,
    27, 25, 23, 21, 19, 18, 16, 15, 14, 13,
    12, 11, 10, 9, 8, 8, 7, 7, 6, 6, 5
};

static uint16_t rssi_to_distance_cm(int32_t rssi)
{
    if (rssi < -120) {
        rssi = -120;
    }

    if (rssi > -70) {
        rssi = -70;
    }

    return g_distance_cm_table[rssi + 120];
}

/*
 * 鐢?SLE 鎵弿鍥炶皟璋冪敤銆? *
 * 杩欓噷鍙洿鏂板嚑涓彉閲忥紝涓嶅湪钃濈墮鍥炶皟绾跨▼涓搷浣?I2C銆? */
void oled_rssi_display_update(int8_t rssi)
{
    if ((uint8_t)rssi == RSSI_INVALID_VALUE) {
        return;
    }

    if (g_rssi_count < RSSI_WINDOW_SIZE) {
        g_rssi_window[g_rssi_index] = rssi;
        g_rssi_sum += rssi;
        g_rssi_count++;
    } else {
        g_rssi_sum -= g_rssi_window[g_rssi_index];
        g_rssi_window[g_rssi_index] = rssi;
        g_rssi_sum += rssi;
    }

    g_rssi_index++;

    if (g_rssi_index >= RSSI_WINDOW_SIZE) {
        g_rssi_index = 0;
    }

    g_latest_rssi = rssi;
    g_rssi_valid = 1;
    g_rssi_seq++;
}

static void oled_show_waiting(void)
{
    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString(
        "SLE RSSI METER",
        Font_7x10,
        White
    );

    ssd1306_SetCursor(0, 22);
    ssd1306_DrawString(
        "WAITING SIGNAL",
        Font_7x10,
        White
    );

    ssd1306_SetCursor(0, 42);
    ssd1306_DrawString(
        "BS21: CHECK",
        Font_7x10,
        White
    );

    ssd1306_UpdateScreen();
}

static void oled_show_measurement(
    int32_t latest,
    int32_t average,
    uint16_t distance_cm
)
{
    char line[24];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString(
        "SLE RSSI METER",
        Font_7x10,
        White
    );

    snprintf(
        line,
        sizeof(line),
        "RSSI:%d dBm",
        (int)latest
    );

    ssd1306_SetCursor(0, 16);
    ssd1306_DrawString(
        line,
        Font_7x10,
        White
    );

    snprintf(
        line,
        sizeof(line),
        "AVG :%d dBm",
        (int)average
    );

    ssd1306_SetCursor(0, 32);
    ssd1306_DrawString(
        line,
        Font_7x10,
        White
    );

    snprintf(
        line,
        sizeof(line),
        "EST :%u.%02u m",
        distance_cm / 100,
        distance_cm % 100
    );

    ssd1306_SetCursor(0, 48);
    ssd1306_DrawString(
        line,
        Font_7x10,
        White
    );

    ssd1306_UpdateScreen();
}

static void oled_rssi_task(void)
{
    uint32_t ret;
    uint32_t last_seq = 0;
    uint8_t stale_cycles = 0;

    uapi_pin_set_mode(
        OLED_I2C_SCL_PIN,
        OLED_I2C_PIN_MODE
    );

    uapi_pin_set_mode(
        OLED_I2C_SDA_PIN,
        OLED_I2C_PIN_MODE
    );

    ret = uapi_i2c_master_init(
        OLED_I2C_BUS_ID,
        OLED_I2C_BAUDRATE,
        OLED_I2C_HSCODE
    );

    if (ret != 0) {
        printf(
            "[OLED] i2c init failed, ret=0x%x\r\n",
            ret
        );
        return;
    }

    ssd1306_Init();
    oled_show_waiting();

    printf("[OLED] RSSI display task started\r\n");

    while (1) {
        uint32_t current_seq = g_rssi_seq;

        /*
         * 搴忓彿鍙樺寲浠ｈ〃鐪熸鏀跺埌浜嗘柊鐨?RSSI銆?         */
        if ((current_seq != last_seq) &&
            g_rssi_valid &&
            (g_rssi_count > 0)) {
            int32_t latest = g_latest_rssi;
            int32_t count = g_rssi_count;
            int32_t sum = g_rssi_sum;
            int32_t average;

            /*
             * 璐熸暟骞冲潎鍊煎洓鑸嶄簲鍏ャ€?             */
            if (sum < 0) {
                average = (sum - count / 2) / count;
            } else {
                average = (sum + count / 2) / count;
            }

            uint16_t distance_cm =
                rssi_to_distance_cm(average);

            last_seq = current_seq;
            stale_cycles = 0;

            oled_show_measurement(
                latest,
                average,
                distance_cm
            );

            printf(
                "[OLED] SEQ=%u RSSI=%d AVG=%d EST=%u.%02u m\r\n",
                (unsigned int)current_seq,
                (int)latest,
                (int)average,
                distance_cm / 100,
                distance_cm % 100
            );
        } else {
            /*
             * 娌℃湁鏀跺埌鏂版暟鎹椂绱瓒呮椂娆℃暟銆?             */
            if (stale_cycles < RSSI_TIMEOUT_CYCLES) {
                stale_cycles++;
            }

            /*
             * 绾?2 绉掓病鏈夋柊 RSSI锛屾樉绀虹瓑寰呯姸鎬併€?             */
            if (stale_cycles == RSSI_TIMEOUT_CYCLES) {
                oled_show_waiting();

                printf(
                    "[OLED] RSSI timeout, waiting for new signal\r\n"
                );

                /*
                 * 閬垮厤閲嶅鍒锋柊绛夊緟鐢婚潰銆?                 */
                stale_cycles++;
            }
        }

        osal_msleep(OLED_REFRESH_MS);
    }
}

void oled_rssi_display_start(void)
{
    osal_task *task;
    uint32_t ret;

    if (g_oled_started) {
        return;
    }

    g_oled_started = 1;

    osal_kthread_lock();

    task = osal_kthread_create(
        (osal_kthread_handler)oled_rssi_task,
        NULL,
        "OledRssiTask",
        OLED_TASK_STACK_SIZE
    );

    if (task == NULL) {
        g_oled_started = 0;
        osal_kthread_unlock();

        printf("[OLED] create task failed\r\n");
        return;
    }

    ret = osal_kthread_set_priority(
        task,
        OLED_TASK_PRIORITY
    );

    osal_kthread_unlock();

    if (ret != OSAL_SUCCESS) {
        printf(
            "[OLED] set task priority failed\r\n"
        );
    }
}