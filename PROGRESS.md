# 光伏板缺陷检测系统 — 项目进度文档

## 版本 4.0 | 2026-06-12

---

## 一、项目概述

基于 RV1126B 嵌入式开发板，使用 C/C++ 实现端侧光伏板缺陷视觉检测系统。通过 USB 摄像头采集图像，利用 RKNN (YOLOv8n INT8) 模型识别 6 类缺陷，实时显示检测结果。

### 硬件环境

| 组件     | 参数                                                     |
| -------- | -------------------------------------------------------- |
| 核心板   | RV1126B，aarch64，Debian 12，Linux 6.1.141               |
| 摄像头   | icspring USB，/dev/video52，YUYV 640x480 30fps           |
| 显示屏   | MIPI DSI 1024x600，/dev/dri/card0，Connector=96，CRTC=73 |
| 舵机     | 二轴 PWM，未实现                                         |
| 串口     | COM7 PuTTY，粘贴超50字符丢字                             |
| U盘      | /run/media/sda1                                          |
| 开发板IP | 192.168.0.232                                            |

### 软件依赖

| 库           | 版本     | 路径                                  |
| ------------ | -------- | ------------------------------------- |
| librknnrt.so | v2.3.2   | /usr/lib/                             |
| librga.so    | v1.10.5  | /usr/lib/aarch64-linux-gnu/           |
| libdrm.so    | 系统自带 | /usr/lib/aarch64-linux-gnu/           |
| Qt5          | 系统包   | apt-get install qt5-qmake qtbase5-dev |

---

## 二、当前版本

### 主分支：solar_defect.c（C 单文件 + Web UI 版本）

**状态**：✅ 稳定运行，~20-28 FPS

- ~1000 行单文件 C 代码 + `web_ui.h` Web 前端
- 5 线程 Pipeline：采集 → 推理 → 显示 → 键盘 → Web 服务
- 手写 DRM 帧缓冲本地 UI + 嵌入式 Web 远程 UI（中文界面）
- 按类别置信度阈值 + 帧间一致性过滤
- 编译：`gcc solar_defect.c -o solar_defect -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg`
- 运行：`chvt 1 && ./solar_defect`

### 分支 B：solar_qt.cpp（Qt C++ 版本）

**状态**：❌ 已放弃 — 运行时稳定性问题未解决

---

## 三、已完成的 Bug 修复（C 版本）

| # | 问题                           | 根因                                     | 修复                                    |
| - | ------------------------------ | ---------------------------------------- | --------------------------------------- |
| 1 | `rknn_output` 数组大小不匹配 | 声明 `outputs[3]` 但只用1个，释放时传3 | 改为 `outputs[1]`，释放传1            |
| 2 | Ctrl+C 程序卡死                | `pthread_cond_wait` 阻塞无法唤醒       | 改为 `pthread_cond_timedwait` 1秒超时 |
| 3 | Ctrl+C 退出不完全              | sig_handler 只广播 not_empty             | 补全4路 condition 广播                  |
| 4 | timedwait 超时后 busy-wait     | `ts` 只在循环外设置一次                | 每次循环内刷新 `clock_gettime`        |
| 5 | 采集/推理竞态条件              | 推理线程使用队列内部缓冲区指针           | 推理线程独立 `local_rgb` 缓冲区       |
| 6 | 摄像头关闭后仍有检测记录       | 推理线程未检查 camera_on 状态            | 关闭时跳过推理，清空检测计数            |

---

## 四、已完成的优化（C 版本）

| 优化             | 方法                                                           | 效果                 |
| ---------------- | -------------------------------------------------------------- | -------------------- |
| 黑屏误检抑制     | 亮度均值检测 stride=32，<20 跳过推理                           | 遮挡时不再输出 clean |
| 置信度阈值可调   | `g_conf_thr` 变量，`+`/`-` 键 0.05 步进，`R` 重置      | 运行时可调           |
| 画面缩放优化     | 16.16 定点步长累加，替代每像素乘除                             | CPU 节省             |
| 串口节流         | printf 改为每30帧摘要                                          | 减少串口阻塞         |
| 队列深度         | QSIZE 3→4                                                     | 缓解级间背压         |
| 后缓冲渲染       | malloc 后缓冲 → 单次 memcpy 到 DRM                            | 消除画面频闪         |
| 日志去重         | 检测集签名比较，仅变化时写入                                   | UI 日志不再重复刷屏  |
| 检测框过滤       | 最小面积 400px²                                               | 减少微小误检         |
| 按类别置信度阈值 | `g_cls_thr[6]` 每类独立阈值，`+`/`-` 或 Web 滑块全局同步 | 可按类微调精度       |
| 帧间一致性过滤   | 连续 3 帧检出才确认（TEMPORAL_WINDOW=3, IoU=0.20）             | 消除瞬态单帧误检     |

