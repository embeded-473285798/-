/*
 * WS63 + SSD1306 OLED
 * Simulated RSSI distance display demo
 */

#include <stdio.h>
#include <stdint.h>

#include "pinctrl.h"
#include "common_def.h"
#include "soc_osal.h"
#include "i2c.h"
#include "osal_debug.h"
#include "ssd1306_fonts.h"
#include "ssd1306.h"
#include "app_init.h"

/*
 * 这组引脚已经通过 Hello World 实物测试成功。
 */
#define CONFIG_I2C_SCL_MASTER_PIN 15
#define CONFIG_I2C_SDA_MASTER_PIN 16
#define CONFIG_I2C_MASTER_PIN_MODE 2

#define I2C_MASTER_BUS_ID 1
#define I2C_MASTER_ADDR 0x0
#define I2C_SET_BANDRATE 400000

#define OLED_TASK_STACK_SIZE 0x1800
#define OLED_TASK_PRIO 17

/*
 * 距离模型：
 *
 * d = 10 ^ ((A - RSSI) / (10N))
 *
 * 当前临时参数：
 * A = -106 dBm
 * N = 2.8
 *
 * 为避免在嵌入式端使用 pow() 和浮点格式化，
 * 这里使用预先计算好的厘米查找表。
 *
 * 下标 0 对应 -120 dBm；
 * 下标 50 对应 -70 dBm。
 */
static const uint16_t g_distance_cm_table[] = {
    316, 291, 268, 247, 228, 210, 193, 178, 164, 151,
    139, 128, 118, 109, 100, 92, 85, 78, 72, 66,
    61, 56, 52, 48, 44, 40, 37, 34, 32, 29,
    27, 25, 23, 21, 19, 18, 16, 15, 14, 13,
    12, 11, 10, 9, 8, 8, 7, 7, 6, 6, 5
};

/*
 * 自动循环的模拟 RSSI 数据。
 */
static const int8_t g_test_rssi_values[] = {
    -90,
    -94,
    -98,
    -100,
    -103,
    -105,
    -101,
    -96
};

static void app_i2c_init_pin(void)
{
    uapi_pin_set_mode(
        CONFIG_I2C_SCL_MASTER_PIN,
        CONFIG_I2C_MASTER_PIN_MODE
    );

    uapi_pin_set_mode(
        CONFIG_I2C_SDA_MASTER_PIN,
        CONFIG_I2C_MASTER_PIN_MODE
    );
}

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

static void oled_draw_demo(
    int32_t rssi,
    uint16_t distance_cm
)
{
    char line[24];

    ssd1306_Fill(Black);

    ssd1306_SetCursor(0, 0);
    ssd1306_DrawString(
        "RSSI SIM TEST",
        Font_7x10,
        White
    );

    snprintf(
        line,
        sizeof(line),
        "RSSI:%d dBm",
        (int)rssi
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
        "DIST:%u.%02u m",
        distance_cm / 100,
        distance_cm % 100
    );

    ssd1306_SetCursor(0, 32);
    ssd1306_DrawString(
        line,
        Font_7x10,
        White
    );

    ssd1306_SetCursor(0, 48);
    ssd1306_DrawString(
        "MODE:DEMO",
        Font_7x10,
        White
    );

    ssd1306_UpdateScreen();
}

static void OledTask(void)
{
    uint32_t ret;
    uint32_t index = 0;

    app_i2c_init_pin();

    ret = uapi_i2c_master_init(
        I2C_MASTER_BUS_ID,
        I2C_SET_BANDRATE,
        I2C_MASTER_ADDR
    );

    if (ret != 0) {
        printf(
            "[OLED] i2c init failed, ret=0x%x\r\n",
            ret
        );
        return;
    }

    ssd1306_Init();

    printf("[OLED] RSSI simulation started\r\n");

    while (1) {
        int32_t rssi =
            g_test_rssi_values[index];

        uint16_t distance_cm =
            rssi_to_distance_cm(rssi);

        oled_draw_demo(
            rssi,
            distance_cm
        );

        printf(
            "[SIM] RSSI=%d dBm, DIST=%u.%02u m\r\n",
            (int)rssi,
            distance_cm / 100,
            distance_cm % 100
        );

        index++;

        if (index >=
            sizeof(g_test_rssi_values) /
            sizeof(g_test_rssi_values[0])) {
            index = 0;
        }

        osal_msleep(1500);
    }
}

static void oled_entry(void)
{
    uint32_t ret;
    osal_task *taskid;

    osal_kthread_lock();

    taskid = osal_kthread_create(
        (osal_kthread_handler)OledTask,
        NULL,
        "OledTask",
        OLED_TASK_STACK_SIZE
    );

    if (taskid == NULL) {
        osal_kthread_unlock();
        printf("[OLED] create task failed\r\n");
        return;
    }

    ret = osal_kthread_set_priority(
        taskid,
        OLED_TASK_PRIO
    );

    osal_kthread_unlock();

    if (ret != OSAL_SUCCESS) {
        printf("[OLED] set priority failed\r\n");
    }
}

app_run(oled_entry);
