/*
 * web_ui.h — 光伏板检测系统 Web UI
 * 纯C实现HTTP服务器 + MJPEG推流 + JSON API + 嵌入式HTML前端
 * 编译依赖: -ljpeg
 */
#ifndef WEB_UI_H
#define WEB_UI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jpeglib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Web配置 ========== */
#define WEB_PORT    8080
#define WEB_BACKLOG 8
#define REC_MAX     200
#define BUF_SZ      65536

/* ========== 检测记录 ========== */
typedef struct {
    time_t ts;
    int    cls;        /* -1=无检出/正常, 0=obstacle, 1=damage */
    float  conf;
    char   thumb[128]; /* 截图路径(暂未实现, 保留字段) */
} web_rec_t;

/* ========== Web全局状态 ========== */
static unsigned char  g_web_frame[640*640*3];  /* 最新帧 RGB888 */
static int            g_web_frame_ready = 0;
static DetResult      g_web_result;
static pthread_mutex_t g_web_mutex = PTHREAD_MUTEX_INITIALIZER;

/* JPEG缓存: 预编码一次, 所有stream连接复用 */
static unsigned char *g_web_jpeg     = NULL;
static unsigned long  g_web_jpeg_len = 0;
static int            g_web_jpeg_id  = -1;

static web_rec_t g_web_recs[REC_MAX];
static int       g_web_rec_cnt = 0;
static int       g_web_rec_head = 0;
static int       g_web_cap_cmd = 0;   /* 手动触发计数 */

/* 面板巡检状态 (3个角度区各对应一块光伏板) */
static int   g_panel_cls[8] = {-1,-1,-1,-1,-1,-1,-1,-1};
static time_t g_panel_ts[8] = {0};
void web_update_panel(int panel, int cls) {
    if (panel < 0 || panel >= 8) return;
    pthread_mutex_lock(&g_web_mutex);
    g_panel_cls[panel] = cls;
    g_panel_ts[panel] = time(NULL);
    pthread_mutex_unlock(&g_web_mutex);
}
static int       g_web_frame_id = 0;
volatile int      g_web_fps = 0;  /* 由display线程更新 */

/* ========== 前向声明 ========== */
extern volatile int g_running;
extern volatile int g_hist_eq;

/* ========== JPEG编码: RGB888 → JPEG 内存缓冲 ========== */
static int rgb_to_jpeg(unsigned char *rgb, int w, int h, int quality,
                       unsigned char **out, unsigned long *out_len) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, out, out_len);
    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    /* 当前帧缓冲实际是 BGR 顺序，编码前逐行转成 RGB */
    unsigned char *rowbuf = (unsigned char*)malloc(w * 3);
    if (!rowbuf) {
        jpeg_destroy_compress(&cinfo);
        return -1;
    }

    JSAMPROW row[1];
    while (cinfo.next_scanline < cinfo.image_height) {
        unsigned char *src = rgb + cinfo.next_scanline * w * 3;
        for (int x = 0; x < w; x++) {
            rowbuf[x*3 + 0] = src[x*3 + 2];
            rowbuf[x*3 + 1] = src[x*3 + 1];
            rowbuf[x*3 + 2] = src[x*3 + 0];
        }
        row[0] = (JSAMPROW)rowbuf;
        jpeg_write_scanlines(&cinfo, row, 1);
    }

    free(rowbuf);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return 0;
}

/* ========== HTTP工具 ========== */
static void urldecode(char *dst, const char *src, int max) {
    char a,b;
    while(*src && --max) {
        if(*src=='%' && ((a=src[1])&&(b=src[2])) &&
           ((a>='0'&&a<='9')||(a>='A'&&a<='F')||(a>='a'&&a<='f')) &&
           ((b>='0'&&b<='9')||(b>='A'&&b<='F')||(b>='a'&&b<='f'))) {
            a=(a<='9')?a-'0':(a<='F')?a-'A'+10:a-'a'+10;
            b=(b<='9')?b-'0':(b<='F')?b-'A'+10:b-'a'+10;
            *dst++=16*a+b; src+=3;
        } else if(*src=='+') { *dst++=' '; src++; }
        else { *dst++=*src++; }
    }
    *dst=0;
}

static void http_ok(int fd, const char *ctype, int clen) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Connection: close\r\n"
        "Content-Length: %d\r\n\r\n", ctype, clen);
    write(fd, buf, n);
}

static void http_200(int fd, const char *ctype) {
    char buf[256];
    int n = snprintf(buf, sizeof(buf),
        "HTTP/1.0 200 OK\r\nContent-Type: %s\r\n"
        "Cache-Control: no-cache\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n", ctype);
    write(fd, buf, n);
}

static void http_404(int fd) {
    const char *s = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    write(fd, s, strlen(s));
}

