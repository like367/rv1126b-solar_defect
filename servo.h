/*
 * servo.h — 二轴舵机 PCA9685 硬件 PWM 控制模块
 * I2C 驱动 PCA9685, 16路 12-bit PWM, 零抖动零CPU开销
 * 适配 RV1126B /dev/i2c-4, 地址 0x40
 */
#ifndef SERVO_H
#define SERVO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

/* ========== PCA9685 配置 ========== */
#define PCA_I2C_DEV    "/dev/i2c-3"
#define PCA_ADDR       0x40
#define PCA_CHAN_X     0              /* X轴: PWM通道0 */
#define PCA_CHAN_Y     1              /* Y轴: PWM通道1 */

/* 舵机参数: 50Hz, 500~2500us脉宽 */
#define SERVO_PERIOD_US  20000
#define SERVO_MIN_US       500
#define SERVO_MAX_US      2500

/* 角度校准 */
#define SERVO_CALIB_X     1.0f   /* 取消校准, 直接用PWM角度 */
#define SERVO_CALIB_Y     1.0f

/* 巡检参数 */
#define SCAN_MAX_PANELS 8
#define SCAN_DWELL_MS   1500
#define SCAN_SETTLE_MS   400

/* PCA9685 寄存器 */
#define PCA_MODE1      0x00
#define PCA_MODE2      0x01
#define PCA_PRESCALE   0xFE
#define PCA_LED0_ON_L  0x06

/* ========== 舵机状态 ========== */
typedef struct {
    int    i2c_fd;
    float  angle[2];
    int    initialized;
    /* 巡检模式 */
    int    scan_mode;
    int    scan_active;
    int    scan_panel;
    int    scan_total;
    float  scan_angles[SCAN_MAX_PANELS][2];
    time_t scan_start;
} servo_t;

static servo_t g_servo;
int servo_settling = 0;

/* ========== I2C 底层 ========== */
static int _i2c_write8(int fd, unsigned char reg, unsigned char val) {
    unsigned char buf[2] = {reg, val};
    return write(fd, buf, 2) == 2 ? 0 : -1;
}

static void _pca_set_pwm(int fd, int chan, unsigned int off) {
    unsigned char buf[5];
    buf[0] = PCA_LED0_ON_L + 4 * chan;
    buf[1] = 0;       /* ON_L = 0 */
    buf[2] = 0;       /* ON_H = 0 */
    buf[3] = off & 0xFF;
    buf[4] = (off >> 8) & 0x0F;
    write(fd, buf, 5);
}

/* ========== 舵机初始化 ========== */
static int servo_init(servo_t *s) {
    memset(s, 0, sizeof(*s));
    s->i2c_fd = open(PCA_I2C_DEV, O_RDWR);
    if (s->i2c_fd < 0) {
        fprintf(stderr, "Servo: cannot open %s\n", PCA_I2C_DEV);
        return -1;
    }
    ioctl(s->i2c_fd, I2C_SLAVE, PCA_ADDR);

    /* 初始化 50Hz */
    _i2c_write8(s->i2c_fd, PCA_MODE1, 0x10);   /* sleep */
    _i2c_write8(s->i2c_fd, PCA_PRESCALE, 121);  /* 25MHz/(4096*50)-1 ≈ 121 */
    _i2c_write8(s->i2c_fd, PCA_MODE1, 0x20);    /* wake, auto-increment */
    _i2c_write8(s->i2c_fd, PCA_MODE2, 0x04);    /* totem pole */

    /* 初始化两轴为0° */
    _pca_set_pwm(s->i2c_fd, PCA_CHAN_X, 0);
    _pca_set_pwm(s->i2c_fd, PCA_CHAN_Y, 0);

    s->initialized = 1;
    printf("Servo: PCA9685 ready, X=ch%d Y=ch%d, 50Hz\n", PCA_CHAN_X, PCA_CHAN_Y);
    return 0;
}

/* ========== 角度控制 ========== */
static void servo_set_angle(servo_t *s, int axis, float angle) {
    if (!s->initialized) return;
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;
    /* 校准 */
    angle *= (axis == 0) ? SERVO_CALIB_X : SERVO_CALIB_Y;
    if (angle > 180) angle = 180;

    long pulse_us = SERVO_MIN_US + (long)(angle / 180.0f * (SERVO_MAX_US - SERVO_MIN_US));
    unsigned int off = (unsigned int)((pulse_us * 4096ULL) / SERVO_PERIOD_US);

    int chan = (axis == 0) ? PCA_CHAN_X : PCA_CHAN_Y;
    _pca_set_pwm(s->i2c_fd, chan, off);
    s->angle[axis] = angle;
}

static void servo_set_both(servo_t *s, float x, float y) {
    servo_set_angle(s, 0, x);
    servo_set_angle(s, 1, y);
}

static void servo_reset(servo_t *s) {
    printf("Servo: resetting\n");
    servo_set_both(s, 0, 0);
}

/* ========== 巡检模式 ========== */
static void servo_scan_config(servo_t *s, int count,
                               float *x_angles, float *y_angles) {
    if (count > SCAN_MAX_PANELS) count = SCAN_MAX_PANELS;
    s->scan_total = count;
    for (int i = 0; i < count; i++) {
        s->scan_angles[i][0] = x_angles ? x_angles[i] : 0;
        s->scan_angles[i][1] = y_angles ? y_angles[i] : 0;
    }
    printf("Servo: scan %d panels\n", count);
}

static void servo_scan_start(servo_t *s) {
    if (!s->initialized || s->scan_total == 0) return;
    s->scan_mode = 1; s->scan_active = 1; s->scan_panel = 0;
    s->scan_start = time(NULL);
    servo_reset(s);
    usleep(SCAN_DWELL_MS * 1000);
    printf("Servo: scan started, %d panels\n", s->scan_total);
}

static int servo_scan_next(servo_t *s) {
    if (!s->scan_active || s->scan_panel >= s->scan_total) {
        s->scan_active = 0;
        printf("Servo: scan done, %d panels in %lds\n",
               s->scan_total, (long)(time(NULL) - s->scan_start));
        servo_reset(s);
        return 0;
    }
    int idx = s->scan_panel;
    float x = s->scan_angles[idx][0], y = s->scan_angles[idx][1];
    printf("Servo: panel %d (%.0f, %.0f)\n", idx+1, x, y);
    servo_set_both(s, x, y);
    servo_settling = 1;
    usleep(SCAN_SETTLE_MS * 1000);
    servo_settling = 0;
    s->scan_panel++;
    return 1;
}

static void servo_scan_stop(servo_t *s) {
    s->scan_mode = 0; s->scan_active = 0;
    servo_reset(s);
    printf("Servo: scan stopped\n");
}

static void servo_cleanup(servo_t *s) {
    if (!s->initialized) return;
    servo_reset(s);
    usleep(100000);
    _pca_set_pwm(s->i2c_fd, PCA_CHAN_X, 0);
    _pca_set_pwm(s->i2c_fd, PCA_CHAN_Y, 0);
    close(s->i2c_fd);
    s->initialized = 0;
    printf("Servo: cleaned up\n");
}

#endif
