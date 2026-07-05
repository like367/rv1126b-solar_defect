const PDFDocument = require('pdfkit');
const fs = require('fs');

const doc = new PDFDocument({
    size: 'A4',
    margins: { top: 50, bottom: 50, left: 55, right: 55 },
    info: { Title: '光伏板缺陷检测系统 — 项目移植手册 v4.0' }
});

const out = fs.createWriteStream('项目移植手册.pdf');
doc.pipe(out);

// Register Chinese font
doc.registerFont('CN', 'C:/Windows/Fonts/simhei.ttf');
const F = 'CN';
const M = 'Helvetica'; // monospace for code

// Colors
const ACCENT = '#225588';
const DARK   = '#111111';
const GRAY   = '#555555';
const RED    = '#CC3333';

// Helpers
function h1(text) {
    doc.fontSize(20).font(F).fillColor(ACCENT).text(text, { paragraphGap: 2 });
    doc.moveTo(55, doc.y + 2).lineTo(540, doc.y + 2).strokeColor(ACCENT).stroke();
    doc.moveDown(0.6);
}
function h2(text) {
    doc.moveDown(0.5);
    doc.fontSize(14).font(F).fillColor(ACCENT).text(text, { paragraphGap: 2 });
    doc.moveDown(0.3);
}
function h3(text) {
    doc.moveDown(0.3);
    doc.fontSize(11).font(F).fillColor(DARK).text(text, { paragraphGap: 1 });
    doc.moveDown(0.2);
}
function body(text) {
    doc.fontSize(9.5).font(F).fillColor(DARK).text(text, { lineGap: 2, paragraphGap: 3 });
}
function dim(text) {
    doc.fontSize(8.5).font(F).fillColor(GRAY).text(text, { lineGap: 1 });
}
function code(text) {
    const y0 = doc.y + 3;
    doc.rect(55, y0, 485, 0).fill('#F4F4F4');
    doc.fontSize(7.8).font(M).fillColor('#333333');
    const lines = text.split('\n');
    for (const line of lines) {
        doc.text(line, 60, doc.y + 1, { lineGap: 1, paragraphGap: 0 });
    }
    doc.moveDown(0.5);
}
function bullet(items) {
    doc.fontSize(9.5).font(F).fillColor(DARK);
    for (const item of items) {
        doc.text('• ' + item, 65, doc.y, { lineGap: 1.5, paragraphGap: 1, indent: 10 });
    }
    doc.moveDown(0.3);
}
function table(headers, rows, colW) {
    if (!colW) {
        // Auto-calculate equal column widths
        const totalW = 485;
        colW = headers.map(() => totalW / headers.length);
    }
    const x0 = 55;
    let y0 = doc.y + 4;

    // Header
    doc.rect(x0, y0, 485, 18).fill(ACCENT);
    doc.fontSize(8.5).font(F).fillColor('#FFFFFF');
    let x = x0 + 4;
    for (let i = 0; i < headers.length; i++) {
        doc.text(headers[i] || '', x + 2, y0 + 3, { width: colW[i] - 4, lineGap: 0 });
        x += colW[i];
    }
    y0 += 18;

    // Rows
    for (let r = 0; r < rows.length; r++) {
        const bg = r % 2 === 0 ? '#F8F8F8' : '#FFFFFF';
        doc.rect(x0, y0, 485, 16).fill(bg);
        doc.fontSize(8).font(F).fillColor(DARK);
        let rx = x0 + 4;
        const row = rows[r];
        for (let c = 0; c < headers.length; c++) {
            doc.text((row[c] !== undefined ? row[c] : ''), rx + 2, y0 + 3, { width: colW[c] - 4, lineGap: 0 });
            rx += colW[c];
        }
        y0 += 16;
    }
    doc.y = y0 + 6;
}
function warn(text) {
    doc.fontSize(9).font(F).fillColor(RED).text('⚠ ' + text, { lineGap: 2, paragraphGap: 2 });
}

// ============ DOCUMENT ============