/* ========== 嵌入式HTML前端 (工业控制台 - 4模块) ========== */
static const char INDEX_HTML[] =
"<!DOCTYPE html>"
"<html lang=\"zh\">"
"<head>"
"<meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\">"
"<title>光伏板缺陷检测控制台</title>"
"<style>"
":root{--bg:#0b0e14;--panel:#11161e;--border:#1e2733;--accent:#238bff;--text:#bcc4d0;--dim:#667080;--red:#f5525b;--green:#30c86a;--amber:#f0a030;--purple:#a855f7;--teal:#2dd4bf;--orange:#f9733c;--warn:#facc15}"
"*{margin:0;padding:0;box-sizing:border-box}"
"body{font:13px/1.5 -apple-system,'PingFang SC','Microsoft YaHei',sans-serif;background:var(--bg);color:var(--text);display:flex;min-height:100vh;overflow:hidden}"
"/* Top Bar */"
".topbar{position:fixed;top:0;left:0;right:0;height:44px;background:#0d121a;border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 16px;z-index:100;gap:8px}"
".topbar .logo{font-size:15px;font-weight:700;color:#fff;white-space:nowrap}"
".topbar .logo span{color:var(--accent);margin:0 2px}"
".topbar .live{display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--green);margin-right:6px;animation:pulse 1.5s infinite}"
"@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}"
".topbar .ts{flex:1;text-align:right;font-size:11px;color:var(--dim);white-space:nowrap}"
"/* Sidebar */"
".sidebar{position:fixed;top:44px;left:0;bottom:0;width:210px;background:var(--panel);border-right:1px solid var(--border);overflow-y:auto;z-index:99;padding:8px 0}"
".sidebar .nav-item{display:flex;align-items:center;gap:10px;padding:10px 16px;cursor:pointer;font-size:13px;color:var(--dim);border-left:3px solid transparent;transition:all .15s;user-select:none}"
".sidebar .nav-item:hover{color:var(--text);background:rgba(255,255,255,.03)}"
".sidebar .nav-item.active{color:#fff;background:rgba(35,139,255,.1);border-left-color:var(--accent)}"
".sidebar .nav-item .ico{width:18px;text-align:center;font-size:14px}"
".sidebar .nav-label{font-size:10px;text-transform:uppercase;letter-spacing:.1em;color:var(--dim);padding:16px 16px 6px;font-weight:600}"
"/* Main */"
".main{margin:44px 0 0 210px;flex:1;overflow-y:auto;height:calc(100vh - 44px);padding:0}"
".page{display:none;padding:12px 16px}"
".page.active{display:block}"
"/* Stats cards */"
".stat-cards{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}"
".stat-card{flex:1;min-width:90px;background:var(--panel);border:1px solid var(--border);border-radius:6px;padding:10px 12px;display:flex;flex-direction:column;gap:2px}"
".stat-card .sv{font-size:22px;font-weight:700}"
".stat-card .sl{font-size:10px;color:var(--dim);text-transform:uppercase;letter-spacing:.03em}"
"/* Two-col */"
".row2{display:flex;gap:10px;flex-wrap:wrap}"
".col-l{flex:1;min-width:340px}"
".col-r{width:340px;min-width:280px}"
"/* Stream */"
".stream-box{background:#000;border:1px solid var(--border);border-radius:6px;overflow:hidden;position:relative;aspect-ratio:1;max-height:55vh}"
".stream-box img{width:100%;height:100%;object-fit:contain;display:block}"
".stream-box .ph{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;color:var(--dim);font-size:16px;pointer-events:none}"
"/* Panel */"
".panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;overflow:hidden;margin-bottom:10px}"
".panel .phd{display:flex;align-items:center;justify-content:space-between;padding:8px 12px;background:rgba(255,255,255,.02);border-bottom:1px solid var(--border);font-size:12px;font-weight:600;color:var(--dim)}"
".panel .pbd{padding:10px 12px}"
"/* Detection card */"
".det-card{display:flex;align-items:center;gap:10px;padding:8px 0;border-bottom:1px solid rgba(255,255,255,.04)}"
".det-card:last-child{border-bottom:none}"
".det-card .dc-cls{font-size:12px;font-weight:600;min-width:60px}"
".det-card .dc-conf{font-size:20px;font-weight:700;min-width:48px}"
".det-card .dc-box{font-size:11px;color:var(--dim)}"
".det-card .dc-dot{width:10px;height:10px;border-radius:3px;flex-shrink:0}"
".empty-state{text-align:center;padding:24px;color:var(--dim);font-size:13px}"
"/* Buttons */"
"button{border:none;padding:7px 14px;border-radius:5px;font-size:12px;cursor:pointer;font-weight:500;transition:all .15s;white-space:nowrap}"
".btn-pri{background:var(--accent);color:#fff}"
".btn-pri:active{opacity:.8}"
".btn-ghost{background:transparent;color:var(--text);border:1px solid var(--border)}"
".btn-ghost:active{background:rgba(255,255,255,.05)}"
".btn-danger{background:var(--red);color:#fff}"
".btn-danger:active{opacity:.8}"
".btn-group{display:flex;gap:6px;flex-wrap:wrap}"
"/* Table */"
".tbl-wrap{overflow-x:auto;max-height:420px;overflow-y:auto}"
"table{width:100%;border-collapse:collapse;font-size:12px}"
"th{text-align:left;padding:8px 10px;font-size:10px;font-weight:600;color:var(--dim);text-transform:uppercase;letter-spacing:.05em;border-bottom:1px solid var(--border);position:sticky;top:0;background:var(--panel);z-index:1}"
"td{padding:7px 10px;border-bottom:1px solid rgba(255,255,255,.04)}"
"tr:hover td{background:rgba(255,255,255,.02)}"
".badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:10px;font-weight:600}"
".badge-b{background:rgba(245,82,91,.2);color:var(--red)}"
".badge-c{background:rgba(48,200,106,.2);color:var(--green)}"
".badge-d{background:rgba(240,160,48,.2);color:var(--amber)}"
".badge-e{background:rgba(168,85,247,.2);color:var(--purple)}"
".badge-p{background:rgba(45,212,191,.2);color:var(--teal)}"
".badge-s{background:rgba(249,115,60,.2);color:var(--orange)}"
".badge-ok{background:rgba(48,200,106,.15);color:var(--green)}"
"/* Filter */"
".filter-row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:8px}"
"select,input[type=text]{background:var(--bg);border:1px solid var(--border);color:var(--text);padding:6px 10px;border-radius:4px;font-size:12px;outline:none}"
"select:focus,input[type=text]:focus{border-color:var(--accent)}"
"/* Sliders */"
".slider-group{margin-bottom:12px}"
".slider-group .sg-label{display:flex;justify-content:space-between;align-items:center;margin-bottom:4px}"
".slider-group .sg-name{font-size:12px;font-weight:600}"
".slider-group .sg-val{font-size:12px;color:var(--accent);font-weight:600}"
".slider-group input[type=range]{width:100%;accent-color:var(--accent);height:6px}"
"/* Charts */"
".chart-wrap{position:relative;max-width:600px;margin:0 auto}"
".chart-wrap canvas{display:block;max-width:100%;height:auto}"
"/* Setting card */"
".set-card{background:var(--panel);border:1px solid var(--border);border-radius:6px;padding:14px;margin-bottom:10px}"
".set-card h4{font-size:13px;margin-bottom:8px;color:#fff}"
"/* Model info */"
".info-grid{display:grid;grid-template-columns:1fr 1fr;gap:4px 16px;font-size:12px}"
".info-grid .ik{color:var(--dim)}"
".info-grid .iv{color:var(--text)}"
"/* Pagination */"
".pagination{display:flex;align-items:center;justify-content:center;gap:8px;padding:8px;font-size:12px}"
".pagination button{min-width:32px;text-align:center}"
".pagination .pg-info{color:var(--dim)}"
"/* Responsive */"
"@media(max-width:768px){.sidebar{width:52px}.sidebar .nav-item span:not(.ico){display:none}.sidebar .nav-label{display:none}.main{margin-left:52px}.col-r{width:100%}}"
"/* Login */"
".login-overlay{position:fixed;inset:0;background:var(--bg);z-index:9999;display:flex;align-items:center;justify-content:center}"
".login-card{background:var(--panel);border:1px solid var(--border);border-radius:8px;padding:32px;width:320px;text-align:center}"
".login-card h2{color:#fff;font-size:18px;margin-bottom:4px}"
".login-card .sub{color:var(--dim);font-size:11px;margin-bottom:20px}"
".login-card input{width:100%;background:var(--bg);border:1px solid var(--border);color:var(--text);padding:10px 12px;border-radius:5px;font-size:13px;margin-bottom:10px;outline:none}"
".login-card input:focus{border-color:var(--accent)}"
".login-card .login-btn{width:100%;padding:10px;background:var(--accent);color:#fff;border:none;border-radius:5px;font-size:14px;cursor:pointer;font-weight:600;margin-top:6px}"
".login-card .login-btn:active{opacity:.8}"
".login-card .err{color:var(--red);font-size:11px;margin-top:8px;display:none}"
"</style>"
"</head>"
"<body>"
""
"<!-- LOGIN OVERLAY -->"
"<div class=\"login-overlay\" id=\"login_overlay\">"
"  <div class=\"login-card\">"
"    <h2>光伏板缺陷检测控制台</h2>"
"    <div class=\"sub\">请登录以继续</div>"
"    <input type=\"text\" id=\"login_user\" placeholder=\"用户名\" autocomplete=\"off\">"
"    <input type=\"password\" id=\"login_pass\" placeholder=\"密码\">"
"    <button class=\"login-btn\" onclick=\"doLogin()\">登 录</button>"
"    <div class=\"err\" id=\"login_err\">用户名或密码错误</div>"
"  </div>"
"</div>"
""
"<!-- TOP BAR -->"
"<div class=\"topbar\">"
"  <div class=\"logo\"><span class=\"live\"></span>光伏板<span>缺陷检测</span>控制台</div>"
"  <div style=\"display:flex;gap:14px;font-size:12px;color:var(--text)\">"
"    <span>FPS <b style=\"color:var(--accent)\" id=\"tb_fps\">--</b></span>"
"    <span>检出 <b style=\"color:var(--text)\" id=\"tb_det\">--</b></span>"
"    <span>阈值 <b style=\"color:var(--amber)\" id=\"tb_thr\">--</b></span>"
"    <span id=\"tb_heq\"></span>"
"    <span id=\"tb_cam\" style=\"color:var(--green)\">● 摄像头</span>"
"  </div>"
"  <div class=\"ts\" id=\"tb_time\">--</div>"
"</div>"
""
"<!-- SIDEBAR -->"
"<div class=\"sidebar\">"
"  <div class=\"nav-label\">功能模块</div>"
"  <div class=\"nav-item active\" data-page=\"monitor\"><span class=\"ico\">&#9673;</span><span>实时监控</span></div>"
"  <div class=\"nav-item\" data-page=\"records\"><span class=\"ico\">&#9776;</span><span>检测记录</span></div>"
"  <div class=\"nav-item\" data-page=\"stats\"><span class=\"ico\">&#9783;</span><span>统计分析</span></div>"
"  <div class=\"nav-item\" data-page=\"settings\"><span class=\"ico\">&#9881;</span><span>系统设置</span></div>"
"</div>"
""
"<!-- MAIN CONTENT -->"
"<div class=\"main\">"
""
"<!-- PAGE: 实时监控 -->"
"<div class=\"page active\" id=\"page-monitor\">"
"  <div class=\"row2\">"
"    <div class=\"col-l\">"
"      <div class=\"stream-box\">"
"        <img id=\"stream\" src=\"\">"
"        <div class=\"ph\" id=\"stream_ph\">点击「开启视频流」查看实时画面</div>"
"      </div>"
"      <div class=\"btn-group\" style=\"margin-top:8px\">"
"        <button class=\"btn-pri\" id=\"btn_stream\" onclick=\"toggleStream()\">开启视频流</button>"
"        <button class=\"btn-ghost\" id=\"btn_cam\" onclick=\"toggleCam()\">关闭摄像头</button>"
"        <button class=\"btn-ghost\" onclick=\"manualCapture()\">手动采集</button>"
"      </div>"
"    </div>"
"    <div class=\"col-r\">"
"      <div class=\"panel\">"
"        <div class=\"phd\"><span>当前帧检测结果</span><span style=\"font-size:10px;color:var(--dim)\" id=\"det_ts\">--</span></div>"
"        <div class=\"pbd\" id=\"det_cards\">"
"          <div class=\"empty-state\">等待检测数据...</div>"
"        </div>"
"      </div>"
"      <div class=\"panel\">"
"        <div class=\"phd\"><span>近5分钟缺陷趋势</span></div>"
"        <div class=\"pbd\"><div class=\"chart-wrap\"><canvas id=\"chart_trend\" width=\"320\" height=\"160\"></canvas></div></div>"
"      </div>"
"      <div class=\"stat-cards\">"
"        <div class=\"stat-card\"><div class=\"sv\" style=\"color:var(--accent)\" id=\"sc_fps\">--</div><div class=\"sl\">实时 FPS</div></div>"
"        <div class=\"stat-card\"><div class=\"sv\" style=\"color:var(--green)\" id=\"sc_total\">--</div><div class=\"sl\">检测总数</div></div>"
"        <div class=\"stat-card\"><div class=\"sv\" style=\"color:var(--red)\" id=\"sc_bad\">--</div><div class=\"sl\">缺陷数</div></div>"
"        <div class=\"stat-card\"><div class=\"sv\" style=\"color:var(--amber)\" id=\"sc_thr\">--</div><div class=\"sl\">置信阈值</div></div>"
"      </div>"
"      <div class=\"panel\" style=\"margin-top:8px\">"
"        <div class=\"phd\"><span>面板巡检状态</span></div>"
"        <div class=\"pbd\" style=\"padding:0\">"
"          <table style=\"margin:0\"><thead><tr>"
"            <th style=\"width:50px\">编号</th><th style=\"width:80px\">状态</th><th>更新时间</th>"
"          </tr></thead><tbody id=\"panel_tb\">"
"            <tr><td><b style=\"color:#238bff\">#1</b></td>"
"              <td id=\"ps_0\" style=\"color:var(--dim);font-size:12px\">--</td>"
"              <td id=\"pt_0\" style=\"font-size:11px;color:var(--dim)\">--</td></tr>"
"            <tr><td><b style=\"color:#238bff\">#2</b></td>"
"              <td id=\"ps_1\" style=\"color:var(--dim);font-size:12px\">--</td>"
"              <td id=\"pt_1\" style=\"font-size:11px;color:var(--dim)\">--</td></tr>"
"            <tr><td><b style=\"color:#238bff\">#3</b></td>"
"              <td id=\"ps_2\" style=\"color:var(--dim);font-size:12px\">--</td>"
"              <td id=\"pt_2\" style=\"font-size:11px;color:var(--dim)\">--</td></tr>"
"          </tbody></table>"
"        </div>"
"      </div>"
"    </div>"
"  </div>"
"</div>"
""
"<!-- PAGE: 检测记录 -->"
"<div class=\"page\" id=\"page-records\">"
"  <div class=\"stat-cards\" id=\"rec_stats\"></div>"
"  <div class=\"filter-row\">"
"    <select id=\"rec_filter\" onchange=\"loadRecords()\">"
"      <option value=\"-1\">全部类别</option>"
"      <option value=\"0\">鸟粪</option><option value=\"1\">正常</option><option value=\"2\">灰尘</option>"
"      <option value=\"3\">电气损伤</option><option value=\"4\">物理损伤</option><option value=\"5\">积雪</option>"
"    </select>"
"    <button class=\"btn-ghost\" onclick=\"loadRecords()\">刷新</button>"
"    <span style=\"font-size:11px;color:var(--dim)\" id=\"rec_info\"></span>"
"  </div>"
"  <div class=\"panel\">"
"    <div class=\"tbl-wrap\">"
"      <table><thead><tr>"
"        <th>时间</th><th>类别</th><th>置信度</th><th>状态</th>"
"      </tr></thead><tbody id=\"rec_tbody\"></tbody></table>"
"    </div>"
"    <div class=\"pagination\" id=\"rec_pager\"></div>"
"  </div>"
"</div>"
""
"<!-- PAGE: 统计分析 -->"
"<div class=\"page\" id=\"page-stats\">"
"  <div class=\"row2\">"
"    <div class=\"col-l\">"
"      <div class=\"panel\">"
"        <div class=\"phd\">缺陷类别分布</div>"
"        <div class=\"pbd\"><div class=\"chart-wrap\"><canvas id=\"chart_pie\" width=\"380\" height=\"300\"></canvas></div></div>"
"      </div>"
"    </div>"
"    <div class=\"col-r\">"
"      <div class=\"panel\">"
"        <div class=\"phd\">各类别平均置信度</div>"
"        <div class=\"pbd\"><div class=\"chart-wrap\"><canvas id=\"chart_gauge\" width=\"320\" height=\"300\"></canvas></div></div>"
"      </div>"
"    </div>"
"  </div>"
"  <div class=\"panel\">"
"    <div class=\"phd\">24小时检出趋势</div>"
"    <div class=\"pbd\"><div class=\"chart-wrap\"><canvas id=\"chart_bar\" width=\"700\" height=\"240\"></canvas></div></div>"
"  </div>"
"</div>"
""
"<!-- PAGE: 系统设置 -->"
"<div class=\"page\" id=\"page-settings\">"
"  <div class=\"set-card\">"
"    <h4>各类别置信度阈值</h4>"
"    <div class=\"btn-group\" style=\"margin-bottom:10px\">"
"      <button class=\"btn-ghost\" onclick=\"resetThrDefaults()\">推荐默认</button>"
"      <button class=\"btn-ghost\" onclick=\"setAllThr(0.65)\">全部设为 0.65</button>"
"    </div>"
"    <div id=\"thr_sliders\"></div>"
"  </div>"
"  <div class=\"set-card\">"
"    <h4>图像预处理</h4>"
"    <div style=\"display:flex;align-items:center;justify-content:space-between\">"
"      <span style=\"font-size:12px;color:var(--text)\">CLAHE 暗光增强 <span style=\"color:var(--dim);font-size:10px\">(Y通道CLAHE, 低照度改善细节)</span></span>"
"      <button id=\"btn_histeq\" class=\"btn-pri\" onclick=\"toggleHistEq()\">开启</button>"
"    </div>"
"  </div>"
"  <div class=\"set-card\">"
"    <h4>摄像头控制</h4>"
"    <div class=\"btn-group\">"
"      <button class=\"btn-pri\" id=\"set_btn_cam\" onclick=\"toggleCam()\">关闭摄像头</button>"
"    </div>"
"  </div>"
"  <div class=\"set-card\">"
"    <h4>舵机巡检控制</h4>"
"    <div style=\"display:flex;gap:8px;margin-bottom:8px;align-items:center\">"
"      <span style=\"font-size:12px;color:var(--dim);min-width:60px\">X轴角度</span>"
"      <input type=\"range\" min=\"0\" max=\"180\" value=\"0\" id=\"sv_x\" oninput=\"svSet(0,this.value)\" style=\"flex:1;accent-color:var(--accent)\">"
"      <span style=\"font-size:12px;color:#fff;min-width:36px\" id=\"sv_xv\">0°</span>"
"    </div>"
"    <div style=\"display:flex;gap:8px;margin-bottom:10px;align-items:center\">"
"      <span style=\"font-size:12px;color:var(--dim);min-width:60px\">Y轴角度</span>"
"      <input type=\"range\" min=\"0\" max=\"180\" value=\"0\" id=\"sv_y\" oninput=\"svSet(1,this.value)\" style=\"flex:1;accent-color:var(--accent)\">"
"      <span style=\"font-size:12px;color:#fff;min-width:36px\" id=\"sv_yv\">0°</span>"
"    </div>"
"    <div class=\"btn-group\">"
"      <button class=\"btn-ghost\" onclick=\"servoCmd('reset')\">复位 0°</button>"
"      <button class=\"btn-pri\" onclick=\"servoCmd('scan')\">开始巡检</button>"
"      <button class=\"btn-danger\" onclick=\"servoCmd('stop')\">停止巡检</button>"
"    </div>"
"    <div style=\"font-size:11px;color:var(--dim);margin-top:6px\" id=\"sv_status\">就绪</div>"
"  </div>"
"  <div class=\"set-card\">"
"    <h4>模型信息</h4>"
"    <div class=\"info-grid\">"
"      <div class=\"ik\">模型架构</div><div class=\"iv\">YOLOv8n</div>"
"      <div class=\"ik\">量化方式</div><div class=\"iv\">INT8 (cls×100)</div>"
"      <div class=\"ik\">输入尺寸</div><div class=\"iv\">640×640</div>"
"      <div class=\"ik\">类别数量</div><div class=\"iv\">6 类</div>"
"      <div class=\"ik\">推理框架</div><div class=\"iv\">RKNN v2.3.2</div>"
"      <div class=\"ik\">NPU 算力</div><div class=\"iv\">3 TOPS @INT8</div>"
"      <div class=\"ik\">平台</div><div class=\"iv\">RV1126B</div>"
"      <div class=\"ik\">模型文件</div><div class=\"iv\">best_scaled.rknn</div>"
"    </div>"
"  </div>"
"</div>"
""
"</div><!-- /main -->"
""
"<script>"
"/* ====== LOGIN ====== */"
"function doLogin(){"
"  var u=document.getElementById('login_user').value;"
"  var p=document.getElementById('login_pass').value;"
"  if(u=='admin'&&p=='admin'){"
"    document.getElementById('login_overlay').style.display='none';"
"    sessionStorage.setItem('solar_login','1');"
"  }else{document.getElementById('login_err').style.display='block'}"
"}"
"document.addEventListener('keydown',function(e){"
"  if(e.key=='Enter'&&document.getElementById('login_overlay').style.display!='none'){doLogin()}"
"});"
"if(sessionStorage.getItem('solar_login')=='1'){"
"  document.getElementById('login_overlay').style.display='none';"
"}"
""
"/* ====== GLOBALS ====== */"
"var CLS=['鸟粪','正常','灰尘','电气损伤','物理损伤','积雪'];"
"var CLS_COLORS=['#f5525b','#30c86a','#f0a030','#a855f7','#2dd4bf','#f9733c'];"
"var BADGE=['badge-b','badge-c','badge-d','badge-e','badge-p','badge-s'];"
"var streaming=false,camOn=true,currentPage='monitor';"
"var trendData=[]; /* [{ts,count}] last 5min */"
"var allRecords=[];"
"var REC_PER_PAGE=20,recPage=0;"
""
"/* ====== API ====== */"
"function GET(u,cb){fetch(u).then(function(r){return r.json()}).then(cb).catch(function(e){console.error(e)})}"
"function POST(u,bd){fetch(u,{method:'POST',body:bd||''}).catch(function(e){console.error(e)})}"
""
"/* ====== NAVIGATION ====== */"
"document.querySelectorAll('.nav-item').forEach(function(el){"
"  el.addEventListener('click',function(){"
"    document.querySelectorAll('.nav-item').forEach(function(n){n.classList.remove('active')});"
"    el.classList.add('active');"
"    currentPage=el.dataset.page;"
"    document.querySelectorAll('.page').forEach(function(p){p.classList.remove('active')});"
"    document.getElementById('page-'+currentPage).classList.add('active');"
"    if(currentPage=='stats'){setTimeout(drawStats,200)}"
"    if(currentPage=='records') loadRecords();"
"    if(currentPage=='settings') buildThrSliders();"
"  })"
"});"
""
"/* ====== STREAM ====== */"
"function toggleStream(){"
"  streaming=!streaming;"
"  if(streaming){"
"    document.getElementById('stream').src='/stream?ts='+Date.now();"
"    document.getElementById('stream_ph').style.display='none';"
"    document.getElementById('btn_stream').textContent='关闭视频流';"
"    document.getElementById('btn_stream').className='btn-danger';"
"  }else{"
"    document.getElementById('stream').src='';"
"    document.getElementById('stream_ph').style.display='flex';"
"    document.getElementById('btn_stream').textContent='开启视频流';"
"    document.getElementById('btn_stream').className='btn-pri';"
"  }"
"}"
""
"/* ====== CAMERA ====== */"
"function toggleCam(){"
"  POST('/capture?cmd=toggle');"
"  setTimeout(updateQuick,400);"
"}"
""
"function manualCapture(){POST('/capture')}"
"function toggleHistEq(){"
"  POST('/histeq');"
"  setTimeout(updateQuick,400);"
"}"
""
"/* ====== THRESHOLD ====== */"
"function setThr(cls,val){"
"  POST('/threshold?cls='+cls+'&value='+val.toFixed(2));"
"}"
""
"function setAllThr(v){"
"  POST('/threshold?value='+v.toFixed(2));"
"  setTimeout(updateQuick,300);"
"}"
"function resetThrDefaults(){"
"  var def=[0.55,0.50,0.60,0.50,0.50,0.50];"
"  for(var i=0;i<6;i++) POST('/threshold?cls='+i+'&value='+def[i].toFixed(2));"
"  setTimeout(updateQuick,500);"
"}"
""
"/* ====== SERVO CONTROL ====== */"
"var _svTimer=null;"
"function svSet(axis,val){"
"  var el=document.getElementById('sv_'+['x','y'][axis]+'v');"
"  if(el) el.textContent=val+'°';"
"  clearTimeout(_svTimer); _svTimer=setTimeout(function(){"
"    POST('/servo?axis='+axis+'&angle='+val);"
"  },150);"
"}"
"function servoCmd(cmd){"
"  POST('/servo?cmd='+cmd);"
"  var st=document.getElementById('sv_status');"
"  if(st){"
"    if(cmd=='scan') st.textContent='巡检运行中...';"
"    else if(cmd=='reset') st.textContent='已复位到0°';"
"    else st.textContent='已停止';"
"  }"
"  setTimeout(updateServoStatus,500);"
"}"
"function updateServoStatus(){"
"  GET('/servo?cmd=status',function(d){"
"    var x=parseFloat(d.x)||0, y=parseFloat(d.y)||0;"
"    var sx=document.getElementById('sv_x'), sy=document.getElementById('sv_y');"
"    if(sx) sx.value=Math.round(x); if(sy) sy.value=Math.round(y);"
"    var sv=document.getElementById('sv_xv'), syv=document.getElementById('sv_yv');"
"    if(sv) sv.textContent=Math.round(x)+'°'; if(syv) syv.textContent=Math.round(y)+'°';"
"    var st=document.getElementById('sv_status');"
"    if(st) st.textContent=d.scan?'巡检运行中...':'就绪';"
"  });"
"}"
""
"/* ====== CANVAS CHARTS ====== */"
""
"/* Pie chart */"
"function drawPie(canvas,data,labels,colors){"
"  var ctx=canvas.getContext('2d');"
"  var w=canvas.width,h=canvas.height;"
"  ctx.clearRect(0,0,w,h);"
"  var total=data.reduce(function(a,b){return a+b},0);"
"  if(total==0){"
"    ctx.fillStyle='#667080';ctx.font='13px sans-serif';"
"    ctx.textAlign='center';ctx.fillText('暂无数据',w/2,h/2);"
"    return;"
"  }"
"  var cx=w*.38,cy=h*.48,r=Math.min(cx,cy)-10;"
"  var start=-Math.PI/2;"
"  for(var i=0;i<data.length;i++){"
"    if(data[i]==0) continue;"
"    var slice=data[i]/total*Math.PI*2;"
"    ctx.beginPath();ctx.moveTo(cx,cy);"
"    ctx.arc(cx,cy,r,start,start+slice);"
"    ctx.closePath();ctx.fillStyle=colors[i];ctx.fill();"
"    ctx.strokeStyle='#0b0e14';ctx.lineWidth=1.5;ctx.stroke();"
"    start+=slice;"
"  }"
"  /* Legend */"
"  var lx=w*.68,ly=20;"
"  ctx.font='11px sans-serif';"
"  for(var i=0;i<data.length;i++){"
"    var y=ly+i*26;"
"    ctx.fillStyle=colors[i];ctx.fillRect(lx,y,10,10);"
"    ctx.fillStyle='#bcc4d0';ctx.textAlign='left';"
"    var pct=total>0?(data[i]/total*100).toFixed(1):'0.0';"
"    ctx.fillText(labels[i]+'  '+data[i]+' ('+pct+'%)',lx+16,y+9);"
"  }"
"}"
""
"/* Bar chart */"
"function drawBar(canvas,data,labels,colors){"
"  var ctx=canvas.getContext('2d');"
"  var w=canvas.width,h=canvas.height;"
"  ctx.clearRect(0,0,w,h);"
"  var max=Math.max.apply(null,data)||1;"
"  var padL=32,padR=8,padT=16,padB=28;"
"  var bw=(w-padL-padR)/data.length*.6;"
"  var gap=(w-padL-padR)/data.length*.4;"
"  /* Grid */"
"  ctx.strokeStyle='rgba(255,255,255,.06)';"
"  ctx.lineWidth=.5;"
"  for(var i=0;i<=4;i++){"
"    var gy=padT+(h-padT-padB)*i/4;"
"    ctx.beginPath();ctx.moveTo(padL,gy);ctx.lineTo(w-padR,gy);ctx.stroke();"
"    ctx.fillStyle='#667080';ctx.font='9px sans-serif';ctx.textAlign='right';"
"    ctx.fillText(Math.round(max*(4-i)/4),padL-6,gy+3);"
"  }"
"  /* Bars */"
"  for(var i=0;i<data.length;i++){"
"    var bh2=(h-padT-padB)*data[i]/max;"
"    var x=padL+i*(bw+gap),y=h-padB-bh2;"
"    ctx.fillStyle=colors||'#238bff';ctx.fillRect(x,y,bw,bh2);"
"    ctx.fillStyle='#667080';ctx.font='9px sans-serif';ctx.textAlign='center';"
"    ctx.fillText(labels[i],x+bw/2,h-8);"
"  }"
"}"
""
"/* Line chart (trend) */"
"function drawLine(canvas,points,color,maxVal){"
"  if(!canvas)return;"
"  var ctx=canvas.getContext('2d');"
"  var w=canvas.width,h=canvas.height;"
"  ctx.clearRect(0,0,w,h);"
"  var padL=36,padR=8,padT=8,padB=24;"
"  if(!points||points.length<2){"
"    ctx.fillStyle='#667080';ctx.font='12px sans-serif';"
"    ctx.textAlign='center';ctx.fillText('数据收集中...',w/2,h/2);"
"    return;"
"  }"
"  maxVal=maxVal||Math.max.apply(null,points.map(function(p){return p.v}))||5;"
"  /* Grid */"
"  ctx.strokeStyle='rgba(255,255,255,.06)';ctx.lineWidth=.5;"
"  for(var i=0;i<=4;i++){"
"    var gy=padT+(h-padT-padB)*i/4;"
"    ctx.beginPath();ctx.moveTo(padL,gy);ctx.lineTo(w-padR,gy);ctx.stroke();"
"  }"
"  var step=(w-padL-padR)/(points.length-1);"
"  /* Fill */"
"  ctx.beginPath();"
"  ctx.moveTo(padL,h-padB);"
"  for(var i=0;i<points.length;i++){"
"    var px=padL+i*step,py=padT+(h-padT-padB)*(1-points[i].v/maxVal);"
"    ctx.lineTo(px,py);"
"  }"
"  ctx.lineTo(padL+(points.length-1)*step,h-padB);"
"  ctx.closePath();"
"  ctx.fillStyle=color+'20';ctx.fill();"
"  /* Line */"
"  ctx.beginPath();ctx.strokeStyle=color;ctx.lineWidth=2;"
"  for(var i=0;i<points.length;i++){"
"    var px=padL+i*step,py=padT+(h-padT-padB)*(1-points[i].v/maxVal);"
"    if(i==0) ctx.moveTo(px,py);else ctx.lineTo(px,py);"
"  }"
"  ctx.stroke();"
"  /* Dots */"
"  for(var i=0;i<points.length;i++){"
"    var px=padL+i*step,py=padT+(h-padT-padB)*(1-points[i].v/maxVal);"
"    ctx.beginPath();ctx.arc(px,py,3,0,Math.PI*2);ctx.fillStyle=color;ctx.fill();"
"  }"
"  /* Labels */"
"  ctx.fillStyle='#667080';ctx.font='9px sans-serif';ctx.textAlign='center';"
"  for(var i=0;i<points.length;i+=Math.max(1,Math.floor(points.length/5))){"
"    ctx.fillText(points[i].t,padL+i*step,h-8);"
"  }"
"}"
""
"/* Gauge */"
"function drawGauge(canvas,val,label,color){"
"  var ctx=canvas.getContext('2d');"
"  var w=canvas.width,h=canvas.height;"
"  ctx.clearRect(0,0,w,h);"
"  var cx=w/2,cy=h*.7,r=Math.min(cx-4,cy-4)-10;"
"  /* Arc */"
"  ctx.beginPath();ctx.arc(cx,cy,r,Math.PI,0);"
"  ctx.strokeStyle='rgba(255,255,255,.08)';ctx.lineWidth=12;ctx.stroke();"
"  /* Value arc */"
"  ctx.beginPath();ctx.arc(cx,cy,r,Math.PI,Math.PI+val*Math.PI);"
"  ctx.strokeStyle=color;ctx.lineWidth=12;ctx.stroke();"
"  /* Needle */"
"  var angle=Math.PI+val*Math.PI;"
"  ctx.beginPath();ctx.moveTo(cx,cy);"
"  ctx.lineTo(cx+Math.cos(angle)*r*.85,cy+Math.sin(angle)*r*.85);"
"  ctx.strokeStyle='#fff';ctx.lineWidth=2;ctx.stroke();"
"  ctx.beginPath();ctx.arc(cx,cy,6,0,Math.PI*2);ctx.fillStyle='#fff';ctx.fill();"
"  /* Value text */"
"  ctx.fillStyle='#fff';ctx.font='bold 22px sans-serif';ctx.textAlign='center';"
"  ctx.fillText((val*100).toFixed(0)+'%',cx,cy-30);"
"  ctx.fillStyle='#667080';ctx.font='11px sans-serif';"
"  ctx.fillText(label,cx,cy-10);"
"  /* Scale */"
"  ctx.fillStyle='#667080';ctx.font='9px sans-serif';"
"  ctx.fillText('0%',cx-r,cy+16);ctx.fillText('100%',cx+r,cy+16);"
"}"
""
"/* ====== STATS DRAWING ====== */"
"function drawStats(){"
"  var data=allRecords.length>0?allRecords:allDataCache||[];"
"  /* Pie */"
"  var pieData=[0,0,0,0,0,0];"
"  data.forEach(function(r){if(r.cls>=0&&r.cls<6) pieData[r.cls]++});"
"  drawPie(document.getElementById('chart_pie'),pieData,CLS,CLS_COLORS);"
"  /* Bar: 24h */"
"  var barData=[],barLabels=[];"
"  var now=Math.floor(Date.now()/1000);"
"  for(var h=0;h<24;h++){barData[h]=0;barLabels[h]=h+'时'}"
"  data.forEach(function(r){"
"    var d=new Date(r.ts*1000);"
"    var age=(now-r.ts)/3600;"
"    if(age>=0&&age<24) barData[d.getHours()]++;"
"  });"
"  drawBar(document.getElementById('chart_bar'),barData,barLabels,'#238bff');"
"  /* Gauge: avg confidence per class */"
"  var gaugeData=[],gaugeN=[];"
"  for(var i=0;i<6;i++){gaugeData[i]=0;gaugeN[i]=0}"
"  data.forEach(function(r){if(r.cls>=0&&r.cls<6){gaugeData[r.cls]+=r.conf;gaugeN[r.cls]++}});"
"  var bestI=0,bestAvg=0;"
"  for(var i=0;i<6;i++){"
"    var avg=gaugeN[i]>0?gaugeData[i]/gaugeN[i]:0;"
"    if(avg>bestAvg){bestAvg=avg;bestI=i}"
"  }"
"  drawGauge(document.getElementById('chart_gauge'),"
"    bestAvg,CLS[bestI]+' 平均置信度',CLS_COLORS[bestI]);"
"}"
""
"var allDataCache=[];"
"var tickCount=0;"
""
"/* ====== TOP BAR + MONITOR UPDATE ====== */"
"function updateQuick(){"
"  GET('/stats',function(d){"
"    allDataCache=d;"
"    document.getElementById('tb_fps').textContent=d.fps||'--';"
"    document.getElementById('tb_det').textContent=d.defects;"
"    document.getElementById('tb_thr').textContent=d.cls_thr?d.cls_thr[0].toFixed(2):d.threshold.toFixed(2);"
"    document.getElementById('tb_cam').innerHTML=(d.camera?'<span style=\"color:#30c86a\">● 摄像头:开</span>':'<span style=\"color:#f5525b\">● 摄像头:关</span>');"
"    document.getElementById('tb_heq').innerHTML=d.hist_eq?'<span style=\"color:#f0a030\">● CLAHE</span>':'';"
"    /* CLAHE按钮状态 */"
"    var heBtn=document.getElementById('btn_histeq');"
"    if(heBtn){heBtn.textContent=d.hist_eq?'关闭':'开启';heBtn.className=d.hist_eq?'btn-danger':'btn-pri'}"
"    document.getElementById('tb_time').textContent=new Date().toLocaleTimeString('zh-CN');"
"    camOn=d.camera;"
"    var camBtns=document.querySelectorAll('[id$=\"btn_cam\"],[id=\"set_btn_cam\"]');"
"    camBtns.forEach(function(b){b.textContent=camOn?'关闭摄像头':'开启摄像头';b.className=camOn?'btn-danger':'btn-pri'});"
"    if(currentPage=='monitor'){"
"      document.getElementById('sc_fps').textContent=d.fps||'--';"
"      document.getElementById('sc_total').textContent=d.total;"
"      document.getElementById('sc_bad').textContent=d.defects;"
"      document.getElementById('sc_thr').textContent=d.cls_thr?d.cls_thr[0].toFixed(2):d.threshold.toFixed(2);"
"      trendData.push({t:new Date().toLocaleTimeString('zh-CN',{hour:'2-digit',minute:'2-digit',second:'2-digit'}),v:d.defects});"
"      if(trendData.length>30) trendData.shift();"
"      drawLine(document.getElementById('chart_trend'),trendData,'#238bff');"
"      /* 面板巡检状态: 从/stats的panel字段直接获取 */"
"      var clsZN3=['遮挡','正常','遮挡','电气损伤','物理损伤','遮挡'];"
"      var clsC3=['#f9733c','#30c86a','#f9733c','#a855f7','#2dd4bf','#f9733c'];"
"      var pnl=d.panel||[];"
"      for(var i=0;i<3;i++){"
"        var el=document.getElementById('ps_'+i);"
"        var ti=document.getElementById('pt_'+i);"
"        if(pnl[i]&&pnl[i].cls>=0){"
"          var c=pnl[i].cls;"
"          el.innerHTML='<span style=\"color:'+clsC3[c]+';\">'+clsZN3[c]+'</span>';"
"          if(pnl[i].ts>0) ti.textContent=new Date(pnl[i].ts*1000).toLocaleTimeString('zh-CN');"
"        }"
"      }"
"    }"
"    if(currentPage=='settings') buildThrSliders();"
"    if(currentPage=='stats'&&tickCount%2==0) drawStats();"
"  });"
"  /* 检测卡片每次刷新拉取, 表格式记录页按需加载 */"
"  GET('/records',function(data){"
"    allRecords=data;"
"    if(currentPage=='monitor'){"
"      var dets=data.filter(function(r){return r.cls>=0});"
"      var cards=document.getElementById('det_cards');"
"      if(dets.length>0){"
"        var html='';"
"        for(var i=0;i<Math.min(dets.length,5);i++){"
"          var r=dets[i],cn=r.cls<6?CLS[r.cls]:'?';"
"          html+='<div class=\"det-card\"><div class=\"dc-dot\" style=\"background:'+CLS_COLORS[r.cls]+'\"></div>';"
"          html+='<span class=\"badge '+(BADGE[r.cls]||'badge-ok')+'\">'+cn+'</span>';"
"          html+='<div class=\"dc-conf\">'+(r.conf*100).toFixed(0)+'%</div>';"
"          html+='<div class=\"dc-box\">置信度 '+r.conf.toFixed(2)+'</div></div>';"
"        }"
"        cards.innerHTML=html;"
"        document.getElementById('det_ts').textContent=new Date(dets[0].ts*1000).toLocaleTimeString('zh-CN');"
"      }else{cards.innerHTML='<div class=\"empty-state\">当前帧无检出</div>'}"
"    }"
"    if(currentPage=='records') loadRecords();"
"  });"
"  tickCount++;"
"}"
""
"/* ====== RECORDS PAGE ====== */"
"function loadRecords(){"
"  /* 优先用缓存, 节省请求 */"
"  var data=allRecords;"
"  function render(data){"
"    var filterCls=parseInt(document.getElementById('rec_filter').value);"
"    var filtered=data.filter(function(r){return filterCls<0||r.cls==filterCls});"
"    document.getElementById('rec_info').textContent='共 '+filtered.length+' 条';"
"    var cnts=[0,0,0,0,0,0];"
"    filtered.forEach(function(r){if(r.cls>=0&&r.cls<6) cnts[r.cls]++});"
"    var statsHtml='<div class=\"stat-card\"><div class=\"sv\" style=\"color:var(--accent)\">'+filtered.length+'</div><div class=\"sl\">筛选结果</div></div>';"
"    for(var i=0;i<6;i++){statsHtml+='<div class=\"stat-card\"><div class=\"sv\" style=\"color:'+CLS_COLORS[i]+'\">'+cnts[i]+'</div><div class=\"sl\">'+CLS[i]+'</div></div>'}"
"    document.getElementById('rec_stats').innerHTML=statsHtml;"
"    var totalPages=Math.ceil(filtered.length/REC_PER_PAGE);"
"    if(recPage>=totalPages) recPage=Math.max(0,totalPages-1);"
"    var start=recPage*REC_PER_PAGE;"
"    var pageData=filtered.slice(start,start+REC_PER_PAGE);"
"    var tbody=document.getElementById('rec_tbody');"
"    if(pageData.length==0){tbody.innerHTML='<tr><td colspan=\"4\" style=\"text-align:center;padding:24px;color:var(--dim)\">暂无记录</td></tr>'}"
"    else{"
"      var html='';"
"      pageData.forEach(function(r){"
"        var ts=new Date(r.ts*1000);"
"        var t=ts.getHours().toString().padStart(2,'0')+':'+ts.getMinutes().toString().padStart(2,'0')+':'+ts.getSeconds().toString().padStart(2,'0');"
"        var cn=r.cls>=0&&r.cls<6?CLS[r.cls]:'无检出';"
"        var bd=r.cls>=0&&r.cls<6?BADGE[r.cls]:'badge-ok';"
"        html+='<tr><td>'+ts.toLocaleDateString('zh-CN')+' '+t+'</td>';"
"        html+='<td><span class=\"badge '+bd+'\">'+cn+'</span></td>';"
"        html+='<td>'+(r.conf*100).toFixed(0)+'%</td>';"
"        html+='<td style=\"color:'+(r.cls>=0?'var(--red)':'var(--green)')+'\">'+(r.cls>=0?'缺陷':'正常')+'</td></tr>'"
"      });"
"      tbody.innerHTML=html;"
"    }"
"    document.getElementById('rec_pager').innerHTML='<button class=\"btn-ghost\" onclick=\"recPage=0;loadRecords()\">首页</button>'+"
"      '<button class=\"btn-ghost\" onclick=\"if(recPage>0){recPage--;loadRecords()}\">上一页</button>'+"
"      '<span class=\"pg-info\">第 '+(recPage+1)+'/'+Math.max(1,totalPages)+' 页</span>'+"
"      '<button class=\"btn-ghost\" onclick=\"if(recPage<totalPages-1){recPage++;loadRecords()}\">下一页</button>'+"
"      '<button class=\"btn-ghost\" onclick=\"recPage='+Math.max(0,totalPages-1)+';loadRecords()\">末页</button>';"
"  }"
"  if(data&&data.length>0){render(data)}"
"  else{GET('/records',function(r){allRecords=r;render(r)})}"
"}"
""
"/* ====== SETTINGS PAGE ====== */"
"function buildThrSliders(){"
"  GET('/stats',function(d){"
"    var thr=d.cls_thr||[0.5,0.5,0.5,0.5,0.5,0.5];"
"    var html='';"
"    for(var i=0;i<6;i++){"
"      html+='<div class=\"slider-group\"><div class=\"sg-label\">'+"
"        '<span class=\"sg-name\" style=\"color:'+CLS_COLORS[i]+'\">&#9632; '+CLS[i]+'</span>'+"
"        '<span class=\"sg-val\" id=\"thrv_'+i+'\">'+thr[i].toFixed(2)+'</span></div>'+"
"        '<input type=\"range\" min=\"10\" max=\"95\" value=\"'+Math.round(thr[i]*100)+'\" step=\"5\" data-cls=\"'+i+'\" oninput=\"onThrInput(this)\"></div>';"
"    }"
"    document.getElementById('thr_sliders').innerHTML=html;"
"  });"
"}"
"function onThrInput(el){"
"  var cls=parseInt(el.dataset.cls);"
"  var v=el.value/100;"
"  document.getElementById('thrv_'+cls).textContent=v.toFixed(2);"
"  setThr(cls,v);"
"}"
""
"/* ====== INIT ====== */"
"setInterval(updateQuick,2000);"
"updateQuick();"
"</script>"
"</body>"
"</html>"
"";