---

## 五、UI 功能

### C 版本 UI（手写DRM）

```
┌ SOLAR Defect  FPS:20 THR:0.50 DET: 2            [ ON ] ┐ 36px 状态栏
│ S: Camera  +/-: Threshold  R: Reset  Q: Quit            │
├──────────────────────────────────────────────────────────┤
│                   Camera Feed  414×414                   │ 画面区
├──────────────────────────────────────────────────────────┤
│ DEFECT LOG [23]                               15:30:45  │ 150px 日志
│ 15:30:44   bird-drop          0.85  (红)                │
│ 15:30:38   --- clear ---             (灰)                │
└──────────────────────────────────────────────────────────┘
```

### 键盘操作

| 按键          | 功能                                       |
| ------------- | ------------------------------------------ |
| `S`         | 摄像头 ON/OFF                              |
| `+` / `-` | 全局置信度阈值 ±0.05（同步更新全部 6 类） |
| `R`         | 阈值重置为 0.50                            |
| `Q`         | 退出程序                                   |

### Web UI（中文界面）

- 纯 C HTTP 服务器（`web_ui.h`），端口 8080
- 嵌入式 HTML/CSS/JS 单文件前端，无外部依赖
- 统计看板：帧率 / 检测总数 / 缺陷数 / 置信阈值
- MJPEG 实时视频流（预编码 JPEG 缓存，零额外编码开销）
- 摄像头远程开关、置信阈值滑块调节、手动采集触发
- 检测日志：中文类别名（鸟粪/正常/灰尘/电气损伤/物理损伤/积雪）
- 手机浏览器适配（响应式布局）
- SIGPIPE 保护：MJPEG 客户端断连不导致进程崩溃
- 访问地址：`http://<开发板IP>:8080`

---

## 六、模型链路

```
best.pt (300epoch, mAP50=0.889)
  → best.onnx (11.7MB)
  → best_scaled.onnx (cls×100 放大)
  → best_scaled.rknn (INT8, 4.8MB) ← 当前使用
```

### cls×100 放大方案（关键）

RV1126B RKNN runtime v2.3.2 的 INT8 量化 Bug：sigmoid 小值(0~0.9)被量化压为0。

解决方案：

1. ONNX 模型添加 Slice+Mul 节点，cls 输出 ×100
2. 转换为 INT8 RKNN 模型（值变大，精度保留）
3. C 代码读取时 `/100.0f` 还原

| 尝试方案                | 结果                         |
| ----------------------- | ---------------------------- |
| INT8 原始               | cls 全为 0 ❌                |
| FP16 不量化             | runtime 强制 INT8，仍为 0 ❌ |
| 输出分离（3 output）    | buf 为 nil ❌                |
| **cls×100 放大** | ✅ 可正常读取                |

---

## 七、6 类检测目标

| ID | 类别                          | mAP50 | 颜色       |
| -- | ----------------------------- | ----- | ---------- |
| 0  | bird-drop（鸟粪）             | 0.814 | #FF4444 红 |
| 1  | clean（正常）                 | 0.919 | #44FF44 绿 |
| 2  | dusty（灰尘）                 | 0.902 | #FFFF44 黄 |
| 3  | electrical-damage（电气损伤） | 0.919 | #FF44FF 紫 |
| 4  | physical-damage（物理损伤）   | 0.920 | #44FFFF 青 |
| 5  | snow-covered（积雪）          | 0.862 | #FF8844 橙 |

---

## 八、Web UI 集成进度（新增）

### 当前状态

**Web UI 最小稳定版已跑通**，基于 `solar_defect.c` 单文件版本集成：

- 纯 C HTTP 服务器（新增 `web_thread`）
- 端口：`8080`
- 页面可访问：`http://<board-ip>:8080`
- `/stats` 正常
- `/records` 正常
- `/stream` 已恢复，前端手动点击 `Start MJPEG` 才建立连接
- 手机浏览器可访问（响应式布局）

### 新增文件

| 文件         | 说明                                                                |
| ------------ | ------------------------------------------------------------------- |
| `web_ui.h` | 纯 C HTTP 服务器、MJPEG 推流、JPEG 编码、JSON API、嵌入式 HTML 前端 |

### 已完成的 Web 相关工作

