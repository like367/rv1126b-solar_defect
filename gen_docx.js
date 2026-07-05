const fs = require('fs');
const { Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
        Header, Footer, AlignmentType, HeadingLevel, BorderStyle, WidthType,
        ShadingType, PageBreak, PageNumber, LevelFormat } = require('docx');

// Color constants
const ACCENT = "225588";
const DARK = "111111";
const GRAY = "666666";
const WHITE = "FFFFFF";
const LIGHT_BG = "F5F7FA";
const CODE_BG = "F0F0F0";
const HEADER_BG = "225588";
const RED = "CC3333";
const WARN_BG = "FFF3CD";

// Common border
const thinBorder = { style: BorderStyle.SINGLE, size: 1, color: "CCCCCC" };
const borders = { top: thinBorder, bottom: thinBorder, left: thinBorder, right: thinBorder };
const noBorders = { top: {style:BorderStyle.NONE}, bottom:{style:BorderStyle.NONE}, left:{style:BorderStyle.NONE}, right:{style:BorderStyle.NONE} };

// Helpers
function heading1(text) {
    return new Paragraph({ heading: HeadingLevel.HEADING_1, spacing: { before: 300, after: 150 }, children: [new TextRun({ text, bold: true, size: 30, font: "Arial", color: ACCENT })] });
}
function heading2(text) {
    return new Paragraph({ heading: HeadingLevel.HEADING_2, spacing: { before: 200, after: 100 }, children: [new TextRun({ text, bold: true, size: 24, font: "Arial", color: ACCENT })] });
}
function bodyText(text) {
    return new Paragraph({ spacing: { after: 80, line: 276 }, children: [new TextRun({ text, size: 20, font: "Arial", color: DARK })] });
}
function dimText(text) {
    return new Paragraph({ spacing: { after: 60 }, children: [new TextRun({ text, size: 18, font: "Arial", color: GRAY, italics: true })] });
}
function warnText(text) {
    return new Paragraph({ spacing: { after: 80 }, shading: { fill: WARN_BG, type: ShadingType.CLEAR },
        children: [new TextRun({ text: `【注意】${text}`, size: 19, font: "Arial", color: RED, bold: true })] });
}
function codeBlock(lines) {
    const children = [];
    lines.forEach((line, i) => {
        children.push(new Paragraph({
            spacing: { after: 0, line: 240 },
            shading: i === 0 ? { fill: CODE_BG, type: ShadingType.CLEAR } : undefined,
            indent: { left: 160 },
            children: [new TextRun({ text: line, size: 16, font: "Courier New", color: DARK })]
        }));
    });
    children.push(new Paragraph({ spacing: { after: 100 }, children: [] })); // spacer
    return children;
}
function bulletList(items) {
    return items.map(item => new Paragraph({
        spacing: { after: 40 }, indent: { left: 480, hanging: 240 },
        children: [{ text: item, size: 19, font: "Arial", color: DARK }].map(t => new TextRun(t))
    }));
}

// Table helper
const TW = 9026; // A4 content width in DXA (11906 - 2880 margins)
function makeTable(headers, rows, colW) {
    if (!colW) colW = headers.map(() => Math.floor(TW / headers.length));
    const hCells = headers.map((h, i) => new TableCell({
        borders, width: { size: colW[i], type: WidthType.DXA },
        shading: { fill: HEADER_BG, type: ShadingType.CLEAR },
        margins: { top: 60, bottom: 60, left: 80, right: 80 },
        children: [new Paragraph({ children: [new TextRun({ text: h, size: 17, font: "Arial", color: WHITE, bold: true })] })]
    }));
    const dataRows = rows.map((row, ri) => new TableRow({
        children: row.map((cell, ci) => new TableCell({
            borders, width: { size: colW[ci], type: WidthType.DXA },
            shading: ri % 2 === 0 ? { fill: LIGHT_BG, type: ShadingType.CLEAR } : undefined,
            margins: { top: 40, bottom: 40, left: 80, right: 80 },
            children: [new Paragraph({ children: [new TextRun({ text: cell || '', size: 17, font: "Arial", color: DARK })] })]
        }))
    }));
    return new Table({ width: { size: TW, type: WidthType.DXA }, columnWidths: colW, rows: [new TableRow({ children: hCells }), ...dataRows] });
}

