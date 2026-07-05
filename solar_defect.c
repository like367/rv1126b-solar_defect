#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <linux/videodev2.h>
#include <rga/im2d.h>
#include <rga/RgaApi.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "rknn_api.h"
#include "servo.h"


/* ================================================================
 * 嵌入式UI库 (原 ui.h — 已合并到单文件)
 * 提供: 位图字体渲染、状态栏、按钮、缺陷日志面板
 * 适配: 1024×600 DRM framebuffer (XRGB8888)
 * ================================================================ */

/* ---- 屏幕布局 ---- */
#define UI_TOP_H      36    /* 双行状态栏 */
#define UI_LOG_H      150
#define UI_CAM_Y      36
#define UI_CAM_H      414
#define UI_LOG_Y      450
#define FONT_W        8
#define FONT_H        16

/* ---- 调色板 ---- */
#define COLOR_BG        0x0011181E
#define COLOR_PANEL     0x001A2330
#define COLOR_BORDER    0x00334455
#define COLOR_TEXT      0x00CCD6DD
#define COLOR_TEXT_DIM  0x00667788
#define COLOR_ACCENT    0x003399FF
#define COLOR_GREEN     0x0022CC66
#define COLOR_RED       0x00EE4444
#define COLOR_YELLOW    0x00FFAA22
#define COLOR_ORANGE    0x00FF8844
#define COLOR_WHITE     0x00EEEEEE
#define COLOR_BTN_BG    0x002A4458
#define COLOR_BTN_ON    0x00226644
#define COLOR_BTN_OFF   0x00664444
#define COLOR_CAM_OFF   0x000A0A0A

/* ---- 日志系统 ---- */
#define LOG_MAX     100
#define LOG_LEN     128

typedef struct { char text[LOG_LEN]; time_t ts; } ui_log_t;

typedef struct {
    int camera_on, fps, frame_count, detection_count;
    char last_class[32]; float last_conf; char status_msg[64];
    ui_log_t logs[LOG_MAX]; int log_head, log_count;
    int btn_x, btn_y, btn_w, btn_h, btn_hover;
} ui_state_t;

ui_state_t g_ui;
volatile int g_hist_eq = 0;  /* CLAHE开关, H键切换, 0=关(高帧率) 1=开(暗光增强) */
#define CONF_THR_DEF 0.50f
static float g_conf_thr = CONF_THR_DEF;

/* ---- 8x16 位图字体 (ASCII 32-122) ---- */
static const unsigned char _font[][16] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0,0x18,0x18,0,0,0,0},
    {0,0x66,0x66,0x66,0x24,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0,0,0,0,0},
    {0x10,0x10,0x7C,0x92,0x90,0x90,0x7C,0x12,0x12,0x92,0x7C,0x10,0x10,0,0,0},
    {0,0,0x62,0x94,0x94,0x68,0x08,0x10,0x20,0x4C,0x52,0x52,0x8C,0,0,0},
    {0,0,0x38,0x44,0x44,0x38,0x30,0x4A,0x8A,0x8A,0x74,0,0,0,0,0},
    {0,0x18,0x18,0x18,0x10,0,0,0,0,0,0,0,0,0,0,0},
    {0,0x0C,0x18,0x30,0x30,0x60,0x60,0x60,0x30,0x30,0x18,0x0C,0,0,0,0},
    {0,0x30,0x18,0x0C,0x0C,0x06,0x06,0x06,0x0C,0x0C,0x18,0x30,0,0,0,0},
    {0,0,0,0x44,0x28,0xFE,0x28,0x44,0,0,0,0,0,0,0,0},
    {0,0,0,0,0x10,0x10,0x10,0xFE,0x10,0x10,0x10,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0x18,0x18,0x18,0x10,8,0,0},
    {0,0,0,0,0,0,0,0x7E,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0,0},
    {0,0x02,0x04,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0x40,0x40,0x80,0,0,0},
    {0,0,0x38,0x44,0x82,0x82,0x82,0x82,0x82,0x82,0x44,0x38,0,0,0,0},
    {0,0,0x08,0x18,0x28,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0,0,0,0},
    {0,0,0x3C,0x42,0x02,0x02,0x04,0x08,0x10,0x20,0x40,0x7E,0,0,0,0},
    {0,0,0x3C,0x42,0x02,0x02,0x1C,0x02,0x02,0x02,0x42,0x3C,0,0,0,0},
    {0,0,0x04,0x0C,0x14,0x24,0x44,0x84,0xFE,0x04,0x04,0x04,0,0,0,0},
    {0,0,0x7E,0x40,0x40,0x40,0x7C,0x02,0x02,0x02,0x42,0x3C,0,0,0,0},
    {0,0,0x1C,0x20,0x40,0x40,0x7C,0x42,0x42,0x42,0x42,0x3C,0,0,0,0},
    {0,0,0x7E,0x02,0x04,0x04,0x08,0x08,0x10,0x10,0x20,0x20,0,0,0,0},
    {0,0,0x3C,0x42,0x42,0x42,0x3C,0x42,0x42,0x42,0x42,0x3C,0,0,0,0},
    {0,0,0x3C,0x42,0x42,0x42,0x42,0x3E,0x02,0x02,0x04,0x38,0,0,0,0},
    {0,0,0,0,0,0x18,0x18,0,0,0,0x18,0x18,0,0,0,0},
    {0,0,0,0,0,0x18,0x18,0,0,0x18,0x18,0x18,0x10,8,0,0},
    {0,0,0,0x04,0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x04,0,0,0,0},
    {0,0,0,0,0,0,0x7E,0,0,0x7E,0,0,0,0,0,0},
    {0,0,0,0x40,0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x40,0,0,0,0},
    {0,0,0x3C,0x42,0x02,0x04,0x08,0x10,0x10,0,0x10,0x10,0,0,0,0},
    {0,0,0x7C,0x82,0x82,0x9A,0xAA,0xAA,0xAA,0x9C,0x80,0x7C,0,0,0,0},
    {0,0,0x10,0x28,0x28,0x44,0x44,0x44,0x7C,0x82,0x82,0x82,0,0,0,0},
    {0,0,0x78,0x44,0x44,0x44,0x78,0x44,0x44,0x44,0x44,0x78,0,0,0,0},
    {0,0,0x3C,0x42,0x40,0x40,0x40,0x40,0x40,0x40,0x42,0x3C,0,0,0,0},
    {0,0,0x78,0x44,0x42,0x42,0x42,0x42,0x42,0x42,0x44,0x78,0,0,0,0},
    {0,0,0x7E,0x40,0x40,0x40,0x78,0x40,0x40,0x40,0x40,0x7E,0,0,0,0},
    {0,0,0x7E,0x40,0x40,0x40,0x78,0x40,0x40,0x40,0x40,0x40,0,0,0,0},
    {0,0,0x3C,0x42,0x40,0x40,0x40,0x4E,0x42,0x42,0x46,0x3A,0,0,0,0},
    {0,0,0x42,0x42,0x42,0x42,0x7E,0x42,0x42,0x42,0x42,0x42,0,0,0,0},
    {0,0,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0,0,0,0},
    {0,0,0x1F,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x44,0x38,0,0,0,0},
    {0,0,0x42,0x44,0x48,0x50,0x60,0x60,0x50,0x48,0x44,0x42,0,0,0,0},
    {0,0,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x7E,0,0,0,0},
    {0,0,0x82,0xC6,0xAA,0xAA,0x92,0x82,0x82,0x82,0x82,0x82,0,0,0,0},
    {0,0,0x42,0x62,0x62,0x52,0x52,0x4A,0x4A,0x46,0x46,0x42,0,0,0,0},
    {0,0,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0,0,0,0},
    {0,0,0x78,0x44,0x44,0x44,0x78,0x40,0x40,0x40,0x40,0x40,0,0,0,0},
    {0,0,0x3C,0x42,0x42,0x42,0x42,0x42,0x42,0x4A,0x44,0x3A,0,0,0,0},
    {0,0,0x78,0x44,0x44,0x44,0x78,0x50,0x48,0x44,0x42,0x42,0,0,0,0},
    {0,0,0x3C,0x42,0x40,0x40,0x3C,0x02,0x02,0x02,0x42,0x3C,0,0,0,0},
    {0,0,0xFE,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0,0,0,0},
    {0,0,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x3C,0,0,0,0},
    {0,0,0x82,0x82,0x82,0x44,0x44,0x44,0x28,0x28,0x10,0x10,0,0,0,0},
    {0,0,0x82,0x82,0x82,0x82,0x82,0x92,0xAA,0xAA,0xC6,0x82,0,0,0,0},
    {0,0,0x82,0x82,0x44,0x28,0x10,0x10,0x28,0x44,0x82,0x82,0,0,0,0},
    {0,0,0x82,0x82,0x44,0x28,0x10,0x10,0x10,0x10,0x10,0x10,0,0,0,0},
    {0,0,0x7E,0x02,0x02,0x04,0x08,0x10,0x20,0x40,0x40,0x7E,0,0,0,0},
    {0,0x3E,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3E,0,0,0,0},
    {0,0x80,0x40,0x40,0x20,0x20,0x10,0x10,0x08,0x08,0x04,0x04,0x02,0,0,0},
    {0,0x7C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x7C,0,0,0,0},
    {0,0x10,0x28,0x44,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0xFE,0,0},
    {0x18,0x18,0x0C,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0x3C,0x42,0x02,0x3E,0x42,0x46,0x3A,0,0,0,0},
    {0,0x40,0x40,0x40,0x5C,0x62,0x42,0x42,0x42,0x42,0x62,0x5C,0,0,0,0},
    {0,0,0,0,0,0x3C,0x42,0x40,0x40,0x40,0x42,0x3C,0,0,0,0},
    {0,0x02,0x02,0x02,0x3A,0x46,0x42,0x42,0x42,0x42,0x46,0x3A,0,0,0,0},
    {0,0,0,0,0,0x3C,0x42,0x42,0x7E,0x40,0x42,0x3C,0,0,0,0},
    {0,0x0C,0x12,0x10,0x10,0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0,0,0,0},
    {0,0,0,0,0,0x3A,0x46,0x42,0x42,0x46,0x3A,0x02,0x42,0x3C,0,0},
    {0,0x40,0x40,0x40,0x5C,0x62,0x42,0x42,0x42,0x42,0x42,0x42,0,0,0,0},
    {0,0,0x08,0,0,0x18,0x08,0x08,0x08,0x08,0x08,0x3E,0,0,0,0},
    {0,0,0x04,0,0,0x0C,0x04,0x04,0x04,0x04,0x04,0x44,0x44,0x38,0,0},
    {0,0x40,0x40,0x40,0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x42,0,0,0,0},
    {0,0x18,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x3E,0,0,0,0},
    {0,0,0,0,0,0xEC,0x92,0x92,0x92,0x92,0x92,0x92,0,0,0,0},
    {0,0,0,0,0,0x5C,0x62,0x42,0x42,0x42,0x42,0x42,0,0,0,0},
    {0,0,0,0,0,0x3C,0x42,0x42,0x42,0x42,0x42,0x3C,0,0,0,0},
    {0,0,0,0,0,0x5C,0x62,0x42,0x42,0x42,0x62,0x5C,0x40,0x40,0,0},
    {0,0,0,0,0,0x3A,0x46,0x42,0x42,0x42,0x46,0x3A,0x02,0x02,0,0},
    {0,0,0,0,0,0x5C,0x62,0x40,0x40,0x40,0x40,0x40,0,0,0,0},
    {0,0,0,0,0,0x3C,0x42,0x40,0x3C,0x02,0x42,0x3C,0,0,0,0},
    {0,0,0x10,0x10,0x10,0x7C,0x10,0x10,0x10,0x10,0x12,0x0C,0,0,0,0},
    {0,0,0,0,0,0x42,0x42,0x42,0x42,0x42,0x46,0x3A,0,0,0,0},
    {0,0,0,0,0,0x42,0x42,0x42,0x24,0x24,0x18,0x18,0,0,0,0},
    {0,0,0,0,0,0x82,0x82,0x82,0x92,0xAA,0x44,0x44,0,0,0,0},
    {0,0,0,0,0,0x42,0x42,0x24,0x18,0x24,0x42,0x42,0,0,0,0},
    {0,0,0,0,0,0x42,0x42,0x42,0x42,0x46,0x3A,0x02,0x42,0x3C,0,0},
    {0,0,0,0,0,0x7E,0x02,0x04,0x08,0x10,0x20,0x7E,0,0,0,0},
};