| 模块            | 状态 | 说明                                           |
| --------------- | ---- | ---------------------------------------------- |
| HTTP 服务       | ✅   | 监听 8080，纯 C socket 实现                    |
| `/` 页面      | ✅   | 单文件 HTML，内嵌 CSS/JS，无 CDN               |
| `/stats`      | ✅   | 返回 FPS / 总检测数 / 异常数 JSON              |
| `/records`    | ✅   | 返回历史检测记录 JSON                          |
| `/capture`    | ✅   | 支持 POST 手动触发；支持 `?cmd=toggle`       |
| `/stream`     | ✅   | MJPEG 已恢复，前端手动启动                     |
| JPEG 编码       | ✅   | 使用 `libjpeg`，修复了 BGR→RGB 通道顺序     |
| JPEG 预编码缓存 | ✅   | 推理线程每帧编码一次，所有 stream 连接复用缓存 |
| Stream 零编码   | ✅   | handle_stream 直接发送缓存 JPEG，不再实时编码  |
| 客户端断连检测  | ✅   | O_NONBLOCK + write() 返回值检查                |

### 重要修复（本次）

1. **RKNN ABI 对齐修复** ✅
2. **RKNN 输出尺寸动态保护** ✅
3. **路由查询串修复** ✅
4. **MJPEG 颜色修复** ✅
5. **Web UI 响应速度优化（2026-06-11）**
   - 问题：每个浏览器连接 `/stream` 都在服务端实时做 640×640 JPEG 编码，锁 mutex 阻塞其他 API
   - 修复：
     - 新增 JPEG 缓存变量 `g_web_jpeg` / `g_web_jpeg_len` / `g_web_jpeg_id`
     - `web_publish_frame()` 预编码 JPEG（质量 40），一次编码所有连接复用
     - `handle_stream()` 改为直接发送缓存，不再实时编码
     - 非阻塞写检测客户端断连
     - mutex 持有时间大幅缩短

### 当前访问方式

- 页面：`http://192.168.0.106:8080`（注意 WiFi IP 会变化）
- 如 IP 变化：`hostname -I`

### WiFi 连接

| 名称               | 密码     | 网段        | 备注                          |
| ------------------ | -------- | ----------- | ----------------------------- |
| 嫁给马嘉祺肘飞劳大 | 12345678 | 192.168.0.x | 路由器，稳定                  |
| OPPO A5爆炸机      | 11111111 | 10.64.38.x  | 手机热点，有 AP 隔离/NAT 延迟 |
| xu                 | ?        | 192.168.0.x | 备选                          |

**注意**：`p2p0` 走 WiFi Direct 模式会导致 Web UI 延迟高，优先让 `wlan0` 走 Station 模式连接：

```bash
nmcli dev wifi connect "嫁给马嘉祺肘飞劳大" password "12345678" ifname wlan0
```

### 编译命令（含 -ljpeg）

```bash
cd /root/solar_defect
gcc solar_defect.c -o solar_defect \
  -I/usr/include -I/usr/include/libdrm \
  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \
  -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg
```

---

## 九、待完成工作

| 优先级 | 任务               | 说明                                                |
| ------ | ------------------ | --------------------------------------------------- |
| 🔴 高  | 消除误检           | 搜集误检样本重训，或单独调高 dusty/clean 类别的阈值 |
| 🔴 高  | 实际光伏板场景测试 | 验证各类别检出率和误检率                            |
| 🔴 高  | 代码开源到 GitHub  | 赛道硬性要求                                        |
| 🟠 中  | 完善竞赛文档       | 设计文档、测试报告、演示视频                        |
| 🟠 中  | 长时间稳定性测试   | 24h 连续运行验证                                    |
| 🟠 中  | 检测记录持久化     | 存盘截图+日志                                       |
| 🟡 低  | PWM 舵机巡检控制   | /sys/class/pwm/ 路径实现                            |
| 🟢 低  | 画面颜色优化       | RGA 色彩参数调整                                    |

---

## 十、编译与部署

### C 版本

```bash
cd /root/solar_defect
gcc solar_defect.c -o solar_defect \
  -I/usr/include -I/usr/include/libdrm \
  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \
  -lrknnrt -lrga -ldrm -ldl -lm -lpthread

# 运行
chvt 1 && ./solar_defect
# 退出
chvt 7
```

### Qt 版本

```bash
cd /root/solar_defect
qmake solar_qt.pro && make
./solar_qt -platform offscreen
```

### U盘备份

```bash
cp -r /root/solar_defect /run/media/sda1/
cp /usr/include/rknn_api.h /run/media/sda1/
sync && umount /run/media/sda1
```

---

## 十、已踩坑记录

1. `rknn_context` 必须是 `uint64_t`，`int` 会导致 Segmentation fault
2. 串口粘贴 >50 字符丢字 → 用 `python3 << 'EOF'` 分段写文件
3. DRM 运行前必须 `chvt 1`，否则画面被桌面覆盖
4. icspring 摄像头 RGA 格式用 `RK_FORMAT_YVYU_422`，不是 YUYV
5. DRM 像素写入时 RGB 字节序需交换（R↔B）
6. numpy 必须 <2 版本，否则 torch 崩溃
7. 平台名必须是 `rv1126b`（带 b），不能用 `rv1126` 或 `rv1106`