// Cover / Title
doc.moveDown(3);
doc.fontSize(26).font(F).fillColor(ACCENT).text('光伏板缺陷检测系统', { align: 'center' });
doc.moveDown(0.3);
doc.fontSize(18).font(F).fillColor(DARK).text('项目移植手册', { align: 'center' });
doc.moveDown(0.5);
doc.fontSize(11).font(F).fillColor(GRAY).text('版本 4.0 | 2026-06-12', { align: 'center' });
doc.moveDown(0.3);
doc.fontSize(9).font(F).fillColor(GRAY).text('RV1126B 嵌入式平台 | YOLOv8n INT8 | 纯C + Web UI', { align: 'center' });
doc.moveDown(1);
doc.rect(100, doc.y, 395, 0.5).fill('#DDDDDD');
doc.moveDown(1);

// Key metrics
const metrics = [
    ['帧率', '~20-28 FPS', '模型', 'YOLOv8n INT8'],
    ['类别', '6类缺陷', '分辨率', '640×640→1024×600'],
    ['代码', '~1000行C + web_ui.h', '线程', '5线程Pipeline'],
    ['UI', 'DRM本地 + Web远程', '端口', '8080'],
];
let mx = 100, my = doc.y;
doc.fontSize(9).font(F);
for (const row of metrics) {
    let rx = mx;
    for (let c = 0; c < 4; c++) {
        if (c % 2 === 0) { doc.fillColor(GRAY); doc.text(row[c], rx, my); }
        else { doc.fillColor(DARK); doc.text(row[c], rx + 70, my); }
        rx += 140;
    }
    my += 15;
}
doc.y = my + 20;

// ======== PAGE BREAK for TOC-like overview ========
doc.addPage();

// Section 1
h1('一、项目概述');
body('基于 RV1126B 嵌入式开发板，使用 C 语言实现端侧光伏板缺陷视觉检测系统。通过 USB 摄像头采集光伏板图像，利用 RKNN（YOLOv8n INT8）模型识别 6 类缺陷，实时显示检测结果。');
doc.moveDown(0.3);
body('数据流: V4L2 采集(YUYV 640x480) → RGA 预处理(YUYV→RGB888 640x640, 硬件加速) → RKNN 推理(YOLOv8n INT8) → 后处理(NMS+按类别阈值+帧间一致性过滤) → DRM 本地显示(1024x600) + Web 远程推流(HTTP MJPEG)');
doc.moveDown(0.3);
body('Web UI 功能: 纯C HTTP 服务器(端口8080)、嵌入式 HTML/CSS/JS 工业控制台(4模块: 实时监控/检测记录/统计分析/系统设置)、Canvas 手绘图表(饼图/柱状图/折线图/仪表盘)、MJPEG 视频流(JPEG 预编码缓存)、JSON REST API(/stats /records /stream /threshold /capture)、登录认证');
doc.moveDown(0.3);
doc.moveDown(0.3);

h2('1.1 运行环境');
table(
    ['序号', '组件', '参数', '备注'],
    [
        ['1', '核心板', 'RV1126B', '内置NPU, aarch64'],
        ['2', '操作系统', 'Debian 12', 'Linux 6.1.141'],
        ['3', '摄像头', 'icspring USB', '/dev/video52, YUYV 640x480 30fps'],
        ['4', '显示屏', 'MIPI DSI 1024x600', '/dev/dri/card0, Conn=96, CRTC=73'],
        ['5', '串口', 'COM7 PuTTY', '粘贴>50字符会丢字!'],
        ['6', 'U盘(板子)', '/run/media/sda1', '拔前须 sync && umount'],
        ['7', 'WiFi', 'nmcli管理', '支持STA/P2P模式, p2p0优先'],
        ['8', 'Web UI端口', '8080', '上位机浏览器访问 http://IP:8080'],
        ['9', '舵机', '二轴 PWM', '未实现, /sys/class/pwm/'],
    ]
);