/* ---- 底层像素 ---- */
static inline void _fb_pixel(uint32_t *fb,int fb_w,int x,int y,uint32_t c){
    if(x<0||x>=fb_w||y<0||y>=600) return;
    fb[y*fb_w+x]=c;
}
static inline void _fb_fill(uint32_t *fb,int fb_w,int x,int y,int w,int h,uint32_t c){
    for(int j=0;j<h;j++){uint32_t *row=&fb[(y+j)*fb_w+x];for(int i=0;i<w;i++)row[i]=c;}
}
static inline void _fb_hline(uint32_t *fb,int fb_w,int x,int y,int w,uint32_t c){
    if(y<0||y>=600) return; uint32_t *row=&fb[y*fb_w];
    for(int i=0;i<w;i++){int px=x+i;if(px>=0&&px<fb_w)row[px]=c;}
}

/* ---- 文本渲染 ---- */
static void ui_draw_char(uint32_t *fb,int fb_w,int ch,int x,int y,uint32_t color){
    if(ch<32||ch>122)ch='?';
    const unsigned char *g=_font[ch-32];
    for(int r=0;r<FONT_H;r++){unsigned char b=g[r];for(int c=0;c<FONT_W;c++){if(b&(0x80>>c))_fb_pixel(fb,fb_w,x+c,y+r,color);}}
}
static void ui_draw_text(uint32_t *fb,int fb_w,const char *s,int x,int y,uint32_t color){
    while(*s){if(*s=='\n'){x=0;y+=FONT_H+2;s++;continue;}ui_draw_char(fb,fb_w,*s,x,y,color);x+=FONT_W;s++;}
}
static void ui_draw_text_center(uint32_t *fb,int fb_w,const char *s,int cx,int y,uint32_t color){
    int w=strlen(s)*FONT_W;ui_draw_text(fb,fb_w,s,cx-w/2,y,color);
}

/* ---- 按钮 ---- */
static void ui_draw_button(uint32_t *fb,int fb_w,int x,int y,int w,int h,
                           const char *label,int active,uint32_t color){
    _fb_fill(fb,fb_w,x+1,y+1,w-2,h-2,active?color:COLOR_BTN_BG);
    _fb_hline(fb,fb_w,x,y,w,COLOR_BORDER);_fb_hline(fb,fb_w,x,y+h-1,w,COLOR_BORDER);
    for(int i=0;i<h;i++){_fb_pixel(fb,fb_w,x,y+i,COLOR_BORDER);_fb_pixel(fb,fb_w,x+w-1,y+i,COLOR_BORDER);}
    ui_draw_text_center(fb,fb_w,label,x+w/2,y+(h-FONT_H)/2,active?COLOR_WHITE:COLOR_TEXT);
}

/* ---- 日志API ---- */
static void ui_log_add(const char *fmt,...){
    va_list ap;va_start(ap,fmt);
    int idx=g_ui.log_head;
    vsnprintf(g_ui.logs[idx].text,LOG_LEN,fmt,ap);
    g_ui.logs[idx].ts=time(NULL);
    g_ui.log_head=(idx+1)%LOG_MAX;
    if(g_ui.log_count<LOG_MAX)g_ui.log_count++;
    va_end(ap);
}

/* ---- 主渲染函数 ---- */
static void ui_init(void){
    memset(&g_ui,0,sizeof(g_ui));g_ui.camera_on=1;
    g_ui.btn_x=920;g_ui.btn_y=6;g_ui.btn_w=96;g_ui.btn_h=24;
}

static void ui_render_top_bar(uint32_t *fb,int fb_w){
    _fb_fill(fb,fb_w,0,0,fb_w,UI_TOP_H,COLOR_PANEL);
    _fb_hline(fb,fb_w,0,UI_TOP_H-1,fb_w,COLOR_BORDER);
    char buf[64];

    /* ---- 第1行: 标题 | 状态 | 按钮 ---- */
    int r1=2;
    ui_draw_text(fb,fb_w,"SOLAR",8,r1,COLOR_ACCENT);
    ui_draw_text(fb,fb_w,"Defect",50,r1,COLOR_TEXT);

    /* 中间: FPS THR DET */
    int cx=fb_w/2-60;
    snprintf(buf,sizeof(buf),"FPS:%-2d",g_ui.fps);
    ui_draw_text(fb,fb_w,buf,cx,r1,COLOR_GREEN);
    snprintf(buf,sizeof(buf),"THR:%.2f",g_conf_thr);
    ui_draw_text(fb,fb_w,buf,cx+65,r1,COLOR_YELLOW);
    if(g_ui.camera_on){
        snprintf(buf,sizeof(buf),"DET:%-2d",g_ui.detection_count);
        ui_draw_text(fb,fb_w,buf,cx+150,r1,g_ui.detection_count>0?COLOR_RED:COLOR_TEXT_DIM);
    } else {
        ui_draw_text(fb,fb_w,"OFF",cx+150,r1,COLOR_RED);
    }

    /* 状态消息 */
    if(g_ui.status_msg[0])
        ui_draw_text(fb,fb_w,g_ui.status_msg,cx+220,r1,COLOR_ACCENT);

    /* 右侧按钮 */
    snprintf(buf,sizeof(buf),"%s",g_ui.camera_on?"[ ON ]":"[OFF]");
    ui_draw_button(fb,fb_w,g_ui.btn_x,g_ui.btn_y,g_ui.btn_w,g_ui.btn_h,
                   buf,g_ui.camera_on,g_ui.camera_on?COLOR_BTN_ON:COLOR_BTN_OFF);

    /* ---- 第2行: 按键提示 ---- */
    int r2=20;
    ui_draw_text(fb,fb_w,"S: Camera",8,r2,COLOR_TEXT_DIM);
    ui_draw_text(fb,fb_w,"+/-: Threshold",110,r2,COLOR_TEXT_DIM);
    ui_draw_text(fb,fb_w,"R: Reset",240,r2,COLOR_TEXT_DIM);
    ui_draw_text(fb,fb_w,"H: CLAHE",330,r2,g_hist_eq?COLOR_GREEN:COLOR_TEXT_DIM);
    ui_draw_text(fb,fb_w,"Q: Quit",400,r2,COLOR_TEXT_DIM);
    /* HEQ状态 */
    snprintf(buf,sizeof(buf),"CLAHE:%s",g_hist_eq?"ON":"OFF");
    ui_draw_text(fb,fb_w,buf,fb_w-80,r2,g_hist_eq?COLOR_GREEN:COLOR_TEXT_DIM);
}