/* ========== JSON 编码工具 ========== */
static void json_str(char *buf, int *pos, const char *s) {
    *pos += snprintf(buf+*pos, BUF_SZ-*pos, "\"%s\"", s);
}
static void json_int(char *buf, int *pos, int v) {
    *pos += snprintf(buf+*pos, BUF_SZ-*pos, "%d", v);
}
static void json_float(char *buf, int *pos, float v) {
    *pos += snprintf(buf+*pos, BUF_SZ-*pos, "%.2f", v);
}

/* ========== API处理器 ========== */

/* GET / → HTML */
static void handle_index(int fd) {
    int len = sizeof(INDEX_HTML)-1;
    http_ok(fd, "text/html; charset=utf-8", len);
    write(fd, INDEX_HTML, len);
}

/* GET /stream → MJPEG (使用预编码JPEG缓存, 零编码开销, 容忍慢客户端) */
static void handle_stream(int fd) {
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
        "HTTP/1.0 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=frame\r\n"
        "Cache-Control: no-cache\r\nConnection: close\r\n\r\n");
    if (write(fd, hdr, n) < 0) return;

    /* 非阻塞写: 检测客户端断连, 但容忍慢客户端 */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int last_id = -1;
    int err_count = 0;
    while (g_running && err_count < 10) {
        pthread_mutex_lock(&g_web_mutex);
        int ready = g_web_frame_ready;
        int fid   = g_web_jpeg_id;
        unsigned long jlen = g_web_jpeg_len;
        unsigned char *jpeg = NULL;
        if (ready && fid != last_id && jlen > 0 && g_web_jpeg) {
            jpeg = malloc(jlen);
            if (jpeg) memcpy(jpeg, g_web_jpeg, jlen);
            last_id = fid;
        }
        pthread_mutex_unlock(&g_web_mutex);

        if (jpeg) {
            char part[128];
            int pn = snprintf(part, sizeof(part),
                "--frame\r\nContent-Type: image/jpeg\r\n"
                "Content-Length: %lu\r\n\r\n", jlen);
            int wr;

            wr = write(fd, part, pn);
            if (wr < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { free(jpeg); break; }

            wr = write(fd, jpeg, jlen);
            if (wr < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { free(jpeg); break; }

            wr = write(fd, "\r\n", 2);
            if (wr < 0 && errno != EAGAIN && errno != EWOULDBLOCK) { free(jpeg); break; }

            free(jpeg);
            err_count = 0;
            usleep(50000);  /* ~20fps */
        } else {
            usleep(30000);  /* 等待新帧 */
            err_count++;
        }
    }
}

/* GET /records → JSON数组 */
static void handle_records(int fd) {
    pthread_mutex_lock(&g_web_mutex);
    char buf[BUF_SZ];
    int pos = 0;
    pos += snprintf(buf+pos, BUF_SZ-pos, "[");

    int total = g_web_rec_cnt;
    for (int i = 0; i < total; i++) {
        int idx = (g_web_rec_head - i - 1 + REC_MAX) % REC_MAX;
        web_rec_t *r = &g_web_recs[idx];
        if (i > 0) pos += snprintf(buf+pos, BUF_SZ-pos, ",");
        pos += snprintf(buf+pos, BUF_SZ-pos,
            "{\"ts\":%ld,\"cls\":%d,\"conf\":%.2f}", r->ts, r->cls, r->conf);
    }
    pos += snprintf(buf+pos, BUF_SZ-pos, "]");
    pthread_mutex_unlock(&g_web_mutex);

    http_ok(fd, "application/json; charset=utf-8", pos);
    write(fd, buf, pos);
}

/* GET /stats → JSON统计 (含阈值和摄像头状态) */
static void handle_stats(int fd) {
    pthread_mutex_lock(&g_web_mutex);
    int total = g_web_rec_cnt;
    int defects = 0;
    int per_cls[NUM_CLS];
    memset(per_cls, 0, sizeof(per_cls));
    for (int i = 0; i < total; i++) {
        int idx = (g_web_rec_head - i - 1 + REC_MAX) % REC_MAX;
        if (g_web_recs[idx].cls >= 0) {
            defects++;
            if (g_web_recs[idx].cls < NUM_CLS) per_cls[g_web_recs[idx].cls]++;
        }
    }
    int fps = g_web_fps;
    /* 读取面板状态（锁内） */
    int p_cls[3] = {g_panel_cls[0],g_panel_cls[1],g_panel_cls[2]};
    long p_ts[3] = {(long)g_panel_ts[0],(long)g_panel_ts[1],(long)g_panel_ts[2]};
    pthread_mutex_unlock(&g_web_mutex);

    char buf[832];
    int pos = snprintf(buf, sizeof(buf),
        "{\"fps\":%d,\"total\":%d,\"defects\":%d,\"camera\":%s,\"hist_eq\":%s,\"threshold\":%.2f,"
        "\"cls_thr\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
        "\"cls_cnt\":[%d,%d,%d,%d,%d,%d],"
        "\"panel\":[{\"cls\":%d,\"ts\":%ld},{\"cls\":%d,\"ts\":%ld},{\"cls\":%d,\"ts\":%ld}]}",
        fps, total, defects, g_ui.camera_on ? "true" : "false",
        g_hist_eq ? "true" : "false", g_conf_thr,
        g_cls_thr[0],g_cls_thr[1],g_cls_thr[2],g_cls_thr[3],g_cls_thr[4],g_cls_thr[5],
        per_cls[0],per_cls[1],per_cls[2],per_cls[3],per_cls[4],per_cls[5],
        p_cls[0],p_ts[0],p_cls[1],p_ts[1],p_cls[2],p_ts[2]);
    http_ok(fd, "application/json; charset=utf-8", pos);
    write(fd, buf, pos);
}

/* POST /threshold?value=X.XX 或 /threshold?cls=N&value=X.XX */
static void handle_threshold(int fd, const char *body) {
    int cls = -1;
    float v = 0;
    /* 尝试解析 cls= 和 value= */
    const char *cp = body;
    if (strstr(body, "cls=")) {
        sscanf(body, "cls=%d&value=%f", &cls, &v);
        /* 兼容 ?value=X.XX&cls=N */
        if (cls < 0) sscanf(body, "value=%f&cls=%d", &v, &cls);
    } else {
        sscanf(body, "value=%f", &v);
    }
    if (v >= 0.10f && v <= 0.95f) {
        if (cls >= 0 && cls < NUM_CLS) {
            g_cls_thr[cls] = v;
        } else {
            g_conf_thr = v;
            for (int i = 0; i < NUM_CLS; i++) g_cls_thr[i] = v;
        }
    }
    /* 返回当前所有阈值 */
    char buf[192];
    int pos = snprintf(buf, sizeof(buf),
        "{\"threshold\":%.2f,\"cls_thr\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f]}",
        g_conf_thr,
        g_cls_thr[0],g_cls_thr[1],g_cls_thr[2],
        g_cls_thr[3],g_cls_thr[4],g_cls_thr[5]);
    http_ok(fd, "application/json; charset=utf-8", pos);
    write(fd, buf, pos);
}

/* POST /servo → 舵机控制 (?cmd=reset|scan|stop 或 ?axis=0&angle=90) */
static void handle_servo(int fd, const char *body) {
    if (strstr(body, "cmd=reset")) {
        servo_reset(&g_servo);
    } else if (strstr(body, "cmd=scan")) {
        if (!g_servo.scan_mode) servo_scan_start(&g_servo);
    } else if (strstr(body, "cmd=stop")) {
        servo_scan_stop(&g_servo);
    } else {
        int axis = -1; float angle = 0;
        sscanf(body, "axis=%d&angle=%f", &axis, &angle);
        if ((axis == 0 || axis == 1) && angle >= 0 && angle <= 180)
            servo_set_angle(&g_servo, axis, angle);
    }
    char buf[128];
    int pos = snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"x\":%.0f,\"y\":%.0f,\"scan\":%s}",
        g_servo.angle[0], g_servo.angle[1],
        g_servo.scan_mode ? "true" : "false");
    http_ok(fd, "application/json", pos);
    write(fd, buf, pos);
}