// Section 2
h1('二、软件依赖');
table(
    ['序号', '库/文件', '版本', '路径'],
    [
        ['1', 'librknnrt.so', 'v2.3.2', '/usr/lib/'],
        ['2', 'librga.so', 'v1.10.5', '/usr/lib/aarch64-linux-gnu/'],
        ['3', 'libdrm.so', '系统', '/usr/lib/aarch64-linux-gnu/'],
        ['4', 'rknn_api.h', '板子ABI对齐', 'uint64_t ctx, 正确结构体布局'],
        ['5', 'libjpeg.so', '系统', 'JPEG编码(MJPEG推流)'],
        ['6', 'web_ui.h', '~750行', '纯C Web服务+嵌入式前端'],
        ['7', 'im2d.h', '—', '/usr/include/rga/'],
        ['8', 'xf86drm.h', '—', '/usr/include/'],
        ['9', 'gcc', '12.2', '直接在板子上编译'],
        ['10', 'libm/libpthread', '系统', '数学库+线程库'],
    ]
);

// Section 3
h1('三、模型信息（关键！）');

h2('3.1 当前模型');
table(
    ['属性', '值', '属性', '值'],
    [
        ['文件名', 'best_scaled.rknn', '大小', '~4.8MB (INT8)'],
        ['板子路径', '/root/solar_defect/best_scaled.rknn', '平台', 'rv1126b (必须带b!)'],
        ['虚拟机路径', '/home/elf/Desktop/best_scaled.rknn', '量化', 'do_quantization=True'],
        ['ONNX来源', 'best_scaled.onnx (cls×100)', '原始PT', 'best.pt (300epoch, mAP50=0.889)'],
    ]
);

doc.moveDown(0.3);
h2('3.2 cls×100 放大方案（最重要！）');
warn('RV1126B 的 RKNN runtime v2.3.2 存在 INT8 量化 Bug: sigmoid 小值 (0~0.9) 被量化精度损失压成 0，导致所有类别分数为 0，完全无法识别。');
doc.moveDown(0.2);
body('解决方案: 在 ONNX 模型中添加 Slice+Mul 节点，将 cls 输出放大 100 倍。转换后的 INT8 RKNN 模型值变大、精度保留。C 代码读取时除以 100 还原。');
doc.moveDown(0.2);

h3('方案演进');
table(
    ['方案', '问题', '结果'],
    [
        ['INT8 原始', 'cls 分数全部量化成 0', '❌ 完全无法识别'],
        ['FP16 不量化', '板子 runtime 强制 INT8 输出, cls 仍为 0', '❌ 无效'],
        ['输出分离 (3 output)', 'outputs[1][2] 的 buf 为 nil', '❌ runtime 不支持'],
        ['cls×100 放大', 'INT8 量化后值变大, 精度保留', '✅ 可读取, 当前方案'],
    ]
);

doc.moveDown(0.3);
h2('3.3 ONNX 修改 Python 代码');
code(`import onnx, onnx.helper as helper, numpy as np
model = onnx.load('best.onnx')
def make_const_i(name, val):
    return helper.make_node('Constant', inputs=[], outputs=[name],
        value=helper.make_tensor(name, onnx.TensorProto.INT64, [1], [val]))
nodes = [
    make_const_i('s0',0), make_const_i('s4',4),
    make_const_i('s10',10), make_const_i('ax1',1),
    helper.make_node('Constant', inputs=[], outputs=['scale100'],
        value=helper.make_tensor('scale100', onnx.TensorProto.FLOAT, [1], [100.0])),
    helper.make_node('Slice', ['output0','s0','s4','ax1'], ['bbox_part']),
    helper.make_node('Slice', ['output0','s4','s10','ax1'], ['cls_part']),
    helper.make_node('Mul', ['cls_part','scale100'], ['cls_scaled']),
    helper.make_node('Concat', ['bbox_part','cls_scaled'], ['output_scaled'], axis=1),
]
for n in nodes: model.graph.node.append(n)
onnx.save(model, 'best_scaled.onnx')`);

h2('3.4 RKNN 转换命令（在虚拟机 Ubuntu 上运行）');
warn('注意: numpy 必须 < 2.0，否则 torch 崩溃。pip3 install \'numpy<2\'');
code(`from rknn.api import RKNN
rknn = RKNN(verbose=False)
rknn.config(mean_values=[[0,0,0]], std_values=[[255,255,255]],
           target_platform='rv1126b', optimization_level=0)
rknn.load_onnx('best_scaled.onnx', outputs=['output_scaled'])
rknn.build(do_quantization=True, dataset='dataset.txt')
rknn.export_rknn('best_scaled.rknn')
rknn.release()`);