static void ui_render_camera_off(uint32_t *fb,int fb_w){
    _fb_fill(fb,fb_w,0,UI_CAM_Y,fb_w,UI_CAM_H,COLOR_CAM_OFF);
    int cy=UI_CAM_Y+UI_CAM_H/2;
    /* 图标: 用简单字符画一个相机图标效果 */
    _fb_hline(fb,fb_w,fb_w/2-25,cy-30,50,COLOR_BORDER);
    _fb_hline(fb,fb_w,fb_w/2-25,cy+10,50,COLOR_BORDER);
    for(int i=-30;i<=10;i++){_fb_pixel(fb,fb_w,fb_w/2-26,cy+i,COLOR_BORDER);_fb_pixel(fb,fb_w,fb_w/2+25,cy+i,COLOR_BORDER);}
    ui_draw_text_center(fb,fb_w,"Camera Off",fb_w/2,cy-45,COLOR_ACCENT);
    ui_draw_text_center(fb,fb_w,"Press 'S' to start detection",fb_w/2,cy+25,COLOR_TEXT_DIM);
    ui_draw_text_center(fb,fb_w,"RV1126B | YOLOv8n INT8 | COM7",fb_w/2,cy+50,COLOR_TEXT_DIM);
}

static void ui_render_log_panel(uint32_t *fb,int fb_w){
    int ly=UI_LOG_Y,lh=UI_LOG_H;
    _fb_fill(fb,fb_w,0,ly,fb_w,lh,COLOR_PANEL);
    _fb_hline(fb,fb_w,0,ly,fb_w,COLOR_BORDER);
    _fb_hline(fb,fb_w,0,ly+lh-1,fb_w,COLOR_BORDER);

    /* 标题栏 */
    char buf[128];
    snprintf(buf,sizeof(buf),"DEFECT LOG  [%d]",g_ui.log_count);
    ui_draw_text(fb,fb_w,buf,8,ly+4,COLOR_ACCENT);
    /* 当前时间 */
    time_t now=time(NULL);struct tm *tm=localtime(&now);
    snprintf(buf,sizeof(buf),"%02d:%02d:%02d",tm->tm_hour,tm->tm_min,tm->tm_sec);
    int tw=strlen(buf)*FONT_W;
    ui_draw_text(fb,fb_w,buf,fb_w-tw-8,ly+4,COLOR_WHITE);
    /* 时间下方分隔线 */
    _fb_hline(fb,fb_w,0,ly+22,fb_w,0x00223344);

    /* 日志条目 */
    int vis=(lh-30)/16, row_y=ly+26, total=g_ui.log_count;
    for(int i=0;i<vis&&i<total;i++){
        int idx=(g_ui.log_head-i-1+LOG_MAX)%LOG_MAX;
        ui_log_t *l=&g_ui.logs[idx];
        int ry=row_y+i*16;

        /* 时间戳 */
        struct tm *lt=localtime(&l->ts);
        char ts[10];
        snprintf(ts,sizeof(ts),"%02d:%02d:%02d",lt->tm_hour,lt->tm_min,lt->tm_sec);
        ui_draw_text(fb,fb_w,ts,8,ry,COLOR_TEXT_DIM);

        /* 类别着色 */
        uint32_t tc=COLOR_TEXT;
        if(strstr(l->text,"bird-drop"))       tc=COLOR_RED;
        else if(strstr(l->text,"dusty"))      tc=COLOR_YELLOW;
        else if(strstr(l->text,"clean"))      tc=COLOR_GREEN;
        else if(strstr(l->text,"electrical")) tc=0x00FF44FF;
        else if(strstr(l->text,"physical"))   tc=0x0044FFFF;
        else if(strstr(l->text,"snow"))       tc=COLOR_ORANGE;
        else if(strstr(l->text,"WARN"))       tc=COLOR_YELLOW;
        else if(strstr(l->text,"ERR"))        tc=COLOR_RED;
        else if(strstr(l->text,"Camera"))     tc=COLOR_ACCENT;
        else if(strstr(l->text,"System"))     tc=COLOR_ACCENT;
        else if(strstr(l->text,"clear"))      tc=COLOR_TEXT_DIM;

        /* 日志文本 */
        char d[96];snprintf(d,sizeof(d),"%.88s",l->text);
        ui_draw_text(fb,fb_w,d,76,ry,tc);
    }
}

static void ui_render_overlay(uint32_t *fb,int fb_w){
    ui_render_top_bar(fb,fb_w);
    if(!g_ui.camera_on)ui_render_camera_off(fb,fb_w);
    ui_render_log_panel(fb,fb_w);
}

/* ---- 配置 ---- */
#define DEV      "/dev/video52"
#define SRC_W    640
#define SRC_H    480
#define DST_W    640
#define DST_H    640
#define BUF_CNT  4
#define NUM_CLS  6
#define IOU_THR  0.45f
#define MAX_DETS 64
#define SCR_W    1024
#define SCR_H    600
#define DRM_DEV  "/dev/dri/card0"
#define QSIZE    4
#define BRIGHTNESS_MIN 20.0f  /* skip inference below this mean, blocks false clean on black screen */
#define HEQ_BRIGHTNESS_MAX 100.0f  /* CLAHE仅在画面偏暗(mean<100)时生效, 防高光过曝 */

/* ---- 按类别置信度阈值 (0=鸟粪 1=正常 2=灰尘 3=电气损伤 4=物理损伤 5=积雪) ---- */
/* 0=鸟粪 1=正常 2=灰尘 3=电气 4=物理 5=积雪 */
static float g_cls_thr[NUM_CLS] = {0.55f, 0.50f, 0.60f, 0.50f, 0.50f, 0.50f};

/* ---- 帧间一致性过滤 (消除瞬态误检) ---- */
#define TEMPORAL_WINDOW 3   /* 连续N帧检出才确认 */
#define TEMPORAL_IOU    0.20f

/* ---- RKNN函数指针 ---- */
typedef int (*fn_init)(rknn_context*,void*,unsigned int,unsigned int,void*);
typedef int (*fn_inputs_set)(rknn_context,uint32_t,rknn_input*);
typedef int (*fn_run)(rknn_context,void*);
typedef int (*fn_outputs_get)(rknn_context,uint32_t,rknn_output*,void*);
typedef int (*fn_outputs_release)(rknn_context,uint32_t,rknn_output*);
typedef int (*fn_destroy)(rknn_context);

static fn_init            f_init;
static fn_inputs_set      f_inputs_set;
static fn_run             f_run;
static fn_outputs_get     f_outputs_get;
static fn_outputs_release f_outputs_release;
static fn_destroy         f_destroy;
static rknn_context       g_ctx;

/* ---- 检测结果 ---- */
typedef struct {
    float x1,y1,x2,y2,score;
    int cls;
} Det;

typedef struct {
    Det  dets[MAX_DETS];
    int  count;
} DetResult;

/* 帧间一致性过滤状态 */
static Det  g_prev_dets[MAX_DETS];
static int  g_prev_cnt = 0;
static int  g_prev_age[MAX_DETS];

/* ---- 帧队列 ---- */
typedef struct {
    unsigned char *rgb;
    DetResult      result;
    int            valid;
} Frame;

typedef struct {
    Frame           frames[QSIZE];
    int             head, tail, count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} Queue;

static Queue g_cap_queue;   /* 采集→推理 */
static Queue g_disp_queue;  /* 推理→显示 */
static volatile int g_running = 1;

#include "web_ui.h"

/* ---- DRM ---- */
typedef struct {
    int      fd;
    uint32_t conn_id, crtc_id, fb_id, handle;
    uint32_t *map, size;
    drmModeCrtc *saved_crtc;
} DRMDev;
static DRMDev g_drm;

/* ---- V4L2 ---- */
struct buffer { void *start; size_t length; };
static int           g_v4l2_fd;
static struct buffer g_bufs[BUF_CNT];

