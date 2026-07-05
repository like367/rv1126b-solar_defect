# 基于 RV1126B 的端侧光伏板缺陷智能检测系统

瑞芯微赛道 · 选题二：端侧 AI 视觉应用

## 项目简介

本系统基于瑞芯微 RV1126B 嵌入式处理器（3TOPS@INT8 NPU），在端侧运行 YOLOv8n INT8 量化模型，实现对光伏板 6 类缺陷（鸟粪、灰尘、电气损伤、物理损伤、积雪、正常）的实时检测。系统采用 5 线程并行流水线架构，支持本地 DRM 帧缓冲显示和 Web 远程监控双模输出。

## 硬件需求

| 组件 | 规格 |
|------|------|
| 核心板 | ELF-RV1126B (RV1126B SoC) |
| 摄像头 | USB 摄像头，YUYV 640×480 |
| 显示屏 | 7寸 MIPI DSI 1024×600 |
| 舵机 | SG90 ×2（可选） |
| 系统 | Debian 12，Linux 6.1.141 |

## 快速开始

### 编译

```bash
# 在开发板上
cd /root/solar_defect
gcc solar_defect.c -o solar_defect \
  -I/usr/include -I/usr/include/libdrm \
  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \
  -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg
```

或使用 Makefile：

```bash
make
```

### 运行

```bash
chvt 1 && ./solar_defect
```

- `S` — 摄像头开关
- `+` / `-` — 全局置信度阈值
- `R` — 阈值重置
- `Q` — 退出

### Web 远程访问

开发板连接网络后，浏览器访问：

```
http://<开发板IP>:8080
```

支持手机和 PC 浏览器远程查看实时视频流、检测结果和统计面板。

## 系统架构

```
USB摄像头 → V4L2采集 → RGA预处理 → RKNN NPU推理 → 后处理过滤
                                          ↓
                                   DRM本地显示 + Web远程监控
```

## 模型部署流程

```
YOLOv8n 训练 (300epoch, mAP50=0.889)
  → ONNX 导出
  → RKNN INT8 量化
  → 端侧 NPU 推理 (20-28 FPS, 4.8MB)
```

## 核心特性

- 纯 C 全栈实现，无外部运行时依赖
- YOLOv8n INT8 量化，~20-28 FPS 实时推理
- 6 类光伏板缺陷检测 (mAP50=0.889)
- 按类别独立阈值 + 帧间一致性双过滤
- 反光/暗光场景端侧图像增强
- 嵌入式 Web 控制台 (MJPEG + REST API)
- 5 线程并行流水线架构
- 二轴舵机巡检控制 (可选)

## 项目文件说明

| 文件 | 说明 |
|------|------|
| `solar_defect.c` | 主程序，5线程流水线（采集/推理/显示/键盘/Web） |
| `web_ui.h` | 嵌入式 Web 服务：HTTP 服务器 + MJPEG 推流 + HTML 前端 |
| `servo.h` | 二轴舵机控制：软 PWM + 实时线程 |
| `ui.h` | DRM 帧缓冲 UI 渲染 |
| `rknn_api.h` | RKNN API 接口定义 |
| `best_scaled.rknn` | 预训练 INT8 量化模型 |
| `gen_ppt.js` | 竞赛演示 PPT 生成脚本 |
| `gen_docx.js` | 竞赛报告文档生成脚本 |
| `gen_pdf.js` | 项目移植手册生成脚本 |
| `竞赛报告全套材料.md` | 竞赛报告完整内容 |
| `Makefile` | 编译脚本 |

## 生成文档

```bash
npm install
node gen_ppt.js      # 生成竞赛PPT
node gen_docx.js     # 生成竞赛报告
node gen_pdf.js      # 生成移植手册
```

## 开源许可

本项目仅供学习参考。