// Section 4
doc.addPage();
h1('四、模型输出格式与后处理');

h2('4.1 输出 Tensor 格式');
table(
    ['属性', '值', '说明'],
    [
        ['输出 shape', '[1, 10, 8400]', 'NCHW 格式 (不是 NHWC!)'],
        ['bbox 通道', 'ch[0~3]', 'cx, cy, bw, bh (像素坐标)'],
        ['cls 通道', 'ch[4~9]', '6个类别分数, 已×100, 需/100还原'],
        ['数据布局', 'data[channel * 8400 + anchor]', 'NCHW 索引方式'],
        ['anchor 数', '8400', 'YOLOv8n 默认'],
    ]
);

doc.moveDown(0.3);
h2('4.2 C 代码后处理关键片段');
code(`float *bbox = (float*)outputs[0].buf;
float *cls  = bbox + 4*8400;

for(int a = 0; a < 8400; a++) {
    float cx = bbox[0*8400 + a], cy = bbox[1*8400 + a];
    float bw = bbox[2*8400 + a], bh = bbox[3*8400 + a];

    // 6个类别中找最高分
    float best = -1; int best_cls = 0;
    for(int c = 0; c < 6; c++) {
        float s = cls[c*8400 + a] / 100.0f;  // ← 除以100还原!
        if(s > best) { best = s; best_cls = c; }
    }
    if(best < g_cls_thr[best_cls]) continue;    // 按类别阈值过滤

    float x1 = cx - bw/2, y1 = cy - bh/2;
    float x2 = cx + bw/2, y2 = cy + bh/2;
    // ... NMS ...
}`);

h2('4.3 可调参数');
table(
    ['参数', '默认值', '说明'],
    [
        ['CONF_THR (全局)', '0.50', '全局置信度阈值, 键盘+/-或Web滑块调整'],
        ['g_cls_thr[6]', '0.55/0.50/0.60/0.50/0.50/0.50', '按类别独立阈值: 鸟粪/正常/灰尘/电气/物理/积雪'],
        ['IOU_THR', '0.45', 'NMS 重叠阈值'],
        ['TEMPORAL_WINDOW', '3', '帧间一致性: 连续N帧检出才确认, 消除瞬态误检'],
        ['TEMPORAL_IOU', '0.20', '帧间匹配IoU阈值'],
        ['BRIGHTNESS_MIN', '20.0', '画面亮度下限, 低于此值跳过推理(防黑屏误检)'],
        ['BOX_MIN_AREA', '400', '最小检测框面积(px²)'],
    ]
);

// Section 5
doc.addPage();
h1('五、6 类检测目标');
table(
    ['ID', '类别', '英文名', 'mAP50', '颜色', '色值'],
    [
        ['0', '鸟粪', 'bird-drop', '0.814', '红色', '#FF4444'],
        ['1', '正常', 'clean', '0.919', '绿色', '#44FF44'],
        ['2', '灰尘', 'dusty', '0.902', '黄色', '#FFFF44'],
        ['3', '电气损伤', 'electrical-damage', '0.919', '紫色', '#FF44FF'],
        ['4', '物理损伤', 'physical-damage', '0.920', '青色', '#44FFFF'],
        ['5', '积雪', 'snow-covered', '0.862', '橙色', '#FF8844'],
    ]
);
doc.moveDown(0.3);
dim('mAP50 数据来源于 best.pt 原始 PyTorch 模型 (300 epoch)。实际 INT8 量化后精度可能有所下降，待实测对比。');