/* ---- 工具 ---- */
static int xioctl(int fd, unsigned long req, void *arg) {
    int r;
    do { r=ioctl(fd,req,arg); } while(r==-1&&errno==EINTR);
    return r;
}
static float sigmoid(float x){return 1.0f/(1.0f+expf(-x));}
static float iou_calc(Det *a,Det *b){
    float ix1=fmaxf(a->x1,b->x1),iy1=fmaxf(a->y1,b->y1);
    float ix2=fminf(a->x2,b->x2),iy2=fminf(a->y2,b->y2);
    float inter=fmaxf(0,ix2-ix1)*fmaxf(0,iy2-iy1);
    if(inter==0) return 0;
    return inter/((a->x2-a->x1)*(a->y2-a->y1)+(b->x2-b->x1)*(b->y2-b->y1)-inter);
}

/* ---- 队列操作 ---- */
static void queue_init(Queue *q){
    memset(q,0,sizeof(*q));
    pthread_mutex_init(&q->mutex,NULL);
    pthread_cond_init(&q->not_empty,NULL);
    pthread_cond_init(&q->not_full,NULL);
    for(int i=0;i<QSIZE;i++)
        q->frames[i].rgb=malloc(DST_W*DST_H*3);
}

static void queue_push(Queue *q, Frame *f){
    pthread_mutex_lock(&q->mutex);
    while(q->count==QSIZE&&g_running){
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME,&ts);
        ts.tv_sec+=1;
        pthread_cond_timedwait(&q->not_full,&q->mutex,&ts);
    }
    if(g_running){
        memcpy(q->frames[q->tail].rgb, f->rgb, DST_W*DST_H*3);
        q->frames[q->tail].result = f->result;
        q->frames[q->tail].valid  = f->valid;
        q->tail=(q->tail+1)%QSIZE;
        q->count++;
        pthread_cond_signal(&q->not_empty);
    }
    pthread_mutex_unlock(&q->mutex);
}

static void queue_pop(Queue *q, Frame *f){
    pthread_mutex_lock(&q->mutex);
    while(q->count==0&&g_running){
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME,&ts);
        ts.tv_sec+=1;
        pthread_cond_timedwait(&q->not_empty,&q->mutex,&ts);
    }
    if(q->count>0){
        *f=q->frames[q->head];
        q->head=(q->head+1)%QSIZE;
        q->count--;
        pthread_cond_signal(&q->not_full);
    }
    pthread_mutex_unlock(&q->mutex);
}

/* ---- DRM操作 ---- */
static int drm_init(DRMDev *drm){
    drm->fd=open(DRM_DEV,O_RDWR);
    if(drm->fd<0){perror("open drm");return -1;}
    drm->conn_id=96; drm->crtc_id=73;
    struct drm_mode_create_dumb creq={0};
    creq.width=SCR_W; creq.height=SCR_H; creq.bpp=32;
    drmIoctl(drm->fd,DRM_IOCTL_MODE_CREATE_DUMB,&creq);
    drm->size=creq.size; drm->handle=creq.handle;
    drmModeAddFB(drm->fd,SCR_W,SCR_H,24,32,creq.pitch,creq.handle,&drm->fb_id);
    struct drm_mode_map_dumb mreq={0};
    mreq.handle=creq.handle;
    drmIoctl(drm->fd,DRM_IOCTL_MODE_MAP_DUMB,&mreq);
    drm->map=mmap(NULL,creq.size,PROT_READ|PROT_WRITE,MAP_SHARED,drm->fd,mreq.offset);
    memset(drm->map,0,creq.size);
    drm->saved_crtc=drmModeGetCrtc(drm->fd,drm->crtc_id);
    drmModeConnector *conn=drmModeGetConnector(drm->fd,drm->conn_id);
    drmModeSetCrtc(drm->fd,drm->crtc_id,drm->fb_id,0,0,&drm->conn_id,1,&conn->modes[0]);
    drmModeFreeConnector(conn);
    return 0;
}

static inline void set_pixel(DRMDev *drm,int x,int y,uint32_t c){
    if(x<0||x>=SCR_W||y<0||y>=SCR_H) return;
    drm->map[y*SCR_W+x]=c;
}

static void draw_rect(DRMDev *drm,int x1,int y1,int x2,int y2,uint32_t color,int thick){
    for(int t=0;t<thick;t++){
        for(int x=x1;x<=x2;x++){set_pixel(drm,x,y1+t,color);set_pixel(drm,x,y2-t,color);}
        for(int y=y1;y<=y2;y++){set_pixel(drm,x1+t,y,color);set_pixel(drm,x2-t,y,color);}
    }
}

static void draw_image(DRMDev *drm,unsigned char *rgb,int iw,int ih){
    /* 固定点步长：避免每像素乘法/除法 */
    int step_x=(iw<<16)/SCR_W;  /* 16.16定点 */
    int step_y=(ih<<16)/SCR_H;
    int sy_fixed=0;
    for(int y=0;y<SCR_H;y++){
        int sy=sy_fixed>>16;
        unsigned char *row=rgb+sy*iw*3;
        uint32_t *fb=&drm->map[y*SCR_W];
        int sx_fixed=0;
        for(int x=0;x<SCR_W;x++){
            int idx=(sx_fixed>>16)*3;
            fb[x]=((uint32_t)row[idx+2]<<16)|
                   ((uint32_t)row[idx+1]<<8)|
                   ((uint32_t)row[idx]);
            sx_fixed+=step_x;
        }
        sy_fixed+=step_y;
    }
}

/* 缩放画面到指定矩形区域(保持宽高比,居中) */
static void draw_image_rect(DRMDev *drm,unsigned char *rgb,int iw,int ih,
                             int dx,int dy,int dw,int dh){
    /* 计算居中方形区域 */
    int size=dw<dh?dw:dh;
    int ox=dx+(dw-size)/2;
    int oy=dy+(dh-size)/2;
    int step=(iw<<16)/size;
    int sy_fixed=0;
    for(int y=0;y<size;y++){
        int sy=sy_fixed>>16;
        unsigned char *row=rgb+sy*iw*3;
        int py=oy+y;
        if(py<0||py>=SCR_H){sy_fixed+=step;continue;}
        uint32_t *fb=&drm->map[py*SCR_W];
        int sx_fixed=0;
        for(int x=0;x<size;x++){
            int idx=(sx_fixed>>16)*3;
            int px=ox+x;
            if(px>=0&&px<SCR_W)
                fb[px]=((uint32_t)row[idx+2]<<16)|
                        ((uint32_t)row[idx+1]<<8)|
                        ((uint32_t)row[idx]);
            sx_fixed+=step;
        }
        sy_fixed+=step;
    }
}

static uint32_t cls_color(int cls){
    uint32_t c[]={0x00FF4444,0x0044FF44,0x00FFFF44,0x00FF44FF,0x0044FFFF,0x00FF8844};
    return c[cls%6];
}

/* ---- 信号处理 ---- */
static void sig_handler(int s){
    (void)s; g_running=0;
    pthread_cond_broadcast(&g_cap_queue.not_empty);
    pthread_cond_broadcast(&g_cap_queue.not_full);
    pthread_cond_broadcast(&g_disp_queue.not_empty);
    pthread_cond_broadcast(&g_disp_queue.not_full);
}

/* ---- 采集线程 ---- */
static void *capture_thread(void *arg){
    (void)arg;
    unsigned char *tmp=malloc(DST_W*DST_H*3);
    int err_cnt = 0;  /* 连续DQBUF失败计数 */
    while(g_running){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP;
        fd_set fds; struct timeval tv={2,0};
        FD_ZERO(&fds); FD_SET(g_v4l2_fd,&fds);
        int sr_sel = select(g_v4l2_fd+1,&fds,NULL,NULL,&tv);
        if(sr_sel <= 0) {
            err_cnt++;
            if(err_cnt > 30) {
                /* 连续超时>60s, 尝试恢复 */
                fprintf(stderr,"[CAP] stream timeout, resetting...\n");
                xioctl(g_v4l2_fd,VIDIOC_STREAMOFF,&buf.type);
                usleep(100000);
                xioctl(g_v4l2_fd,VIDIOC_STREAMON,&buf.type);
                err_cnt = 0;
            }
            continue;
        }
        if(xioctl(g_v4l2_fd,VIDIOC_DQBUF,&buf)<0) {
            err_cnt++;
            /* DQBUF失败会丢失buffer, 连续失败说明USB有问题 */
            if(err_cnt > 5) {
                fprintf(stderr,"[CAP] DQBUF retry #%d\n", err_cnt);
                usleep(50000);
            }
            continue;
        }
        err_cnt = 0;  /* DQBUF成功, 重置 */

        rga_buffer_t src=wrapbuffer_virtualaddr(
            g_bufs[buf.index].start,SRC_W,SRC_H,RK_FORMAT_YVYU_422);
        rga_buffer_t dst=wrapbuffer_virtualaddr(tmp,DST_W,DST_H,RK_FORMAT_RGB_888);
        im_rect sr_={0,0,SRC_W,SRC_H},dr={0,0,DST_W,DST_H};
        rga_buffer_t pat={0}; im_rect pr={0};
        improcess(src,dst,pat,sr_,dr,pr,IM_SYNC);
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);

        Frame f={0};
        f.rgb=tmp; f.valid=1;
        queue_push(&g_cap_queue,&f);
    }
    free(tmp);
    return NULL;
}