/* POST /capture → 触发手动采集或切换摄像头 */
static void handle_capture(int fd, const char *body) {
    if (body && strstr(body, "cmd=toggle")) {
        g_ui.camera_on = !g_ui.camera_on;
    } else {
        g_web_cap_cmd = 1;
    }
    char buf[64];
    int pos = snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"camera\":%s}", g_ui.camera_on ? "true" : "false");
    http_ok(fd, "application/json", pos);
    write(fd, buf, pos);
}

/* POST /histeq → 切换直方图均衡化开关 */
static void handle_histeq(int fd) {
    g_hist_eq = !g_hist_eq;
    char buf[64];
    int pos = snprintf(buf, sizeof(buf),
        "{\"ok\":true,\"hist_eq\":%s}", g_hist_eq ? "true" : "false");
    http_ok(fd, "application/json", pos);
    write(fd, buf, pos);
}

/* ========== web_thread 主循环 ========== */
static void *web_thread(void *arg) {
    (void)arg;
    signal(SIGPIPE, SIG_IGN);  /* 防止MJPEG客户端断连时write()触发SIGPIPE杀进程 */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("web socket"); return NULL; }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(WEB_PORT);
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("web bind"); close(sock); return NULL;
    }
    listen(sock, WEB_BACKLOG);
    char local_ip[32] = "127.0.0.1";
    {   /* 动态获取本机IP */
        FILE *fp = popen("hostname -I 2>/dev/null | awk '{print $1}'", "r");
        if (fp) {
            if (fgets(local_ip, sizeof(local_ip), fp)) {
                local_ip[strcspn(local_ip,"\n")] = 0;
            }
            pclose(fp);
        }
    }
    printf("Web UI: http://%s:%d\n", local_ip, WEB_PORT);

    while (g_running) {
        struct timeval tv = {1, 0};
        fd_set fds;
        FD_ZERO(&fds); FD_SET(sock, &fds);
        if (select(sock+1, &fds, NULL, NULL, &tv) <= 0) continue;

        int cli = accept(sock, NULL, NULL);
        if (cli < 0) continue;

        /* 读HTTP请求头 */
        char req[4096];
        int rlen = 0;
        struct timeval rt = {2, 0};
        setsockopt(cli, SOL_SOCKET, SO_RCVTIMEO, &rt, sizeof(rt));
        rlen = recv(cli, req, sizeof(req)-1, 0);
        if (rlen <= 0) { close(cli); continue; }
        req[rlen] = 0;

        /* 解析方法+路径 */
        char method[16], path[256];
        method[0]=path[0]=0;
        sscanf(req, "%15s %255s", method, path);

        /* 路由：去掉查询串，支持 /stream?ts=... 这类请求 */
        char path_only[256];
        strncpy(path_only, path, sizeof(path_only)-1);
        path_only[sizeof(path_only)-1] = 0;
        char *q = strchr(path_only, '?');
        if (q) *q = 0;

        if (strcmp(method, "GET") == 0) {
            if (strcmp(path_only, "/") == 0 || strcmp(path_only, "/index.html") == 0)
                handle_index(cli);
            else if (strcmp(path_only, "/stream") == 0)
                handle_stream(cli);
            else if (strcmp(path_only, "/records") == 0)
                handle_records(cli);
            else if (strcmp(path_only, "/stats") == 0)
                handle_stats(cli);
            else if (strcmp(path_only, "/servo") == 0)
                handle_servo(cli, path);
            else
                http_404(cli);
        } else if (strcmp(method, "POST") == 0) {
            if (strcmp(path_only, "/capture") == 0) {
                /* 找body，同时也允许 query: /capture?cmd=toggle */
                char *body = strstr(req, "\r\n\r\n");
                const char *payload = body ? body+4 : NULL;
                if (!payload || !*payload) payload = path;
                handle_capture(cli, payload);
            } else if (strcmp(path_only, "/servo") == 0) {
                char *body = strstr(req, "\r\n\r\n");
                const char *payload = body ? body+4 : NULL;
                if (!payload || !*payload) payload = path;
                handle_servo(cli, payload);
            } else if (strcmp(path_only, "/threshold") == 0) {
                char *body = strstr(req, "\r\n\r\n");
                const char *payload = body ? body+4 : NULL;
                if (!payload || !*payload) payload = path;
                handle_threshold(cli, payload);
            } else if (strcmp(path_only, "/histeq") == 0) {
                handle_histeq(cli);
            } else
                http_404(cli);
        } else {
            http_404(cli);
        }
        close(cli);
    }
    close(sock);
    return NULL;
}