// Section 6
h1('六、代码架构');
h2('主分支: solar_defect.c + web_ui.h — 纯C + Web UI (✅ 稳定)');
bullet([
    '~1000 行单文件 C 代码 + ~750 行 web_ui.h Web 前端',
    '5 线程 Pipeline: 采集 → 推理 → 显示 → 键盘 → Web 服务',
    'DRM 帧缓冲本地 UI (手写 8×16 位图字体, 后缓冲防频闪)',
    'Web 工业控制台 (嵌入式 HTML/CSS/JS, 4 模块, Canvas 图表)',
    '按类别置信度阈值 + 帧间一致性过滤 (TEMPORAL_WINDOW=3)',
    'MJPEG 视频流 (JPEG 预编码缓存, 锁外编码, SIGPIPE 保护)',
    '登录认证 (admin/admin, sessionStorage 会话保持)',
    '编译: gcc solar_defect.c -o solar_defect -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg',
    '运行: chvt 1 && ./solar_defect',
]);

doc.moveDown(0.2);
h2('分支 B: solar_qt.cpp — Qt5 C++ 版本 (❌ 已放弃)');
bullet([
    '因运行时稳定性问题 (多个 Segfault, Qt offscreen 平台兼容性) 放弃开发',
    '主分支已完全覆盖 Qt 版本的 UI 需求 (通过 DRM 本地 + Web 远程双模)',
]);

// Section 7
h1('七、关键坑点记录（移植必读）');
table(
    ['#', '问题', '原因', '解决方案'],
    [
        ['1', 'rknn_context 段错误', '定义为 int 但实为 uint64_t', 'typedef uint64_t rknn_context'],
        ['2', '串口粘贴丢字', '缓冲区限制约50字符', '用 python3 << \'EOF\' 分段写文件'],
        ['3', 'DRM 画面被覆盖', '图形桌面抢占 DRM', '必须先执行 chvt 1 切换终端'],
        ['4', '摄像头 RGA 格式错', 'YUYV 和 YVYU 不同', 'icspring 摄像头用 RK_FORMAT_YVYU_422'],
        ['5', 'DRM 颜色 R/B 异常', 'RGB 字节序与 ARGB 不同', '写像素时交换 R 和 B'],
        ['6', 'numpy 版本冲突', 'numpy 2.x 不兼容 torch', 'pip install \'numpy<2\''],
        ['7', '平台名错误', '写成了 rv1126 或 rv1106', '必须是 rv1126b (带 b)'],
        ['8', '.pt 文件被解压', '系统自动识别为 zip', '改名 .pt.bin 再传输到 U盘'],
        ['9', 'MJPEG客户端断连杀进程', 'write()触发SIGPIPE', 'signal(SIGPIPE, SIG_IGN) 保护'],
        ['10', 'rknn_output ABI不匹配', '手写结构体字段顺序/类型错误', '对照板子 /usr/include/rknn_api.h 修正'],
        ['11', 'Ctrl+C 卡死', '线程阻塞在 cond_wait', '必须用 pthread_cond_timedwait'],
        ['12', '采集/推理竞态', '队列缓冲区被覆盖', '推理线程必须有独立 local_rgb'],
    ]
);

// Section 8
doc.addPage();
h1('八、编译与部署');

h2('8.1 编译运行');
code(`cd /root/solar_defect
gcc solar_defect.c -o solar_defect \\
  -I/usr/include -I/usr/include/libdrm \\
  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \\
  -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg
chvt 1 && ./solar_defect`);

h2('8.2 常用操作');
table(
    ['操作', '命令'],
    [
        ['强制退出', 'killall solar_defect 或按 Q'],
        ['恢复桌面', 'chvt 7'],
        ['查 WiFi IP', 'hostname -I'],
        ['连 WiFi', 'nmcli dev wifi connect "SSID" password "PWD" ifname wlan0'],
        ['安全拔 U盘', 'sync && umount /run/media/sda1'],
        ['备份到 U盘', 'cp -r /root/solar_defect /run/media/sda1/'],
        ['串口写文件', 'python3 << \'EOF\' ... f.write(...) ... EOF'],
    ]
);

h2('8.3 重启后恢复流程');
code(`# 从 U盘恢复文件
cp /run/media/sda1/rknn_api.h /usr/include/
cp -r /run/media/sda1/solar_defect /root/
cp /run/media/sda1/best_scaled.rknn /root/solar_defect/

# 编译运行 (注意: 必须加 -ljpeg!)
cd /root/solar_defect
gcc solar_defect.c -o solar_defect \\
  -I/usr/include -I/usr/include/libdrm \\
  -L/usr/lib -L/usr/lib/aarch64-linux-gnu \\
  -lrknnrt -lrga -ldrm -ldl -lm -lpthread -ljpeg
chvt 1 && ./solar_defect`);