/* ====== Y通道CLAHE v4.2 (降噪后置 + 暗亮自适应UV) ======
 * AWB → CLAHE(clip=2.5) → 亮度降噪(thr=10) → 0.8%拉伸
 * → UV自适应(暗区Y<80 + 亮区Y>180各2次) → USM(0.3)
 * 亮区2次滤波压制天花板等平坦区残留彩噪, USM 0.3补强细裂纹边缘
 */
#define CLAHE_TILES  8
#define CLAHE_CLIP   2.5f

/* ---- 辅助: RGB → Y (亮度分量) ---- */
static void _y_from_rgb(const unsigned char *rgb, unsigned char *y, int w, int h) {
    int total = w * h;
    for (int i = 0; i < total; i++) {
        const unsigned char *p = rgb + i * 3;
        y[i] = (unsigned char)((p[0] * 114 + p[1] * 587 + p[2] * 299) / 1000);
    }
}

/* ---- 自动白平衡 (灰世界法, 增益限幅防过曝) ---- */
static void _auto_wb(unsigned char *rgb, int w, int h, double max_gain) {
    int total = w * h;
    double sum_r = 0, sum_g = 0, sum_b = 0;
    for (int i = 0; i < total; i++) {
        unsigned char *p = rgb + i * 3;
        sum_r += p[0]; sum_g += p[1]; sum_b += p[2];
    }
    double avg_g = sum_g / total;
    if (avg_g < 1.0) return;
    double avg_r = sum_r / total, avg_b = sum_b / total;
    double gain_r = avg_g / avg_r;
    double gain_b = avg_g / avg_b;
    if (gain_r > max_gain) gain_r = max_gain;
    if (gain_r < 1.0 / max_gain) gain_r = 1.0 / max_gain;
    if (gain_b > max_gain) gain_b = max_gain;
    if (gain_b < 1.0 / max_gain) gain_b = 1.0 / max_gain;
    for (int i = 0; i < total; i++) {
        unsigned char *p = rgb + i * 3;
        int r = (int)(p[0] * gain_r); p[0] = r > 255 ? 255 : (unsigned char)r;
        int b = (int)(p[2] * gain_b); p[2] = b > 255 ? 255 : (unsigned char)b;
    }
}

/* ---- 边缘保持平滑 (3×3 Y门控平均, 压制亮度噪点) ---- */
static void _y_smooth_edge(unsigned char *y, int w, int h, int thr) {
    unsigned char *tmp = (unsigned char*)malloc(w * h);
    if (!tmp) return;
    memcpy(tmp, y, w * h);
    for (int yy = 1; yy < h - 1; yy++) {
        for (int xx = 1; xx < w - 1; xx++) {
            int cen = tmp[yy * w + xx];
            int sum = cen, cnt = 1;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    if (dy == 0 && dx == 0) continue;
                    int v = tmp[(yy + dy) * w + (xx + dx)];
                    if (abs(v - cen) <= thr) { sum += v; cnt++; }
                }
            }
            y[yy * w + xx] = (unsigned char)(sum / cnt);
        }
    }
    free(tmp);
}

/* ---- CLAHE核心 (Y缓冲就地进行) ---- */
static void _clahe_y(unsigned char *y, int w, int h) {
    int tw = w / CLAHE_TILES, th = h / CLAHE_TILES;
    unsigned char cdf[CLAHE_TILES][CLAHE_TILES][256];
    int clip_val = (int)((float)(tw * th) / 256.0f * CLAHE_CLIP);

    for (int ty = 0; ty < CLAHE_TILES; ty++) {
        for (int tx = 0; tx < CLAHE_TILES; tx++) {
            int y0 = ty * th, x0 = tx * tw;
            int hist[256] = {0};
            for (int yy = y0; yy < y0 + th; yy++)
                for (int xx = x0; xx < x0 + tw; xx++)
                    hist[y[yy * w + xx]]++;

            int excess = 0;
            for (int i = 0; i < 256; i++)
                if (hist[i] > clip_val) {
                    excess += hist[i] - clip_val;
                    hist[i] = clip_val;
                }
            int add = excess / 256;
            for (int i = 0; i < 256; i++) hist[i] += add;

            int sum = 0, pix = tw * th;
            for (int i = 0; i < 256; i++) {
                sum += hist[i];
                cdf[ty][tx][i] = (unsigned char)(sum * 255 / pix);
            }
        }
    }

    for (int yy = 0; yy < h; yy++) {
        float fy = ((float)yy / th) - 0.5f;
        int ty0 = (int)fy;
        if (ty0 < 0) ty0 = 0;
        int ty1 = ty0 + 1;
        if (ty1 >= CLAHE_TILES) { ty1 = CLAHE_TILES - 1; ty0 = ty1 - 1; }
        float yr = fy - ty0;
        for (int xx = 0; xx < w; xx++) {
            float fx = ((float)xx / tw) - 0.5f;
            int tx0 = (int)fx;
            if (tx0 < 0) tx0 = 0;
            int tx1 = tx0 + 1;
            if (tx1 >= CLAHE_TILES) { tx1 = CLAHE_TILES - 1; tx0 = tx1 - 1; }
            float xr = fx - tx0;
            int val = y[yy * w + xx];
            float mapped = (1 - yr) * ((1 - xr) * cdf[ty0][tx0][val] + xr * cdf[ty0][tx1][val])
                         + yr     * ((1 - xr) * cdf[ty1][tx0][val] + xr * cdf[ty1][tx1][val]);
            y[yy * w + xx] = (unsigned char)(mapped > 255 ? 255 : (int)mapped);
        }
    }
}

/* ---- 对比度拉伸 (裁剪两端后线性映射到0-255) ---- */
static void _y_stretch(unsigned char *y, int w, int h, double clip_pct) {
    int total = w * h;
    int hist[256] = {0};
    for (int i = 0; i < total; i++) hist[y[i]]++;

    int cutoff = (int)(total * clip_pct);
    int lo = 0, hi = 255, sum = 0;
    for (; lo < 256; lo++) { sum += hist[lo]; if (sum > cutoff) break; }
    sum = 0;
    for (; hi >= 0; hi--) { sum += hist[hi]; if (sum > cutoff) break; }
    if (hi <= lo) return;

    double scale = 255.0 / (hi - lo);
    for (int i = 0; i < total; i++) {
        int v = y[i];
        if (v <= lo) y[i] = 0;
        else if (v >= hi) y[i] = 255;
        else y[i] = (unsigned char)((v - lo) * scale);
    }
}

/* ---- UV降噪 (自适应: 暗区2次box blur, 亮区1次, 压制彩噪) ---- */
static void _blur_uv(unsigned char *rgb, const unsigned char *y, int w, int h) {
    int total = w * h;
    unsigned char *u_buf = (unsigned char*)malloc(total);
    unsigned char *v_buf = (unsigned char*)malloc(total);
    if (!u_buf || !v_buf) { free(u_buf); free(v_buf); return; }

    for (int i = 0; i < total; i++) {
        unsigned char *p = rgb + i * 3;
        int r = p[0], g = p[1], b = p[2];
        u_buf[i] = (unsigned char)((-38*r - 74*g + 112*b + 128) / 256 + 128);
        v_buf[i] = (unsigned char)((112*r - 94*g - 18*b + 128) / 256 + 128);
    }

    for (int pass = 0; pass < 2; pass++) {
        for (int ch = 0; ch < 2; ch++) {
            unsigned char *buf = ch ? v_buf : u_buf;
            unsigned char *tmp = (unsigned char*)malloc(total);
            if (!tmp) continue;
            memcpy(tmp, buf, total);
            int is_dark_pass = (pass == 1);
            for (int yy = 1; yy < h - 1; yy++)
                for (int xx = 1; xx < w - 1; xx++) {
                    if (is_dark_pass && y[yy * w + xx] >= 80 && y[yy * w + xx] <= 180) continue;
                    int s = 0;
                    for (int dy = -1; dy <= 1; dy++)
                        for (int dx = -1; dx <= 1; dx++)
                            s += tmp[(yy + dy) * w + (xx + dx)];
                    buf[yy * w + xx] = (unsigned char)(s / 9);
                }
            free(tmp);
        }
    }

    /* Y + 模糊UV → RGB (保持亮度不变) */
    for (int i = 0; i < total; i++) {
        unsigned char *p = rgb + i * 3;
        int Yf = (66*p[0] + 129*p[1] + 25*p[2]) / 256 + 16;
        int Uu = u_buf[i] - 128, Vv = v_buf[i] - 128;
        int rr = (298*(Yf-16) + 409*Vv + 128) / 256;
        int gg = (298*(Yf-16) - 100*Uu - 208*Vv + 128) / 256;
        int bb = (298*(Yf-16) + 516*Uu + 128) / 256;
        p[0] = rr < 0 ? 0 : (rr > 255 ? 255 : (unsigned char)rr);
        p[1] = gg < 0 ? 0 : (gg > 255 ? 255 : (unsigned char)gg);
        p[2] = bb < 0 ? 0 : (bb > 255 ? 255 : (unsigned char)bb);
    }

    free(u_buf);
    free(v_buf);
}