// ============ BUILD DOCUMENT ============
const doc = new Document({
    styles: {
        default: { document: { run: { font: "Arial", size: 20 } } },
        paragraphStyles: [
            { id: "Heading1", name: "Heading 1", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 30, bold: true, font: "Arial", color: ACCENT },
              paragraph: { spacing: { before: 300, after: 150 }, outlineLevel: 0 } },
            { id: "Heading2", name: "Heading 2", basedOn: "Normal", next: "Normal", quickFormat: true,
              run: { size: 24, bold: true, font: "Arial", color: ACCENT },
              paragraph: { spacing: { before: 200, after: 100 }, outlineLevel: 1 } },
        ]
    },
    numbering: {
        config: [
            { reference: "pits", levels: [{ level: 0, format: LevelFormat.DECIMAL, text: "%1.", alignment: AlignmentType.LEFT,
              style: { paragraph: { indent: { left: 480, hanging: 360 } } } }] },
        ]
    },
    sections: [{
        properties: {
            page: {
                size: { width: 11906, height: 16838 }, // A4
                margin: { top: 1440, right: 1440, bottom: 1440, left: 1440 }
            }
        },
        headers: {
            default: new Header({ children: [
                new Paragraph({ spacing: { after: 0 }, border: { bottom: { style: BorderStyle.SINGLE, size: 4, color: "CCCCCC", space: 4 } },
                    children: [new TextRun({ text: "光伏板缺陷检测系统 — 项目移植手册 v3.1", size: 14, font: "Arial", color: GRAY })] })
            ]})
        },
        footers: {
            default: new Footer({ children: [
                new Paragraph({ alignment: AlignmentType.CENTER, children: [
                    new TextRun({ text: "Page ", size: 14, font: "Arial", color: GRAY }),
                    new TextRun({ children: [PageNumber.CURRENT], size: 14, font: "Arial", color: GRAY })
                ]})
            ]})
        },
        children: [
            // ============ COVER PAGE ============
            new Paragraph({ spacing: { before: 4000 }, children: [] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 100 },
                children: [new TextRun({ text: "光伏板缺陷检测系统", size: 52, font: "Arial", bold: true, color: ACCENT })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 300 },
                children: [new TextRun({ text: "项目移植手册", size: 36, font: "Arial", color: DARK })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 120 },
                children: [new TextRun({ text: "版本 3.1 | 2026-05-07", size: 22, font: "Arial", color: GRAY })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 },
                children: [new TextRun({ text: "RV1126B 嵌入式平台 | YOLOv8n INT8 | C/C++", size: 20, font: "Arial", color: GRAY })] }),
            new Paragraph({ spacing: { after: 300 }, border: { bottom: { style: BorderStyle.SINGLE, size: 2, color: "CCCCCC", space: 1 } }, children: [] }),

            // Key metrics table
            new Paragraph({ spacing: { before: 200, after: 100 }, children: [] }),
            makeTable(
                ["项目", "参数", "项目", "参数"],
                [
                    ["帧率", "~20 FPS", "模型", "YOLOv8n INT8"],
                    ["类别", "6类缺陷", "分辨率", "640×640 → 1024×600"],
                    ["代码", "940行C / 640行C++", "线程", "4线程Pipeline"],
                    ["平台", "RV1126B / Debian 12", "NPU", "RKNN v2.3.2"],
                ],
                [2256, 2257, 2256, 2257]
            ),

            new Paragraph({ children: [new PageBreak()] }),

            // ============ 1. 项目概述 ============
            heading1("一、项目概述"),
            bodyText("基于 RV1126B 嵌入式开发板，使用 C 语言实现端侧光伏板缺陷视觉检测系统。通过 USB 摄像头采集光伏板图像，利用 RKNN (YOLOv8n INT8) 模型识别 6 类光伏板缺陷，实时显示检测结果。"),
            bodyText("数据流: V4L2 采集 (YUYV 640x480) → RGA 预处理 (YUYV→RGB888 640x640, 硬件加速) → RKNN 推理 (YOLOv8n INT8, 6 类) → 后处理 (NMS + 坐标还原) → DRM 显示 (1024x600, 实时画面 + 检测框)"),
            bodyText("帧率: ~20 FPS | 模型: best_scaled.rknn (cls×100 放大方案) | 线程: 采集 + 推理 + 显示 + 键盘输入"),

            // ============ 2. 硬件环境 ============
            heading1("二、硬件环境"),
            makeTable(
                ["#", "组件", "参数", "备注"],
                [
                    ["1", "核心板", "RV1126B", "内置NPU, aarch64, Debian 12, Linux 6.1.141"],
                    ["2", "摄像头", "icspring USB camera", "/dev/video52, YUYV 640x480 30fps, 无NV12"],
                    ["3", "显示屏", "MIPI DSI 1024×600", "/dev/dri/card0, Connector ID=96, CRTC ID=73"],
                    ["4", "串口", "COM7 PuTTY", "⚠ 粘贴超过50字符会丢字!"],
                    ["5", "U盘 (板子)", "/run/media/sda1", "拔前必须 sync && umount"],
                    ["6", "U盘 (虚拟机)", "/media/elf/新加卷/", "Ubuntu 虚拟机, 模型转换环境"],
                    ["7", "开发板 IP", "192.168.0.232", "SSH已启动但PC连接超时, 原因未明"],
                    ["8", "舵机", "二轴 PWM", "未实现, /sys/class/pwm/路径控制"],
                ],
                [400, 1700, 2800, 4126]
            ),

            // ============ 3. 软件依赖 ============
            heading1("三、软件依赖"),
            makeTable(
                ["#", "库/文件", "版本", "路径"],
                [
                    ["1", "librknnrt.so", "v2.3.2", "/usr/lib/"],
                    ["2", "librga.so", "v1.10.5", "/usr/lib/aarch64-linux-gnu/"],
                    ["3", "libdrm.so", "系统自带", "/usr/lib/aarch64-linux-gnu/"],
                    ["4", "rknn_api.h", "手动创建", "/usr/include/ (rknn_context=uint64_t!)"],
                    ["5", "im2d.h / RgaApi.h", "v1.10.5", "/usr/include/rga/"],
                    ["6", "xf86drm.h / xf86drmMode.h", "系统", "/usr/include/ / /usr/include/libdrm/"],
                    ["7", "gcc / cmake", "12.2 / 3.25", "直接在板子上编译"],
                    ["8", "Qt5 (可选)", "apt-get", "qtbase5-dev, 仅 solar_qt 需要"],
                ],
                [400, 1900, 1200, 5526]
            ),

            // ============ 4. 模型信息（最重要） ============
            heading1("四、模型信息（核心知识点）"),

            heading2("4.1 当前模型"),
            makeTable(
                ["属性", "值"],
                [
                    ["文件名", "best_scaled.rknn"],
                    ["大小", "~4.8 MB (INT8 量化)"],
                    ["板子路径", "/root/solar_defect/best_scaled.rknn"],
                    ["虚拟机路径", "/home/elf/Desktop/best_scaled.rknn"],
                    ["原始 PyTorch", "best.pt (300 epoch, mAP50=0.889)"],
                    ["ONNX 来源", "best_scaled.onnx (cls×100 放大)"],
                    ["目标平台", "rv1126b (必须带 'b'!)"],
                    ["量化方式", "do_quantification=True, optimization_level=0"],
                ],
                [2500, 6526]
            ),

            heading2("4.2 cls×100 放大方案（最重要！）"),
            warnText("RV1126B 的 RKNN runtime v2.3.2 存在 INT8 量化缺陷: sigmoid 后的小值 (0~0.9) 被量化精度损失压缩为 0，导致所有类别分数为 0，完全无法识别任何缺陷。"),
            bodyText("解决方案: 在 ONNX 模型中添加 Slice + Mul 节点，将 cls 输出放大 100 倍。INT8 量化后数值范围变大，精度得以保留。C 代码读取时除以 100 还原真实概率。"),

            heading2("4.3 方案演进历程"),
            makeTable(
                ["尝试方案", "问题", "结果"],
                [
                    ["INT8 量化 (原始)", "cls 分数全部被量化为 0", "❌ 完全无法识别"],
                    ["FP16 不量化", "板子 runtime 强制 INT8 输出, cls 仍为 0", "❌ 无效"],
                    ["3 路输出分离", "outputs[1][2].buf = nil, runtime 不支持", "❌ 错误"],
                    ["cls×100 放大", "INT8 量化后值变大, 精度保留", "✅ 成功, 当前方案"],
                ],
                [2500, 3500, 3026]
            ),

            heading2("4.4 ONNX 修改 Python 代码（在虚拟机运行）"),
            ...codeBlock([
                "import onnx, onnx.helper as helper, numpy as np",
                "model = onnx.load('/home/elf/Desktop/best.onnx')",
                "",
                "def make_const_i(name, val):",
                "    return helper.make_node('Constant', inputs=[], outputs=[name],",
                "        value=helper.make_tensor(name, TensorProto.INT64, [1], [val]))",
                "",
                "nodes = [",
                "    make_const_i('s0',0), make_const_i('s4',4),",
                "    make_const_i('s10',10), make_const_i('ax1',1),",
                "    helper.make_node('Constant', [], ['scale100'],",
                "        value=helper.make_tensor('scale100', TensorProto.FLOAT, [1], [100.0])),",
                "    helper.make_node('Slice', ['output0','s0','s4','ax1'], ['bbox_part']),",
                "    helper.make_node('Slice', ['output0','s4','s10','ax1'], ['cls_part']),",
                "    helper.make_node('Mul', ['cls_part','scale100'], ['cls_scaled']),",
                "    helper.make_node('Concat', ['bbox_part','cls_scaled'],",
                "                     ['output_scaled'], axis=1),",
                "]",
                "for n in nodes: model.graph.node.append(n)",
                "out = helper.make_tensor_value_info('output_scaled',",
                "    TensorProto.FLOAT, [1,10,8400])",
                "model.graph.output.extend([out])",
                "onnx.save(model, '/home/elf/Desktop/best_scaled.onnx')",
            ]),

            heading2("4.5 RKNN 转换命令（在虚拟机运行）"),
            warnText("必须 numpy < 2.0, 否则 torch 崩溃。先执行: pip3 install 'numpy<2'"),
            ...codeBlock([
                "from rknn.api import RKNN",
                "rknn = RKNN(verbose=False)",
                "rknn.config(mean_values=[[0,0,0]], std_values=[[255,255,255]],",
                "           target_platform='rv1126b', optimization_level=0)",
                "rknn.load_onnx('/home/elf/Desktop/best_scaled.onnx',",
                "               outputs=['output_scaled'])",
                "rknn.build(do_quantization=True,",
                "          dataset='/home/elf/Desktop/dataset.txt')",
                "rknn.export_rknn('/home/elf/Desktop/best_scaled.rknn')",
                "rknn.release()",
            ]),

            // ============ 5. 模型输出格式 ============
            heading1("五、模型输出格式与后处理"),
            makeTable(
                ["属性", "值", "说明"],
                [
                    ["输出 Shape", "[1, 10, 8400]", "NCHW 格式 (注意不是 NHWC!)"],
                    ["bbox 通道", "ch[0~3]", "cx, cy, bw, bh (像素坐标)"],
                    ["cls 通道", "ch[4~9]", "6 类分数, 已×100, 需 /100 还原"],
                    ["数据布局", "data[channel × 8400 + anchor]", "NCHW 索引"],
                    ["CONF_THR", "0.50", "运行时 +/- 键可调 (0.10~0.95)"],
                    ["IOU_THR", "0.45", "NMS 重叠阈值"],
                    ["BRIGHTNESS_MIN", "20.0", "亮度保护: 低于此值跳过推理"],
                    ["BOX_MIN_AREA", "400 px²", "过滤微小误检框"],
                ],
                [1800, 3000, 4226]
            ),

            heading2("C 代码后处理关键片段"),
            ...codeBlock([
                "float *bbox = (float*)outputs[0].buf;",
                "float *cls  = bbox + 4 * 8400;",
                "",
                "for (int a = 0; a < 8400; a++) {",
                "    float cx = bbox[0*8400 + a];  // 中心 x",
                "    float cy = bbox[1*8400 + a];  // 中心 y",
                "    float bw = bbox[2*8400 + a];  // 宽度",
                "    float bh = bbox[3*8400 + a];  // 高度",
                "",
                "    float best = -1; int best_cls = 0;",
                "    for (int c = 0; c < 6; c++) {",
                "        float s = cls[c*8400 + a] / 100.0f;  // 除以100还原!",
                "        if (s > best) { best = s; best_cls = c; }",
                "    }",
                "    if (best < g_conf_thr) continue;",
                "",
                "    float x1 = cx - bw/2, y1 = cy - bh/2;",
                "    float x2 = cx + bw/2, y2 = cy + bh/2;",
                "    // ... NMS 去重 ...",
                "}",
            ]),

            // ============ 6. 检测目标 ============
            heading1("六、6 类检测目标"),
            makeTable(
                ["ID", "类别", "英文名", "mAP50", "颜色"],
                [
                    ["0", "鸟粪", "bird-drop", "0.814", "红 #FF4444"],
                    ["1", "正常", "clean", "0.919", "绿 #44FF44"],
                    ["2", "灰尘", "dusty", "0.902", "黄 #FFFF44"],
                    ["3", "电气损伤", "electrical-damage", "0.919", "紫 #FF44FF"],
                    ["4", "物理损伤", "physical-damage", "0.920", "青 #44FFFF"],
                    ["5", "积雪", "snow-covered", "0.862", "橙 #FF8844"],
                ],
                [500, 1200, 2200, 1000, 4126]
            ),
            dimText("mAP50 来源于 best.pt 原始 PyTorch 模型。INT8 量化后精度可能下降, 待实景测试对比。"),

            // ============ 7. 代码分支 ============
            heading1("七、代码分支"),

            heading2("分支 A: solar_defect.c — C 语言版本 (✅ 稳定)"),
            ...bulletList([
                "940 行单文件 C 代码, 无外部依赖 (仅系统库)",
                "手写 DRM 帧缓冲 UI, 内嵌 8×16 位图字体 (91 个 ASCII 字符)",
                "4 线程 Pipeline: 采集线程 → capQueue → 推理线程 → dispQueue → 显示线程",
                "后缓冲渲染消除画面频闪, 亮度检测防黑屏误检",
                "运行时置信度阈值可调 (+/- 键步进 0.05)",
                "编译: gcc solar_defect.c -o solar_defect -I/usr/include -I/usr/include/libdrm -L/usr/lib -L/usr/lib/aarch64-linux-gnu -lrknnrt -lrga -ldrm -ldl -lm -lpthread",
                "运行: chvt 1 && ./solar_defect",
            ]),

            heading2("分支 B: solar_qt.cpp — Qt5 C++ 版本 (⚠️ 实验性)"),
            ...bulletList([
                "~640 行 C++ 代码 + 1 个 .pro 项目文件",
                "Qt5 Widgets 界面 (QMainWindow / QPushButton / QListWidget / QLabel)",
                "编译: qmake solar_qt.pro && make",
                "运行: ./solar_qt -platform offscreen",
                "当前状态: 编译通过, 运行时 setupUI() 处 Segmentation fault",
                "原因分析: connect(m_infer,...) 在 m_infer = new InferThread() 之前调用, 访问未初始化指针。修复已提交, 待测试。",
            ]),

            // ============ 8. 坑点记录 ============
            heading1("八、关键坑点记录（移植前务必逐条阅读！）"),
            makeTable(
                ["#", "问题", "原因", "解决方案"],
                [
                    ["1", "rknn_context 段错误", "定义为 int, 实为 uint64_t", "typedef uint64_t rknn_context"],
                    ["2", "串口粘贴丢字", "缓冲区限制 ~50 字符", "python3 << 'EOF' 分段写文件"],
                    ["3", "DRM 画面被覆盖", "图形桌面抢占 DRM", "必须先 chvt 1 切换终端"],
                    ["4", "摄像头 RGA 格式错", "YUYV ≠ YVYU", "icspring 用 RK_FORMAT_YVYU_422"],
                    ["5", "DRM 颜色 R/B 异常", "RGB 字节序与 ARGB 不同", "写像素: rgb[idx+2]<<16|rgb[idx]"],
                    ["6", "numpy 版本冲突", "numpy 2.x 不兼容 torch", "pip install 'numpy<2'"],
                    ["7", "平台名错误", "写成 rv1126 或 rv1106", "必须用 rv1126b (带 b)"],
                    ["8", ".pt 文件被解压", "系统自动识别为 zip", "改名 .pt.bin 再传 U盘"],
                    ["9", "FP16 cls 仍为 0", "runtime 强制 INT8", "只有 cls×100 方案可行"],
                    ["10", "Ctrl+C 卡死", "cond_wait 阻塞不响应信号", "改用 pthread_cond_timedwait"],
                    ["11", "采集/推理竞态", "队列缓冲区被覆盖", "推理线程分配 local_rgb"],
                    ["12", "timedwait 超时 busy-wait", "ts 只设置一次", "每次循环内刷新 clock_gettime"],
                ],
                [400, 1500, 2200, 4926]
            ),

            // ============ 9. 编译部署 ============
            heading1("九、编译与部署"),

            heading2("9.1 C 版本编译运行"),
            ...codeBlock([
                "cd /root/solar_defect",
                "gcc solar_defect.c -o solar_defect \\",
                "  -I/usr/include -I/usr/include/libdrm \\",
                "  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \\",
                "  -lrknnrt -lrga -ldrm -ldl -lm -lpthread",
                "chvt 1 && ./solar_defect",
            ]),

            heading2("9.2 Qt 版本编译运行"),
            ...codeBlock([
                "cd /root/solar_defect",
                "qmake solar_qt.pro && make",
                "./solar_qt -platform offscreen",
            ]),

            heading2("9.3 常用操作"),
            makeTable(
                ["操作", "命令"],
                [
                    ["强制退出程序", "killall solar_defect  或  Ctrl+C"],
                    ["恢复桌面显示", "chvt 7"],
                    ["查看 U盘内容", "ls /run/media/sda1/"],
                    ["安全拔 U盘", "sync && umount /run/media/sda1"],
                    ["备份代码到U盘", "cp -r /root/solar_defect /run/media/sda1/ && cp /usr/include/rknn_api.h /run/media/sda1/"],
                    ["串口分段写文件", "python3 << 'EOF'  ...  f.write(...)  ...  EOF"],
                ],
                [2500, 6526]
            ),

            heading2("9.4 重启后一键恢复"),
            ...codeBlock([
                "# 从U盘恢复文件",
                "cp /run/media/sda1/rknn_api.h /usr/include/",
                "cp -r /run/media/sda1/solar_defect /root/",
                "cp /run/media/sda1/best_scaled.rknn /root/solar_defect/",
                "",
                "# 编译运行",
                "cd /root/solar_defect",
                "gcc solar_defect.c -o solar_defect -I/usr/include \\",
                "  -I/usr/include/libdrm -L/usr/lib \\",
                "  -L/usr/lib/aarch64-linux-gnu \\",
                "  -lrknnrt -lrga -ldrm -ldl -lm -lpthread",
                "chvt 1 && ./solar_defect",
            ]),

            // ============ 10. 文件目录 ============
            heading1("十、文件目录结构"),

            heading2("10.1 开发板 /root/solar_defect/"),
            makeTable(
                ["文件", "说明", "状态"],
                [
                    ["solar_defect.c", "主程序 (多线程 Pipeline, 940行)", "✅ 稳定"],
                    ["solar_defect", "编译后可执行文件", "✅"],
                    ["solar_qt.cpp", "Qt5 版本 (640行)", "⚠️ 调试中"],
                    ["solar_qt.pro", "Qt 项目文件", "⚠️"],
                    ["rknn_api.h", "RKNN API 头文件 (手动创建)", "✅"],
                    ["best_scaled.rknn", "当前 INT8 模型 (cls×100)", "✅ 4.8MB"],
                    ["best.rknn", "原始 INT8 (cls=0, 已废弃)", "❌"],
                    ["best_fp16.rknn", "FP16 模型 (cls=0, 已废弃)", "❌"],
                    ["test_v4l2.c / test_rga.c", "V4L2 / RGA 测试程序", "✅"],
                    ["test_drm.c", "DRM 显示测试", "✅"],
                    ["test_rknn.c / test_rknn_infer.c", "RKNN 加载/推理测试", "✅"],
                    ["PROGRESS.md", "项目进度文档", "✅"],
                ],
                [2800, 4000, 2226]
            ),

            heading2("10.2 Ubuntu 虚拟机 /home/elf/Desktop/"),
            makeTable(
                ["文件", "说明"],
                [
                    ["best.pt", "原始 PyTorch 模型 (300 epoch, mAP50=0.889)"],
                    ["best.onnx", "标准 ONNX 导出 (11.7MB)"],
                    ["best_scaled.onnx", "cls×100 放大的 ONNX (用于 RKNN 转换)"],
                    ["best_scaled.rknn", "最终 RKNN 模型 (~7.7MB 虚拟机版)"],
                    ["dataset.txt", "INT8 量化校准图片列表 (10 张)"],
                    ["images/", "10 张校准用光伏板图片"],
                ],
                [3000, 6026]
            ),

            // ============ 11. UI ============
            heading1("十一、UI 操作说明"),

            heading2("11.1 屏幕布局 (1024×600)"),
            ...codeBlock([
                "┌ SOLAR Defect  FPS:20 THR:0.50 DET: 2            [ ON ] ┐ 36px",
                "│ S: Camera  +/-: Threshold  R: Reset  Q: Quit            │",
                "├──────────────────────────────────────────────────────────┤",
                "│                   Camera Feed  414×414                   │",
                "│                   带检测框叠加                            │",
                "├──────────────────────────────────────────────────────────┤",
                "│ DEFECT LOG [23]                               15:30:45   │ 150px",
                "│ 15:30:44   bird-drop          0.85  (红)                 │",
                "│ 15:30:38   --- clear ---             (灰)                 │",
                "└──────────────────────────────────────────────────────────┘",
            ]),

            heading2("11.2 键盘操作"),
            makeTable(
                ["按键", "功能", "说明"],
                [
                    ["S", "摄像头 ON/OFF", "关闭时跳过推理, 状态栏显示 OFF"],
                    ["+ / =", "阈值 +0.05", "上限 0.95"],
                    ["- / _", "阈值 -0.05", "下限 0.10"],
                    ["R", "阈值重置", "恢复为默认 0.50"],
                    ["Q", "退出程序", "安全退出并恢复终端"],
                    ["Ctrl+C", "信号退出", "触发 sig_handler 清理"],
                ],
                [1500, 2500, 5026]
            ),

            heading2("11.3 日志着色规则"),
            makeTable(
                ["检测类别", "颜色", "色值"],
                [
                    ["bird-drop (鸟粪)", "红色", "#EE4444"],
                    ["clean (正常)", "绿色", "#22CC66"],
                    ["dusty (灰尘)", "黄色", "#FFAA22"],
                    ["electrical-damage (电气损伤)", "紫色", "#FF44FF"],
                    ["physical-damage (物理损伤)", "青色", "#44FFFF"],
                    ["snow-covered (积雪)", "橙色", "#FF8844"],
                    ["WARN / ERR (系统警告)", "黄 / 红", "#FFAA22 / #EE4444"],
                    ["--- clear --- (目标消失)", "灰色", "#667788"],
                ],
                [2800, 1500, 4726]
            ),

            // ============ 12. 待完成 ============
            heading1("十二、待完成工作"),
            makeTable(
                ["优先级", "任务", "状态", "说明"],
                [
                    ["🔴 高", "Qt 版本调试", "进行中", "connect 顺序修复后待测试"],
                    ["🔴 高", "实际光伏板场景测试", "待部署", "验证各类别检出率和误检率"],
                    ["🟠 中", "FPS 提升到 27+", "待优化", "NPU 推理是主要瓶颈, 需模型侧优化"],
                    ["🟠 中", "PWM 舵机巡检控制", "未实现", "/sys/class/pwm/ 路径实现"],
                    ["🟡 低", "异常记录存盘", "未实现", "检测到缺陷时保存截图+时间戳+类别"],
                    ["🟡 低", "SSH 网络连接修复", "未解决", "PC→192.168.0.232 连接超时排查"],
                    ["🟢 低", "画面颜色优化", "待调整", "RGA 色彩参数调整, 画面偏灰"],
                ],
                [900, 2200, 900, 5026]
            ),

            // ============ END MARKER ============
            new Paragraph({ spacing: { before: 400 }, children: [] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 60 },
                children: [new TextRun({ text: "— 文档结束 —", size: 24, font: "Arial", color: ACCENT, bold: true })] }),
            new Paragraph({ alignment: AlignmentType.CENTER, spacing: { after: 40 },
                children: [new TextRun({ text: "光伏板缺陷检测系统 项目移植手册 v3.1", size: 18, font: "Arial", color: GRAY })] }),
            new Paragraph({ alignment: AlignmentType.CENTER,
                children: [new TextRun({ text: "2026-05-07 | RV1126B | YOLOv8n INT8 | /root/solar_defect/ | COM7 | 192.168.0.232", size: 16, font: "Arial", color: GRAY })] }),
        ]
    }]
});

// Write
Packer.toBuffer(doc).then(buffer => {
    fs.writeFileSync("光伏板检测系统_项目移植手册.docx", buffer);
    console.log("DOCX generated successfully");
});