// Section 9
h1('九、文件目录结构');

h2('9.1 开发板 /root/solar_defect/');
table(
    ['文件', '说明', '状态'],
    [
        ['solar_defect.c', '主程序 (5线程Pipeline, ~1000行)', '✅ 稳定'],
        ['web_ui.h', 'Web服务+嵌入式前端 (~750行)', '✅ 稳定'],
        ['rknn_api.h', 'RKNN API头文件 (板子ABI对齐)', '✅ 稳定'],
        ['solar_defect', '编译后可执行文件', '✅'],
        ['best_scaled.rknn', '当前 INT8 模型 (cls×100, 6类)', '✅'],
        ['PROGRESS.md', '项目进度文档', '✅'],
    ]
);

h2('9.2 Ubuntu 虚拟机 /home/elf/Desktop/');
table(
    ['文件', '说明'],
    [
        ['best.pt', '原始 PyTorch 模型 (300 epoch, mAP=0.889)'],
        ['best.onnx', '标准 ONNX (11.7MB)'],
        ['best_scaled.onnx', 'cls×100 放大的 ONNX (用于转换)'],
        ['best_scaled.rknn', '最终 RKNN 模型'],
        ['dataset.txt', '量化校准图片列表'],
        ['images/', '校准图片'],
    ]
);

// Section 10
doc.addPage();
h1('十、UI 操作说明');

h2('10.1 本地 DRM 屏幕 (1024×600)');
code(`┌ SOLAR Defect  FPS:20 THR:0.50 DET: 2            [ ON ] ┐ 36px
│ S: Camera  +/-: Threshold  R: Reset  Q: Quit            │
├──────────────────────────────────────────────────────────┤
│                   Camera Feed  (414×414)                 │
│                     带检测框叠加                          │
├──────────────────────────────────────────────────────────┤
│ DEFECT LOG [23]                               15:30:45   │ 150px
│ 15:30:44  bird-drop               0.85 (红)             │
│ 15:30:38  --- clear ---                 (灰)             │
└──────────────────────────────────────────────────────────┘`);

h2('10.2 键盘操作');
table(
    ['按键', '功能', '说明'],
    [
        ['S', '摄像头 ON/OFF', '关闭时跳过推理, 显示 OFF 占位'],
        ['+ / =', '阈值 +0.05', '上限 0.95, 同步更新全部6类'],
        ['- / _', '阈值 -0.05', '下限 0.10, 同步更新全部6类'],
        ['R', '阈值重置', '恢复推荐默认值 (鸟粪0.55/灰尘0.60/其余0.50)'],
        ['Q', '退出程序', '安全退出并恢复终端'],
    ]
);

h2('10.3 Web 远程控制台 (http://IP:8080)');
bullet([
    '登录认证: 默认用户名 admin, 密码 admin',
    '实时监控页: 左侧大画幅 MJPEG 视频流 + 右侧检测结果卡片 + 近5分钟趋势折线图',
    '检测记录页: 可翻页表格 + 类别筛选 + 统计概览条',
    '统计分析页: 缺陷类别分布饼图 + 24h检出趋势柱状图 + 各类别平均置信度仪表盘 (全部 Canvas 手绘)',
    '系统设置页: 6类独立阈值滑块 + 推荐默认/全部0.65快捷按钮 + 摄像头开关 + 模型信息面板',
    '顶部状态栏: 实时FPS/检出数/阈值/摄像头状态指示',
]);

// Section 11
h1('十一、Pipeline 架构');
body('5 线程 + 2 队列的流水线架构:');
doc.moveDown(0.2);
code(`V4L2设备              采集线程              推理线程              显示线程
/dev/video52 ──→ [RGA 640×640] ──→ capQueue ──→ [RKNN推理] ──→ dispQueue ──→ [DRM+UI]
                    ↑ memcpy                  ↑ local_rgb 独立缓冲    ↑ 后缓冲渲染
                    队列内部缓冲区    JPEG预编码+帧间一致性过滤    memcpy→DRM

Web线程 (端口8080): 从推理线程接收帧+JPEG缓存 → HTTP服务 → 上位机浏览器`);