/* ---- USM锐化 (补偿降噪与均衡化导致的边缘软化) ---- */
static void _usm_sharpen(unsigned char *rgb, const unsigned char *y, int w, int h, double amount) {
    int total = w * h;
    unsigned char *blur = (unsigned char*)malloc(total);
    if (!blur) return;
    for (int yy = 1; yy < h - 1; yy++)
        for (int xx = 1; xx < w - 1; xx++) {
            int s = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    s += y[(yy + dy) * w + (xx + dx)];
            blur[yy * w + xx] = s / 9;
        }
    memcpy(blur, y, w);
    memcpy(blur + (h - 1) * w, y + (h - 1) * w, w);
    for (int yy = 1; yy < h - 1; yy++) {
        blur[yy * w] = y[yy * w];
        blur[yy * w + w - 1] = y[yy * w + w - 1];
    }

    for (int i = 0; i < total; i++) {
        int detail = y[i] - blur[i];
        int delta = (int)(detail * amount);
        if (delta == 0) continue;
        unsigned char *p = rgb + i * 3;
        for (int c = 0; c < 3; c++) {
            int v = p[c] + delta;
            p[c] = v < 0 ? 0 : (v > 255 ? 255 : (unsigned char)v);
        }
    }
    free(blur);
}

/* ---- 完整管线入口 ---- */
static void clahe_y(unsigned char *rgb, int w, int h) {
    int total = w * h;
    unsigned char *y = (unsigned char*)malloc(total);
    if (!y) return;

    _auto_wb(rgb, w, h, 2.0);                    /* 0. 自动白平衡 */

    _y_from_rgb(rgb, y, w, h);                  /* 1. RGB → Y */
    _clahe_y(y, w, h);                          /* 2. CLAHE 8×8 clip=2.5 */
    _y_smooth_edge(y, w, h, 10);                /* 3. 亮度保边降噪(thr=10, 强压制) */
    _y_stretch(y, w, h, 0.008);                 /* 4. 0.8%对比度拉伸, 提升通透度 */

    /* 5. Y → RGB比例缩放 */
    for (int i = 0; i < total; i++) {
        unsigned char *p = rgb + i * 3;
        int y_old = (p[0] * 114 + p[1] * 587 + p[2] * 299) / 1000;
        if (y_old == 0) continue;
        double s = (double)y[i] / y_old;
        for (int c = 0; c < 3; c++) {
            int v = (int)(p[c] * s);
            p[c] = v > 255 ? 255 : (unsigned char)v;
        }
    }

    _blur_uv(rgb, y, w, h);                     /* 6. UV自适应降噪(暗区2次,亮区1次) */
    _usm_sharpen(rgb, y, w, h, 0.30);           /* 7. USM锐化(amount=0.3补强细裂纹) */
    free(y);
}

/* ---- 推理线程 ---- */
static void *infer_thread(void *arg){
    (void)arg;
    const char *cls_names[]={"bird-drop","clean","dusty",
                              "electrical-damage","physical-damage","snow-covered"};
    /* 本地缓冲区：防止采集线程覆盖队列缓冲区的竞态条件 */
    unsigned char *local_rgb=malloc(DST_W*DST_H*3);
    int black_frames=0;  /* 连续黑屏帧计数 */

    while(g_running){
        Frame f={0};
        queue_pop(&g_cap_queue,&f);
        if(!g_running) break;

        memcpy(local_rgb, f.rgb, DST_W*DST_H*3);

        /* 画面亮度检测：跳过黑屏/遮挡帧，防止虚假clean检出 */
        unsigned int sum=0; int bs=32; int n=(DST_W*DST_H*3)/bs;
        for(int i=0;i<DST_W*DST_H*3;i+=bs) sum+=local_rgb[i];
        float mean=(float)sum/n;
        if(mean < BRIGHTNESS_MIN){
            black_frames++;
            if(black_frames<30 || black_frames%30==0)
                printf("[WARN] low brightness (mean=%.1f), frame #%d skipped\n",
                       mean, black_frames);
            f.result.count=0;
            queue_push(&g_disp_queue,&f);
            continue;
        }
        black_frames=0;

        /* 摄像头关闭时跳过推理，仅透传空帧 */
        if(!g_ui.camera_on){
            f.result.count=0;
            g_ui.detection_count=0;
            queue_push(&g_disp_queue,&f);
            continue;
        }

        /* === 方案A: 高光检测 — 饱和像素>30%跳过推理, 防反光误检 === */
        {
            int sat_cnt = 0;
            unsigned char *p = local_rgb;
            for (int i = 0; i < DST_W * DST_H; i++, p += 3) {
                if (p[0] > 240 && p[1] > 240 && p[2] > 240) sat_cnt++;
            }
            float sat_ratio = (float)sat_cnt / (DST_W * DST_H);
            if (sat_ratio > 0.30f) {
                static int sat_frames = 0;
                sat_frames++;
                if (sat_frames < 10 || sat_frames % 30 == 0)
                    printf("[WARN] high saturation (%.0f%%), frame skipped #%d\n",
                           sat_ratio * 100, sat_frames);
                f.result.count = 0;
                queue_push(&g_disp_queue, &f);
                continue;
            }
        }

        /* === 方案B: Y通道CLAHE — 限制对比度自适应直方图均衡化 (H键切换) === */
        if (g_hist_eq && mean < HEQ_BRIGHTNESS_MAX) {
            clahe_y(local_rgb, DST_W, DST_H);
        }

        rknn_input inputs[1];
        memset(inputs,0,sizeof(inputs));
        inputs[0].index=0;
        inputs[0].type=RKNN_TENSOR_UINT8;
        inputs[0].size=DST_W*DST_H*3;
        inputs[0].fmt=RKNN_TENSOR_NHWC;
        inputs[0].buf=local_rgb;
        f_inputs_set(g_ctx,1,inputs);
        f_run(g_ctx,NULL);

        rknn_output outputs[1];
        memset(outputs,0,sizeof(outputs));
        outputs[0].want_float=1;outputs[0].index=0;
        f_outputs_get(g_ctx,1,outputs,NULL);

        float *bbox=(float*)outputs[0].buf;
        float *cls=(float*)outputs[0].buf + 4*8400;
        Det dets[MAX_DETS]; int det_cnt=0;
        for(int a=0;a<8400&&det_cnt<MAX_DETS;a++){
            float cx=bbox[0*8400+a],cy=bbox[1*8400+a];
            float bw=bbox[2*8400+a],bh=bbox[3*8400+a];
            float best=-1; int bcls=0;
            for(int c=0;c<NUM_CLS;c++){
                float s=cls[c*8400+a]/100.0f;
                if(s>best){best=s;bcls=c;}
            }
            if(best < g_cls_thr[bcls]) continue;
            float x1=fmaxf(0,cx-bw/2),y1=fmaxf(0,cy-bh/2);
            float x2=fminf(DST_W,cx+bw/2),y2=fminf(DST_H,cy+bh/2);
            if((x2-x1)*(y2-y1)<400) continue;
            dets[det_cnt++]=(Det){x1,y1,x2,y2,best,bcls};
        }
        char sup[MAX_DETS]={0};
        for(int i=0;i<det_cnt-1;i++)
            for(int j=i+1;j<det_cnt;j++)
                if(dets[j].score>dets[i].score){Det t=dets[i];dets[i]=dets[j];dets[j]=t;}
        f.result.count=0;
        for(int i=0;i<det_cnt;i++){
            if(sup[i]) continue;
            f.result.dets[f.result.count++]=dets[i];
            for(int j=i+1;j<det_cnt;j++)
                if(!sup[j]&&dets[i].cls==dets[j].cls&&iou_calc(&dets[i],&dets[j])>IOU_THR)
                    sup[j]=1;
        }
        f_outputs_release(g_ctx,1,outputs);

        /* === 帧间一致性过滤: 连续TEMPORAL_WINDOW帧检出才确认, 消抖 === */
        {
            Det  matched_dets[MAX_DETS];
            int  matched_age[MAX_DETS];
            int  matched_cnt = 0;

            for (int i = 0; i < f.result.count; i++) {
                Det *cur = &f.result.dets[i];
                int best_j = -1;
                float best_iou = TEMPORAL_IOU;
                for (int j = 0; j < g_prev_cnt; j++) {
                    if (g_prev_dets[j].cls != cur->cls) continue;
                    float iou = iou_calc(cur, &g_prev_dets[j]);
                    if (iou > best_iou) { best_iou = iou; best_j = j; }
                }
                if (best_j >= 0) {
                    /* 历史匹配: 延续年龄 */
                    matched_dets[matched_cnt] = *cur;
                    matched_age[matched_cnt] = g_prev_age[best_j] + 1;
                    matched_cnt++;
                } else {
                    /* 新出现: 年龄从1开始 */
                    matched_dets[matched_cnt] = *cur;
                    matched_age[matched_cnt] = 1;
                    matched_cnt++;
                }
            }

            /* 保存当前帧结果供下一帧比对 */
            g_prev_cnt = matched_cnt;
            for (int i = 0; i < matched_cnt && i < MAX_DETS; i++) {
                g_prev_dets[i] = matched_dets[i];
                g_prev_age[i] = matched_age[i];
            }

            /* 仅保留连续出现足够帧数的检测 */
            int out_cnt = 0;
            for (int i = 0; i < matched_cnt; i++) {
                if (matched_age[i] >= TEMPORAL_WINDOW) {
                    f.result.dets[out_cnt++] = matched_dets[i];
                }
            }
            f.result.count = out_cnt;
        }

        /* 记录检测结果到UI日志(去重:仅变化时写入) */
        g_ui.detection_count=f.result.count;
        static char last_sig[128]="";
        char sig[128]="";
        for(int i=0;i<f.result.count&&i<5;i++){
            int ic=f.result.dets[i].cls;
            if(ic>=0&&ic<NUM_CLS){
                char tmp[32];snprintf(tmp,sizeof(tmp),"%s%.1f",cls_names[ic],f.result.dets[i].score);
                strncat(sig,tmp,sizeof(sig)-1);
            }
        }
        if(strcmp(sig,last_sig)!=0){
            strcpy(last_sig,sig);
            if(f.result.count==0) {
                ui_log_add("--- clear ---");
                web_add_record(-1, 0.0f);
            }
            else for(int i=0;i<f.result.count&&i<5;i++){
                int ic=f.result.dets[i].cls;float sc=f.result.dets[i].score;
                if(ic>=0&&ic<NUM_CLS){
                    ui_log_add("%-17s %.2f",cls_names[ic],sc);
                    web_add_record(ic, sc);
                }
            }
        }

        /* 更新面板巡检状态 */
        {
            float ax = g_servo.angle[0];
            int pn = (ax < 30) ? 0 : (ax < 90) ? 1 : 2;
            int best_c;
            if (f.result.count > 0) {
                best_c = f.result.dets[0].cls;
                if (best_c < 0 || best_c >= NUM_CLS) best_c = 1;
            } else {
                best_c = 1; /* 无检出 = 正常 */
            }
            web_update_panel(pn, best_c);
        }

        /* 每30帧打印诊断 */
        static int infer_frame=0;
        infer_frame++;
        if(infer_frame%30==0){
            printf("[F%04d] thr=%.2f mean=%.0f clahe=%s | cls_thr: B=%.2f C=%.2f D=%.2f E=%.2f P=%.2f S=%.2f | %d det",
                   infer_frame, g_conf_thr, mean,
                   g_hist_eq ? (mean < HEQ_BRIGHTNESS_MAX ? "ON" : "SKIP") : "OFF",
                   g_cls_thr[0],g_cls_thr[1],g_cls_thr[2],
                   g_cls_thr[3],g_cls_thr[4],g_cls_thr[5],
                   f.result.count);
            for(int i=0;i<f.result.count&&i<3;i++)
                printf(" %s(%.2f)", cls_names[f.result.dets[i].cls], f.result.dets[i].score);
            if(f.result.count==0){
                /* 显示最高分(可能是clean误检) */
                float top=-99;int tc=0;
                for(int a=0;a<8400;a++)for(int c=0;c<NUM_CLS;c++){
                    float s=cls[c*8400+a]/100.0f;if(s>top){top=s;tc=c;}
                }
                if(top>0.2f)printf(" top=%s(%.2f)",cls_names[tc],top);
            }
            printf("\n");
        }
        web_publish_frame(local_rgb, &f.result);
        /* 推入显示队列前换用 local_rgb，使显示线程也看到预处理后的画面 */
        unsigned char *_saved_rgb = f.rgb;
        f.rgb = local_rgb;
        queue_push(&g_disp_queue,&f);
        f.rgb = _saved_rgb;
    }
    free(local_rgb);
    return NULL;
}