/* ========== 供solar_defect.c调用的记录函数 ========== */
static void web_add_record(int cls, float conf) {
    pthread_mutex_lock(&g_web_mutex);
    int idx = g_web_rec_head;
    g_web_recs[idx].ts = time(NULL);
    g_web_recs[idx].cls = cls;
    g_web_recs[idx].conf = conf;
    g_web_rec_head = (idx + 1) % REC_MAX;
    if (g_web_rec_cnt < REC_MAX) g_web_rec_cnt++;
    pthread_mutex_unlock(&g_web_mutex);
}

/* 推理线程调用: 将最新帧和检测结果发布给Web (预编码JPEG, 锁外编码) */
static void web_publish_frame(unsigned char *rgb, DetResult *result) {
    /* JPEG编码在锁外进行, 不阻塞MJPEG流和其他API */
    unsigned char *new_jpeg = NULL;
    unsigned long  new_len = 0;
    rgb_to_jpeg(rgb, 640, 640, 40, &new_jpeg, &new_len);

    pthread_mutex_lock(&g_web_mutex);
    memcpy(g_web_frame, rgb, 640*640*3);
    g_web_frame_ready = 1;
    g_web_frame_id++;
    if (result) memcpy(&g_web_result, result, sizeof(DetResult));

    /* 快速指针交换 */
    unsigned char *old_jpeg = g_web_jpeg;
    if (new_jpeg && new_len > 0) {
        g_web_jpeg     = new_jpeg;
        g_web_jpeg_len = new_len;
        g_web_jpeg_id  = g_web_frame_id;
    }
    pthread_mutex_unlock(&g_web_mutex);
    free(old_jpeg);
}

/* 供infer线程检查手动触发 */
static int web_capture_pending(void) {
    int v = __sync_fetch_and_and(&g_web_cap_cmd, 0);
    return v;
}

/* ========== 类名数组(供solar_defect.c引用) ========== */
static const char *g_web_cls_names[] = {
    "cover", "clean", "cover", "electrical", "physical", "cover"
};

#ifdef __cplusplus
}
#endif

#endif /* WEB_UI_H */