doc.moveDown(0.3);
bullet([
    '采集线程: V4L2 DQBUF → RGA YUYV→RGB888 → 推入 capQueue',
    '推理线程: 从 capQueue 取出 → memcpy 到 local_rgb (防竞态) → 亮度检测 → RKNN推理 → 按类别阈值过滤 → NMS → 帧间一致性过滤(连续3帧确认) → JPEG预编码(锁外) → 推入 dispQueue + 发布到Web',
    '显示线程: 从 dispQueue 取出 → 渲染到后缓冲 (防频闪) → memcpy 到 DRM → UI 叠加',
    '输入线程: termios raw 模式读 stdin → 处理 S/+/-/R/Q 按键',
    'Web线程: HTTP socket listen → 路由分发 (/ /stream /stats /records /threshold /capture) → MJPEG 预编码缓存复用 (零实时编码)',
]);

// Section 12
doc.addPage();
h1('十二、Web REST API');
body('所有 API 通过 HTTP/1.0 访问, Content-Type: application/json, Access-Control-Allow-Origin: *');
doc.moveDown(0.3);
table(
    ['端点', '方法', '功能', '参数/响应'],
    [
        ['/', 'GET', '返回 Web 控制台页面', 'text/html, 嵌入式单文件 HTML'],
        ['/stream', 'GET', 'MJPEG 视频流', 'multipart/x-mixed-replace, 预编码JPEG缓存'],
        ['/stats', 'GET', '系统统计', 'JSON: fps, total, defects, camera, threshold, cls_thr[6], cls_cnt[6]'],
        ['/records', 'GET', '检测记录列表', 'JSON数组: [{ts, cls, conf}, ...], 最多200条'],
        ['/capture', 'POST', '手动采集或切换摄像头', '?cmd=toggle 切换, 响应: {ok, camera}'],
        ['/threshold', 'POST', '设置置信阈值', '?value=X.XX 全局, ?cls=N&value=X.XX 单类'],
    ]
);

// Section 13
h1('十三、待完成工作');
table(
    ['优先级', '任务', '状态', '说明'],
    [
        ['🔴 高', '实际光伏板场景测试', '待部署', '验证各类别检出率/误检率'],
        ['🔴 高', '代码开源到 GitHub', '未做', '赛道硬性要求'],
        ['🔴 高', '模型精度优化', '进行中', '已设各类别阈值、帧间一致性, 需补充反光/误检样本重训'],
        ['🟠 中', '竞赛文档完善', '待做', '设计报告、测试报告、演示视频'],
        ['🟠 中', '24h稳定性测试', '未做', '长时间高负载连续运行验证'],
        ['🟠 中', 'PWM 舵机巡检', '未实现', '/sys/class/pwm/ 路径'],
        ['🟡 低', '检测记录存盘', '未实现', '缺陷截图+时间戳保存'],
        ['🟡 低', '画面颜色优化', '待调整', 'RGA 色彩参数'],
        ['🟢 低', '告警推送', '未实现', 'WebSocket 实时推送缺陷告警'],
    ]
);

// Footer
doc.addPage();
doc.moveDown(4);
doc.fontSize(14).font(F).fillColor(ACCENT).text('— 文档结束 —', { align: 'center' });
doc.moveDown(1);
doc.fontSize(9).font(F).fillColor(GRAY).text('光伏板缺陷检测系统 项目移植手册 v4.0', { align: 'center' });
doc.fontSize(8).font(F).fillColor(GRAY).text('生成日期: 2026-06-12 | 平台: RV1126B | 模型: YOLOv8n INT8 | Web: 工业控制台', { align: 'center' });
doc.fontSize(8).font(F).fillColor(GRAY).text('工作目录: /root/solar_defect/ | Web端口: 8080 | 核心文件: solar_defect.c + web_ui.h', { align: 'center' });

doc.end();
console.log('PDF generated: 项目移植手册.pdf');