/* ---- 显示线程 ---- */
static void *display_thread(void *arg){
    (void)arg;
    int frame_cnt=0;
    time_t t_start=time(NULL);

    /* 后缓冲：消除画面撕裂/频闪 */
    uint32_t *fb_back=malloc(SCR_W*SCR_H*4);

    int cam_size=UI_CAM_H;
    int cam_ox=(SCR_W-cam_size)/2;
    int cam_oy=UI_CAM_Y;
    int cam_step_x=(DST_W<<16)/cam_size;
    int cam_step_y=(DST_H<<16)/cam_size;

    while(g_running){
        Frame f={0};
        queue_pop(&g_disp_queue,&f);
        if(!g_running) break;

        /* 所有渲染到后缓冲 */
        _fb_fill(fb_back,SCR_W,0,0,SCR_W,SCR_H,COLOR_BG);

        if(g_ui.camera_on){
            /* 摄像头画面: 640x640 → 420x420 居中 (定点缩放) */
            int sy_fixed=0;
            for(int y=0;y<cam_size;y++){
                int sy=sy_fixed>>16;
                unsigned char *row=f.rgb+sy*DST_W*3;
                int py=cam_oy+y;
                uint32_t *fbr=&fb_back[py*SCR_W];
                int sx_fixed=0;
                for(int x=0;x<cam_size;x++){
                    int idx=(sx_fixed>>16)*3;
                    int px=cam_ox+x;
                    fbr[px]=((uint32_t)row[idx+2]<<16)|
                             ((uint32_t)row[idx+1]<<8)|
                             ((uint32_t)row[idx]);
                    sx_fixed+=cam_step_x;
                }
                sy_fixed+=cam_step_y;
            }

            /* 检测框叠加 */
            if(f.valid && f.result.count>0){
                float sc=(float)cam_size/DST_W;
                for(int i=0;i<f.result.count;i++){
                    Det *d=&f.result.dets[i];
                    int bx1=cam_ox+(int)(d->x1*sc), by1=cam_oy+(int)(d->y1*sc);
                    int bx2=cam_ox+(int)(d->x2*sc), by2=cam_oy+(int)(d->y2*sc);
                    uint32_t cc=cls_color(d->cls);
                    /* 画框(3px)直接用_fb_hline */
                    for(int t=0;t<3;t++){
                        _fb_hline(fb_back,SCR_W,bx1,by1+t,bx2-bx1,cc);
                        _fb_hline(fb_back,SCR_W,bx1,by2-t,bx2-bx1,cc);
                    }
                    for(int t=0;t<3;t++){
                        for(int yy=by1;yy<=by2;yy++){
                            _fb_pixel(fb_back,SCR_W,bx1+t,yy,cc);
                            _fb_pixel(fb_back,SCR_W,bx2-t,yy,cc);
                        }
                    }
                }
            }
        }

        /* UI叠加层 */
        ui_render_overlay(fb_back,SCR_W);

        /* 一次性写入DRM，消除频闪 */
        memcpy(g_drm.map,fb_back,SCR_W*SCR_H*4);

        frame_cnt++;
        if(frame_cnt%30==0){
            int elapsed=(int)(time(NULL)-t_start+1);
            g_ui.fps=frame_cnt/elapsed;
            g_web_fps = g_ui.fps;
            printf("FPS: %d\n",g_ui.fps);
        }
        g_ui.frame_count=frame_cnt;
    }
    free(fb_back);
    return NULL;
}

/* ---- 键盘输入线程 ---- */
static void *input_thread(void *arg){
    (void)arg;
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO,&old_t);
    new_t=old_t;
    new_t.c_lflag&=~(ICANON|ECHO);  /* 非规范模式, 无回显 */
    new_t.c_cc[VMIN]=0;             /* 非阻塞读 */
    new_t.c_cc[VTIME]=1;            /* 100ms超时 */
    tcsetattr(STDIN_FILENO,TCSANOW,&new_t);

    char c;
    while(g_running){
        if(read(STDIN_FILENO,&c,1)==1){
            switch(c){
            case 's': case 'S':
                g_ui.camera_on=!g_ui.camera_on;
                if(!g_ui.camera_on) g_ui.detection_count=0;
                ui_log_add("%s",g_ui.camera_on?"Camera ON":"Camera OFF");
                break;
            case '+': case '=':
                {g_conf_thr+=0.05f;if(g_conf_thr>0.95f)g_conf_thr=0.95f;
                 for(int i=0;i<NUM_CLS;i++)g_cls_thr[i]=g_conf_thr;}
                snprintf(g_ui.status_msg,sizeof(g_ui.status_msg),"CONF_THR=%.2f",g_conf_thr);
                break;
            case '-': case '_':
                {g_conf_thr-=0.05f;if(g_conf_thr<0.10f)g_conf_thr=0.10f;
                 for(int i=0;i<NUM_CLS;i++)g_cls_thr[i]=g_conf_thr;}
                snprintf(g_ui.status_msg,sizeof(g_ui.status_msg),"CONF_THR=%.2f",g_conf_thr);
                break;
            case 'r': case 'R':
                {g_conf_thr=CONF_THR_DEF;
                 g_cls_thr[0]=0.55f;g_cls_thr[1]=0.50f;g_cls_thr[2]=0.60f;
                 g_cls_thr[3]=0.50f;g_cls_thr[4]=0.50f;g_cls_thr[5]=0.50f;}
                snprintf(g_ui.status_msg,sizeof(g_ui.status_msg),"CONF_THR reset to %.2f",g_conf_thr);
                break;
            /* ---- 舵机控制 ---- */
            case '0':
                servo_reset(&g_servo);
                ui_log_add("Servo reset to 0");
                break;
            case '1': case '2': case '3':
            case '4': case '5': case '6':
            case '7': case '8': case '9':
                {
                    int idx = c - '1';
                    if (idx < g_servo.scan_total) {
                        float x = g_servo.scan_angles[idx][0];
                        float y = g_servo.scan_angles[idx][1];
                        servo_set_both(&g_servo, x, y);
                        char msg[32];
                        snprintf(msg, sizeof(msg), "Panel %d (%.0f,%.0f)", idx+1, x, y);
                        ui_log_add(msg);
                    }
                }
                break;
            case 'm': case 'M':
                g_servo.scan_mode = !g_servo.scan_mode;
                ui_log_add(g_servo.scan_mode ? "Scan mode ON" : "Scan mode OFF");
                if (g_servo.scan_mode) {
                    servo_scan_start(&g_servo);
                } else {
                    servo_scan_stop(&g_servo);
                }
                break;
            case 'h': case 'H':
                g_hist_eq = !g_hist_eq;
                ui_log_add("CLAHE %s", g_hist_eq ? "ON" : "OFF");
                snprintf(g_ui.status_msg, sizeof(g_ui.status_msg),
                         "CLAHE: %s", g_hist_eq ? "ON" : "OFF");
                break;
            /* ---- 退出 ---- */
            case 'q': case 'Q':
                g_running=0;
                pthread_cond_broadcast(&g_cap_queue.not_empty);
                pthread_cond_broadcast(&g_cap_queue.not_full);
                pthread_cond_broadcast(&g_disp_queue.not_empty);
                pthread_cond_broadcast(&g_disp_queue.not_full);
                break;
            }
        }
        usleep(50000);  /* 50ms轮询间隔 */
        /* 巡检模式: 到位后等待SCAN_DWELL_MS, 然后采集, 然后切换下一个角度 */
        if (g_servo.scan_mode && g_servo.scan_active) {
            static time_t scan_arrive = 0;
            static int scan_phase = 0; /* 0=等待稳定 1=已采集,等待切换 */
            if (scan_phase == 0) {
                if (scan_arrive == 0) scan_arrive = time(NULL);
                time_t elapsed = time(NULL) - scan_arrive;
                if (elapsed * 1000 >= SCAN_DWELL_MS) {
                    /* 稳定时间到, 触发一次手动采集 */
                    g_web_cap_cmd = 1;
                    scan_phase = 1;
                }
            }
            if (scan_phase == 1) {
                /* 等待采集完成 (给2秒推理时间), 然后切换 */
                static time_t scan_collect = 0;
                if (scan_collect == 0) scan_collect = time(NULL);
                if (time(NULL) - scan_collect >= 2) {
                    scan_collect = 0;
                    scan_phase = 0;
                    scan_arrive = 0;
                    if (!servo_scan_next(&g_servo)) {
                        /* 巡检完成 */
                        g_servo.scan_mode = 0;
                        ui_log_add("Scan complete");
                    }
                }
            }
        }
    }
    tcsetattr(STDIN_FILENO,TCSANOW,&old_t);
    return NULL;
}

/* ---- 主函数 ---- */
int main(){
    signal(SIGINT,sig_handler);
    signal(SIGTERM,sig_handler);

    ui_init();
    ui_log_add("System boot | Solar Defect Detection v4.0");
    ui_log_add("Platform: RV1126B | Model: YOLOv8n INT8");

    /* Servo */
    servo_init(&g_servo);
    /* 默认巡检面板角度: 0/30/60/90/120/150 度, 可在此修改 */
    float scan_x[] = {0, 30, 60, 90, 120, 150};
    float scan_y[] = {0, 0, 0, 0, 0, 0};
    servo_scan_config(&g_servo, 6, scan_x, scan_y);

    /* DRM */
    if(drm_init(&g_drm)<0) return 1;
    printf("DRM OK\n");

    /* RKNN */
    void *lib=dlopen("/usr/lib/librknnrt.so",RTLD_NOW);
    f_init           =dlsym(lib,"rknn_init");
    f_inputs_set     =dlsym(lib,"rknn_inputs_set");
    f_run            =dlsym(lib,"rknn_run");
    f_outputs_get    =dlsym(lib,"rknn_outputs_get");
    f_outputs_release=dlsym(lib,"rknn_outputs_release");
    f_destroy        =dlsym(lib,"rknn_destroy");
    FILE *fp=fopen("/root/solar_defect/best_scaled.rknn","rb");
    fseek(fp,0,SEEK_END); int sz=ftell(fp); rewind(fp);
    unsigned char *model=malloc(sz);
    fread(model,1,sz,fp); fclose(fp);
    g_ctx=0;
    f_init(&g_ctx,model,sz,0,NULL);
    free(model);
    char io_buf[256];
    memset(io_buf,0,256);
    ((int(*)(rknn_context,int,void*,unsigned int))dlsym(lib,"rknn_query"))(g_ctx,RKNN_QUERY_IN_OUT_NUM,io_buf,256);
    printf("n_input=%d n_output=%d\n",*(int*)io_buf,*(int*)(io_buf+4));
    printf("RKNN OK\n");

    /* V4L2 */
    g_v4l2_fd=open(DEV,O_RDWR);
    struct v4l2_format fmt={0};
    fmt.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width=SRC_W; fmt.fmt.pix.height=SRC_H;
    fmt.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field=V4L2_FIELD_NONE;
    xioctl(g_v4l2_fd,VIDIOC_S_FMT,&fmt);
    struct v4l2_requestbuffers req={0};
    req.count=BUF_CNT; req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory=V4L2_MEMORY_MMAP;
    xioctl(g_v4l2_fd,VIDIOC_REQBUFS,&req);
    for(int i=0;i<BUF_CNT;i++){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP; buf.index=i;
        xioctl(g_v4l2_fd,VIDIOC_QUERYBUF,&buf);
        g_bufs[i].length=buf.length;
        g_bufs[i].start=mmap(NULL,buf.length,PROT_READ|PROT_WRITE,
                             MAP_SHARED,g_v4l2_fd,buf.m.offset);
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);
    }
    enum v4l2_buf_type type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(g_v4l2_fd,VIDIOC_STREAMON,&type);
    /* 丢弃前5帧 */
    for(int i=0;i<5;i++){
        struct v4l2_buffer buf={0};
        buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory=V4L2_MEMORY_MMAP;
        fd_set fds; struct timeval tv={2,0};
        FD_ZERO(&fds); FD_SET(g_v4l2_fd,&fds);
        select(g_v4l2_fd+1,&fds,NULL,NULL,&tv);
        xioctl(g_v4l2_fd,VIDIOC_DQBUF,&buf);
        xioctl(g_v4l2_fd,VIDIOC_QBUF,&buf);
    }
    printf("V4L2 OK\n");

    /* 队列初始化 */
    queue_init(&g_cap_queue);
    queue_init(&g_disp_queue);

    /* 启动线程 */
    pthread_t t_cap,t_infer,t_disp,t_input;
    pthread_create(&t_cap,  NULL,capture_thread, NULL);
    pthread_create(&t_infer,NULL,infer_thread,   NULL);
    pthread_create(&t_disp, NULL,display_thread, NULL);
    pthread_create(&t_input,NULL,input_thread,   NULL);
    printf("Pipeline running | S:cam +/-:thr R:reset M:scan 1-9:pos Q:quit\n");

    /* 启动Web线程 */
    pthread_t t_web;
    pthread_create(&t_web, NULL, web_thread, NULL);

    pthread_join(t_cap,  NULL);
    pthread_join(t_infer,NULL);
    pthread_join(t_disp, NULL);
    pthread_join(t_input,NULL);
    pthread_join(t_web, NULL);

    /* 清理 */
    servo_cleanup(&g_servo);
    xioctl(g_v4l2_fd,VIDIOC_STREAMOFF,&type);
    for(int i=0;i<BUF_CNT;i++) munmap(g_bufs[i].start,g_bufs[i].length);
    close(g_v4l2_fd);
    f_destroy(g_ctx);
    dlclose(lib);
    if(g_drm.saved_crtc){
        drmModeSetCrtc(g_drm.fd,g_drm.saved_crtc->crtc_id,
                       g_drm.saved_crtc->buffer_id,0,0,
                       &g_drm.conn_id,1,&g_drm.saved_crtc->mode);
        drmModeFreeCrtc(g_drm.saved_crtc);
    }
    munmap(g_drm.map,g_drm.size);
    close(g_drm.fd);
    printf("Done\n");
    return 0;
}