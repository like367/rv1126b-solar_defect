const pptxgen = require("pptxgenjs");
const React = require("react");
const ReactDOMServer = require("react-dom/server");
const sharp = require("sharp");
const {
  FaCamera, FaMicrochip, FaSearch, FaGlobe,
  FaRocket, FaChartBar, FaCogs, FaCheckCircle,
  FaLightbulb, FaServer, FaMobileAlt, FaTachometerAlt,
  FaProjectDiagram, FaLayerGroup, FaDatabase
} = require("react-icons/fa");

// ── Icon helper ────────────────────────────────────────
function renderIconSvg(IconComponent, color = "#000000", size = 256) {
  return ReactDOMServer.renderToStaticMarkup(
    React.createElement(IconComponent, { color, size: String(size) })
  );
}
async function iconToBase64Png(IconComponent, color, size = 256) {
  const svg = renderIconSvg(IconComponent, color, size);
  const pngBuffer = await sharp(Buffer.from(svg)).png().toBuffer();
  return "image/png;base64," + pngBuffer.toString("base64");
}

// ── Color palette ──────────────────────────────────────
const C = {
  dark:      "1A365D",
  navy:      "2B4C7E",
  blue:      "2B6CB0",
  teal:      "0D9488",
  tealLight: "14B8A6",
  accentBg:  "EBF4FF",
  cardBg:    "FFFFFF",
  lightBg:   "F0F4F8",
  sectionBg: "E6F0FA",
  text:      "1A202C",
  muted:     "718096",
  white:     "FFFFFF",
  border:    "E2E8F0",
  green:     "38A169",
  orange:    "DD6B20",
  red:       "E53E3E",
  purple:    "805AD5",
};

const FONT_TITLE = "Arial Black";
const FONT_BODY  = "Calibri";

// ── Slide helper ───────────────────────────────────────
function slideHeader(slide, pres, title, subtitle) {
  // Top bar
  slide.addShape(pres.shapes.RECTANGLE, {
    x:0, y:0, w:10, h:0.08, fill: { color: C.teal },
  });
  slide.addText(title, {
    x:0.6, y:0.3, w:9, h:0.6, fontSize:26, fontFace:FONT_TITLE,
    color:C.dark, bold:true, align:"left", valign:"middle", margin:0,
  });
  if (subtitle) {
    slide.addText(subtitle, {
      x:0.6, y:0.85, w:9, h:0.35, fontSize:13, fontFace:FONT_BODY,
      color:C.muted, align:"left", valign:"top", margin:0,
    });
  }
  // Separator line
  slide.addShape(pres.shapes.LINE, {
    x:0.6, y:subtitle ? 1.2 : 0.95, w:8.8, h:0,
    line: { color: C.border, width: 1 },
  });
}

// ── Progress/phase colors for cards ────────────────────
const PHASE_COLORS = [C.teal, C.blue, C.purple, C.orange];

// ── Main ───────────────────────────────────────────────
async function main() {
  const pres = new pptxgen();
  pres.layout = "LAYOUT_16x9";
  pres.author = "Solar Defect Team";
  pres.title = "基于RV1126B的端侧光伏板缺陷智能巡检系统";

  // Pre-render icons
  const icons = {
    camera:  await iconToBase64Png(FaCamera, "#" + C.teal),
    chip:    await iconToBase64Png(FaMicrochip, "#" + C.blue),
    search:  await iconToBase64Png(FaSearch, "#" + C.purple),
    globe:   await iconToBase64Png(FaGlobe, "#" + C.orange),
    rocket:  await iconToBase64Png(FaRocket, "#" + C.teal),
    chart:   await iconToBase64Png(FaChartBar, "#" + C.blue),
    cogs:    await iconToBase64Png(FaCogs, "#" + C.purple),
    check:   await iconToBase64Png(FaCheckCircle, "#" + C.green),
    bulb:    await iconToBase64Png(FaLightbulb, "#" + C.orange),
    server:  await iconToBase64Png(FaServer, "#" + C.teal),
    mobile:  await iconToBase64Png(FaMobileAlt, "#" + C.blue),
    tacho:   await iconToBase64Png(FaTachometerAlt, "#" + C.purple),
    diagram: await iconToBase64Png(FaProjectDiagram, "#" + C.teal),
    layer:   await iconToBase64Png(FaLayerGroup, "#" + C.blue),
    db:      await iconToBase64Png(FaDatabase, "#" + C.purple),
  };

  // ═══════════════════════════════════════════════════
  // SLIDE 1 — Title + Overview Cards
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.white };

    // Teal accent bar at top
    s.addShape(pres.shapes.RECTANGLE, {
      x:0, y:0, w:10, h:0.06, fill: { color: C.teal },
    });

    // Title — increased box height + explicit lineSpacing to prevent line overlap
    s.addText("基于RV1126B的端侧光伏板\n缺陷智能巡检系统", {
      x:0.6, y:0.35, w:8.8, h:2.2,
      fontSize:36, fontFace:FONT_TITLE, bold:true, color:C.dark,
      align:"left", valign:"top", margin:0,
      lineSpacing: 56,
    });

    // Subtitle line
    s.addText("瑞芯微赛道 · 选题二：端侧AI视觉应用", {
      x:0.6, y:2.2, w:6, h:0.4, fontSize:14, fontFace:FONT_BODY,
      color:C.teal, bold:true, align:"left", valign:"top", margin:0,
    });

    // Separator
    s.addShape(pres.shapes.LINE, {
      x:0.6, y:2.65, w:8.8, h:0, line: { color: C.border, width: 1 },
    });

    // "本系统实现的核心功能" section header
    s.addText("本系统实现的核心功能", {
      x:0.6, y:2.75, w:5, h:0.4, fontSize:15, fontFace:FONT_BODY,
      color:C.dark, bold:true, align:"left", valign:"middle", margin:0,
    });

    // 4 feature cards
    const features = [
      { icon:"camera",  title:"实时采集与显示", desc:"USB摄像头640×480@30fps\nDRM帧缓冲本地显示", },
      { icon:"chip",    title:"AI端侧推理",     desc:"RKNN INT8量化模型\n~20-28 FPS实时推理", },
      { icon:"search",  title:"缺陷智能识别",   desc:"6类光伏板缺陷检测\n帧间一致性过滤误检", },
      { icon:"globe",   title:"Web远程监控",    desc:"MJPEG实时视频流\n手机/PC浏览器远程访问", },
    ];
    const cardW = 2.0;
    const cardGap = 0.27;
    const cardStartX = 0.6;
    const cardY = 3.2;
    const cardH = 1.7;

    features.forEach((f, i) => {
      const cx = cardStartX + i * (cardW + cardGap);
      // Card bg
      s.addShape(pres.shapes.RECTANGLE, {
        x:cx, y:cardY, w:cardW, h:cardH,
        fill: { color: C.cardBg },
        shadow: { type:"outer", color:"000000", blur:4, offset:1, angle:135, opacity:0.08 },
      });
      // Top accent
      s.addShape(pres.shapes.RECTANGLE, {
        x:cx, y:cardY, w:cardW, h:0.05, fill: { color: PHASE_COLORS[i] },
      });
      // Icon
      s.addImage({
        data: icons[f.icon],
        x:cx + 0.65, y:cardY + 0.2, w:0.55, h:0.55,
      });
      // Title
      s.addText(f.title, {
        x:cx + 0.12, y:cardY + 0.8, w:cardW - 0.24, h:0.3,
        fontSize:12, fontFace:FONT_BODY, color:C.dark, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // Description
      s.addText(f.desc, {
        x:cx + 0.12, y:cardY + 1.05, w:cardW - 0.24, h:0.6,
        fontSize:9.5, fontFace:FONT_BODY, color:C.muted,
        align:"center", valign:"top", margin:0,
      });
    });

    // 4 metric badges at the bottom
    const metrics = [
      { val:"~20-28", unit:"FPS", label:"推理帧率", color:C.teal },
      { val:"6",  unit:"类", label:"缺陷类别", color:C.blue },
      { val:"INT8", unit:"",  label:"模型量化", color:C.purple },
      { val:"可调",  unit:"",  label:"置信度阈值", color:C.orange },
    ];
    const mw = 1.8;
    const mg = 0.4;
    const mStart = 0.6;
    const mY = 5.0;

    metrics.forEach((m, i) => {
      const mx = mStart + i * (mw + mg);
      // metric bg pill
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, {
        x:mx, y:mY, w:mw, h:0.5,
        fill: { color: m.color, transparency: 90 },
        rectRadius: 0.1,
      });
      // value
      s.addText(m.val, {
        x:mx, y:mY, w:mw * 0.5, h:0.5,
        fontSize:16, fontFace:FONT_TITLE, color:m.color, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // unit
      const unitX = mx + mw * 0.5;
      s.addText(m.unit, {
        x:unitX, y:mY, w:mw * 0.15, h:0.5,
        fontSize:9, fontFace:FONT_BODY, color:C.muted,
        align:"left", valign:"middle", margin:0,
      });
      // label
      s.addText(m.label, {
        x:mx + mw * 0.63, y:mY, w:mw * 0.37, h:0.5,
        fontSize:9, fontFace:FONT_BODY, color:C.muted,
        align:"left", valign:"middle", margin:0,
      });
    });
  }

  // ═══════════════════════════════════════════════════
  // SLIDE 2 — System Pipeline (评委评分核心链路)
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.white };
    slideHeader(s, pres, "端侧AI视觉检测流水线", "选题二考核链路：摄像头采集 → 预处理 → NPU推理 → 结果上报");

    // Full pipeline: 5 horizontal stages across slide
    const pipeY = 1.5;
    const pipeH = 2.2;
    const stages = [
      { label:"摄像头采集", detail:"USB摄像头\nYUYV 640×480\nV4L2 MMAP", icon:"camera",
        ring:"选题要求①" },
      { label:"图像预处理", detail:"RGA硬件加速转码\nYUYV→RGB888\n640×640缩放", icon:"cogs",
        ring:"选题要求④" },
      { label:"NPU推理", detail:"RKNN INT8量化\nYOLOv8n 6类检测\n3TOPS NPU加速", icon:"chip",
        ring:"选题要求①" },
      { label:"后处理过滤", detail:"NMS非极大抑制\n按类别阈值过滤\n帧间一致性消抖", icon:"search",
        ring:"选题要求⑤" },
      { label:"双模输出", detail:"本地DRM 1024×600\nWeb远程MJPEG推流\n手机/PC远程访问", icon:"globe",
        ring:"选题要求③" },
    ];

    const sW = 1.6;
    const sGap = 0.25;
    const sX = 0.6;

    stages.forEach((st, i) => {
      const cx = sX + i * (sW + sGap);

      // Card
      s.addShape(pres.shapes.RECTANGLE, {
        x:cx, y:pipeY, w:sW, h:pipeH,
        fill: { color: C.cardBg },
        shadow: { type:"outer", color:"000000", blur:3, offset:1, angle:135, opacity:0.06 },
      });
      // Top accent
      s.addShape(pres.shapes.RECTANGLE, {
        x:cx, y:pipeY, w:sW, h:0.05, fill: { color: PHASE_COLORS[i % 4] },
      });
      // Direction arrow (except last)
      if (i < stages.length - 1) {
        s.addText("→", {
          x:cx + sW - 0.08, y:pipeY + pipeH/2 - 0.2, w:0.4, h:0.4,
          fontSize:18, fontFace:FONT_BODY, color:C.teal, bold:true,
          align:"center", valign:"middle", margin:0,
        });
      }
      // Icon
      s.addImage({
        data: icons[st.icon],
        x:cx + sW/2 - 0.25, y:pipeY + 0.15, w:0.5, h:0.5,
      });
      // Label
      s.addText(st.label, {
        x:cx + 0.05, y:pipeY + 0.7, w:sW - 0.1, h:0.35,
        fontSize:12, fontFace:FONT_BODY, color:C.dark, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // Detail
      s.addText(st.detail, {
        x:cx + 0.08, y:pipeY + 1.05, w:sW - 0.16, h:0.9,
        fontSize:9, fontFace:FONT_BODY, color:C.muted,
        align:"center", valign:"top", margin:0,
      });
      // Badge: corresponding competition requirement
      s.addShape(pres.shapes.ROUNDED_RECTANGLE, {
        x:cx + 0.15, y:pipeY + pipeH - 0.4, w:sW - 0.3, h:0.28,
        fill: { color: C.teal, transparency: 85 },
        rectRadius: 0.08,
      });
      s.addText(st.ring, {
        x:cx + 0.15, y:pipeY + pipeH - 0.4, w:sW - 0.3, h:0.28,
        fontSize:7.5, fontFace:FONT_BODY, color:C.teal, bold:true,
        align:"center", valign:"middle", margin:0,
      });
    });

    // Bottom: hardware photo placeholder area
    s.addShape(pres.shapes.RECTANGLE, {
      x:0.6, y:4.0, w:8.8, h:0.45,
      fill: { color: C.sectionBg },
    });
    s.addText("赛题方向：瑞芯微赛道·选题二——端侧AI视觉应用  |  硬件平台：ELF-RV1126B (RV1126B, 3TOPS@INT8, ARM Cortex-A7)", {
      x:0.8, y:4.0, w:8.4, h:0.45,
      fontSize:10, fontFace:FONT_BODY, color:C.dark,
      align:"center", valign:"middle", margin:0,
    });

    // Bottom citation
    s.addText("注：选题要求①NPU运行视觉模型 ②完整数据处理流程 ③网络/串口上报结果 ④图像预处理 ⑤视觉算法性能", {
      x:0.6, y:4.5, w:8.8, h:0.3,
      fontSize:8, fontFace:FONT_BODY, color:C.muted, italic:true,
      align:"center", valign:"middle", margin:0,
    });
  }

  // ═══════════════════════════════════════════════════
  // SLIDE 3 — Technical Innovations (评委加分项)
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.white };
    slideHeader(s, pres, "核心技术亮点", "边缘AI全栈自研：从模型部署到Web控制台100%纯C实现");

    // 2-column layout: left = 2 innovation cards stacked, right = 1 tall card
    const colL = 0.6;
    const colR = 5.3;
    const cw = 4.1;

    // --- Left top: 双过滤机制 ---
    const topY = 1.5;
    s.addShape(pres.shapes.RECTANGLE, {
      x:colL, y:topY, w:cw, h:1.6,
      fill: { color: C.cardBg },
      shadow: { type:"outer", color:"000000", blur:3, offset:1, angle:135, opacity:0.06 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x:colL, y:topY, w:cw, h:0.04, fill: { color: C.teal },
    });
    // Number badge
    s.addShape(pres.shapes.OVAL, {
      x:colL + 0.2, y:topY + 0.2, w:0.4, h:0.4,
      fill: { color: C.teal },
    });
    s.addText("1", {
      x:colL + 0.2, y:topY + 0.2, w:0.4, h:0.4,
      fontSize:16, fontFace:FONT_TITLE, color:C.white, bold:true,
      align:"center", valign:"middle", margin:0,
    });
    s.addText("按类别阈值 + 帧间一致性双过滤", {
      x:colL + 0.75, y:topY + 0.15, w:cw - 0.9, h:0.4,
      fontSize:13, fontFace:FONT_BODY, color:C.dark, bold:true,
      align:"left", valign:"middle", margin:0,
    });
    s.addText([
      { text:"6类缺陷独立阈值", options:{bullet:true,breakLine:true} },
      { text:"帧间IoU匹配连续3帧确认", options:{bullet:true,breakLine:true} },
      { text:"最小面积400px²过滤微小误检", options:{bullet:true} },
    ], {
      x:colL + 0.3, y:topY + 0.6, w:cw - 0.5, h:0.85,
      fontSize:10.5, fontFace:FONT_BODY, color:C.text,
      valign:"top", margin:0, paraSpaceAfter:3,
    });

    // --- Left bottom: 反光优化 ---
    const botY = 3.3;
    s.addShape(pres.shapes.RECTANGLE, {
      x:colL, y:botY, w:cw, h:1.6,
      fill: { color: C.cardBg },
      shadow: { type:"outer", color:"000000", blur:3, offset:1, angle:135, opacity:0.06 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x:colL, y:botY, w:cw, h:0.04, fill: { color: C.blue },
    });
    s.addShape(pres.shapes.OVAL, {
      x:colL + 0.2, y:botY + 0.2, w:0.4, h:0.4,
      fill: { color: C.blue },
    });
    s.addText("2", {
      x:colL + 0.2, y:botY + 0.2, w:0.4, h:0.4,
      fontSize:16, fontFace:FONT_TITLE, color:C.white, bold:true,
      align:"center", valign:"middle", margin:0,
    });
    s.addText("反光场景端侧图像优化", {
      x:colL + 0.75, y:botY + 0.15, w:cw - 0.9, h:0.4,
      fontSize:13, fontFace:FONT_BODY, color:C.dark, bold:true,
      align:"left", valign:"middle", margin:0,
    });
    s.addText([
      { text:"高光检测：饱和像素>30%自动跳过", options:{bullet:true,breakLine:true} },
      { text:"RGB三通道直方图均衡化改善对比度", options:{bullet:true,breakLine:true} },
      { text:"黑屏检测：亮度均值<20跳过推理", options:{bullet:true} },
    ], {
      x:colL + 0.3, y:botY + 0.6, w:cw - 0.5, h:0.85,
      fontSize:10.5, fontFace:FONT_BODY, color:C.text,
      valign:"top", margin:0, paraSpaceAfter:3,
    });

    // --- Right tall card: Architecture highlights ---
    s.addShape(pres.shapes.RECTANGLE, {
      x:colR, y:1.5, w:cw, h:3.4,
      fill: { color: C.cardBg },
      shadow: { type:"outer", color:"000000", blur:3, offset:1, angle:135, opacity:0.06 },
    });
    s.addShape(pres.shapes.RECTANGLE, {
      x:colR, y:1.5, w:cw, h:0.04, fill: { color: C.purple },
    });
    s.addShape(pres.shapes.OVAL, {
      x:colR + 0.2, y:1.7, w:0.4, h:0.4,
      fill: { color: C.purple },
    });
    s.addText("3", {
      x:colR + 0.2, y:1.7, w:0.4, h:0.4,
      fontSize:16, fontFace:FONT_TITLE, color:C.white, bold:true,
      align:"center", valign:"middle", margin:0,
    });
    s.addText("纯C全栈实现 · 无外部依赖", {
      x:colR + 0.75, y:1.65, w:cw - 0.9, h:0.4,
      fontSize:13, fontFace:FONT_BODY, color:C.dark, bold:true,
      align:"left", valign:"middle", margin:0,
    });

    const archPoints = [
      { bold:"5线程流水线", text:"采集→推理→显示→键盘→Web，双队列松耦合" },
      { bold:"50MB运行内存", text:"含4.8MB模型，编译后单二进制文件" },
      { bold:"嵌入式Web控制台", text:"纯C HTTP服务+MJPEG推流+Canvas图表，~28KB内嵌" },
      { bold:"二轴舵机巡检", text:"GPIO软PWM+实时线程，支持自动步进巡检" },
      { bold:"异常自动恢复", text:"V4L2中断自动STREAMON，SIGPIPE客户端保护" },
    ];

    s.addText(archPoints.map((p, i) => ({
      text: p.bold, options: { bold:true, breakLine:false }
    })).concat(
      archPoints.map((p, i) => ({
        text: "：" + p.text,
        options: { breakLine: i < archPoints.length - 1 ? true : false }
      }))
    ).reduce((acc, item, i, arr) => {
      // Interleave bold + normal text
      if (i < archPoints.length) {
        acc.push({ text: archPoints[i].bold, options: { bold: true, breakLine: false, fontSize:10.5, color:"#" + C.dark } });
        acc.push({ text: "：" + archPoints[i].text, options: { breakLine: i < archPoints.length - 1 ? true : false, fontSize:10.5, color:"#" + C.muted } });
      }
      return acc;
    }, []), {
      x:colR + 0.2, y:2.2, w:cw - 0.4, h:2.5,
      fontFace:FONT_BODY, valign:"top", margin:0, paraSpaceAfter:8,
    });
  }

  // ═══════════════════════════════════════════════════
  // SLIDE 4 — System Demo
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.white };
    slideHeader(s, pres, "系统演示", "端侧运行效果与用户界面展示");

    // 3 demo cards — simplified
    const demos = [
      {
        title:"本地UI界面",
        points:[
          "实时检测画面 + 类别标注",
          "FPS / 置信度实时显示",
          "键盘快捷键控制摄像头",
        ],
        color:C.teal,
      },
      {
        title:"Web远程监控",
        points:[
          "手机/PC浏览器实时画面",
          "MJPEG低延迟视频推流",
          "远程开关 + 阈值调节",
        ],
        color:C.blue,
      },
      {
        title:"运行性能",
        points:[
          "~20-28 FPS稳定推理",
          "5线程并行流水线",
          "帧间一致性过滤误检",
        ],
        color:C.purple,
      },
    ];

    const dCardW = 2.7;
    const dCardGap = 0.35;
    const dStartX = 0.6;
    const dCardY = 1.5;
    const dCardH = 2.8;

    demos.forEach((d, i) => {
      const dx = dStartX + i * (dCardW + dCardGap);
      // Card bg
      s.addShape(pres.shapes.RECTANGLE, {
        x:dx, y:dCardY, w:dCardW, h:dCardH,
        fill: { color: C.cardBg },
        shadow: { type:"outer", color:"000000", blur:4, offset:1, angle:135, opacity:0.06 },
      });
      // Top accent
      s.addShape(pres.shapes.RECTANGLE, {
        x:dx, y:dCardY, w:dCardW, h:0.06, fill: { color: d.color },
      });
      // Title
      s.addText(d.title, {
        x:dx, y:dCardY + 0.2, w:dCardW, h:0.35,
        fontSize:15, fontFace:FONT_BODY, color:C.dark, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // Points
      s.addText(d.points.map((p, pi) => ({
        text: p, options: { bullet: true, breakLine: true }
      })), {
        x:dx + 0.2, y:dCardY + 0.65, w:dCardW - 0.4, h:1.8,
        fontSize:10.5, fontFace:FONT_BODY, color:C.text,
        valign:"top", margin:0, paraSpaceAfter:4,
      });
    });

    // Bottom: scene description
    s.addShape(pres.shapes.RECTANGLE, {
      x:0.6, y:4.6, w:8.8, h:0.55,
      fill: { color: C.sectionBg },
    });
    s.addText("640×480 USB摄像头输入 → 640×640模型推理 → DRM 1024×600本地显示 + Web远程访问", {
      x:0.8, y:4.6, w:8.4, h:0.55,
      fontSize:10, fontFace:FONT_BODY, color:C.dark,
      align:"center", valign:"middle", margin:0,
    });
  }

  // ═══════════════════════════════════════════════════
  // SLIDE 5 — Performance & Evaluation
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.white };
    slideHeader(s, pres, "性能指标与赛题要求达成", "6类缺陷检测精度与选题考核指标对照");

    // Top: evaluation criteria checkboxes
    const criteria = [
      { label:"NPU运行视觉模型",            check:"✓", color:C.green, detail:"YOLOv8n INT8 @3TOPS NPU" },
      { label:"摄像头→预处理→推理全流程",  check:"✓", color:C.green, detail:"V4L2→RGA→RKNN→后处理" },
      { label:"网络/串口上报结果",          check:"✓", color:C.green, detail:"Web MJPEG + REST API" },
      { label:"图像预处理",                 check:"✓", color:C.green, detail:"HE + 高光/黑屏检测" },
      { label:"视觉算法性能",               check:"✓", color:C.green, detail:"20-28FPS, mAP50=0.889" },
      { label:"系统鲁棒性",                 check:"△", color:C.orange, detail:"帧间过滤 + 异常恢复" },
      { label:"系统稳定性",                 check:"○", color:C.muted, detail:"需补充24h测试数据" },
    ];

    const cw = 1.15;
    const cStart = 0.6;
    const cGap = 0.2;
    const cY = 1.4;

    criteria.forEach((c, i) => {
      const cx = cStart + i * (cw + cGap);
      // card
      s.addShape(pres.shapes.RECTANGLE, {
        x:cx, y:cY, w:cw, h:1.15,
        fill: { color: c.color, transparency: c.color === C.green ? 88 : 92 },
      });
      // checkmark
      s.addText(c.check, {
        x:cx, y:cY + 0.05, w:cw, h:0.4,
        fontSize:20, fontFace:FONT_TITLE, color:c.color, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // label
      s.addText(c.label, {
        x:cx + 0.05, y:cY + 0.4, w:cw - 0.1, h:0.35,
        fontSize:7.5, fontFace:FONT_BODY, color:C.dark, bold:true,
        align:"center", valign:"middle", margin:0,
      });
      // detail
      s.addText(c.detail, {
        x:cx + 0.05, y:cY + 0.75, w:cw - 0.1, h:0.35,
        fontSize:6.5, fontFace:FONT_BODY, color:C.muted,
        align:"center", valign:"top", margin:0,
      });
    });

    // Classification results table
    const tableHeader = (text) => ({
      text, options: { bold:true, color:"FFFFFF", fontSize:10, fontFace:FONT_BODY, fill:{color:C.navy}, align:"center", valign:"middle" }
    });
    const tableCell = (text, opts = {}) => ({
      text, options: { fontSize:9.5, fontFace:FONT_BODY, color:C.text, align:"center", valign:"middle", ...opts }
    });

    const resultsRows = [
      [
        tableHeader("缺陷类别"),
        tableHeader("mAP50"),
        tableHeader("阈值"),
        tableHeader("检出色"),
      ],
      [
        tableCell("bird-drop (鸟粪)"),
        tableCell("0.814"),
        tableCell("0.50"),
        tableCell("红色", { color: C.red }),
      ],
      [
        tableCell("clean (正常)"),
        tableCell("0.919"),
        tableCell("0.70"),
        tableCell("绿色", { color: C.green }),
      ],
      [
        tableCell("dusty (灰尘)"),
        tableCell("0.902"),
        tableCell("0.55"),
        tableCell("黄色", { color: C.orange }),
      ],
      [
        tableCell("electrical-damage (电气损伤)"),
        tableCell("0.919"),
        tableCell("0.50"),
        tableCell("紫色", { color: C.purple }),
      ],
      [
        tableCell("physical-damage (物理损伤)"),
        tableCell("0.920"),
        tableCell("0.50"),
        tableCell("青色"),
      ],
      [
        tableCell("snow-covered (积雪)"),
        tableCell("0.862"),
        tableCell("0.50"),
        tableCell("橙色", { color: C.orange }),
      ],
    ];

    s.addTable(resultsRows, {
      x:0.6, y:2.8, w:5.2,
      colW: [1.8, 0.8, 0.6, 0.8],
      border: { pt: 0.5, color: C.border },
      rowH: [0.32, 0.28, 0.28, 0.28, 0.28, 0.28, 0.28],
      autoPage: false,
    });

    // Right: Key metrics summary
    const perfX = 6.3;
    const perfItems = [
      { val:"20-28", unit:"FPS", label:"推理帧率" },
      { val:"0.889", unit:"mAP50", label:"模型精度" },
      { val:"4.8",   unit:"MB",   label:"模型体积" },
      { val:"~50",   unit:"MB",   label:"运行内存" },
    ];

    perfItems.forEach((p, i) => {
      const py = 2.8 + i * 0.6;
      // Metric card
      s.addShape(pres.shapes.RECTANGLE, {
        x:perfX, y:py, w:3.1, h:0.48,
        fill: { color: C.accentBg, transparency: 60 },
      });
      // Value
      s.addText(p.val, {
        x:perfX + 0.1, y:py, w:1.2, h:0.48,
        fontSize:18, fontFace:FONT_TITLE, color:C.teal, bold:true,
        align:"left", valign:"middle", margin:0,
      });
      // Unit
      s.addText(p.unit, {
        x:perfX + 1.15, y:py, w:0.6, h:0.48,
        fontSize:9, fontFace:FONT_BODY, color:C.muted,
        align:"left", valign:"middle", margin:0,
      });
      // Label
      s.addText(p.label, {
        x:perfX + 1.7, y:py, w:1.2, h:0.48,
        fontSize:10, fontFace:FONT_BODY, color:C.dark,
        align:"left", valign:"middle", margin:0,
      });
    });

    // Bottom: model info
    s.addShape(pres.shapes.RECTANGLE, {
      x:0.6, y:5.0, w:8.8, h:0.45,
      fill: { color: C.sectionBg },
    });
    s.addText("模型：YOLOv8n | 训练300epoch | 输入640×640 | 6类缺陷 | INT8量化部署至RV1126B NPU | 代码已开源", {
      x:0.8, y:5.0, w:8.4, h:0.45,
      fontSize:10, fontFace:FONT_BODY, color:C.dark,
      align:"center", valign:"middle", margin:0,
    });
  }

  // ═══════════════════════════════════════════════════
  // SLIDE 6 — Summary & Thanks
  // ═══════════════════════════════════════════════════
  {
    const s = pres.addSlide();
    s.background = { color: C.dark };

    // Decorative teal bar
    s.addShape(pres.shapes.RECTANGLE, {
      x:0, y:0, w:10, h:0.06, fill: { color: C.teal },
    });

    // Title
    s.addText("总结与展望", {
      x:0.6, y:0.5, w:8.8, h:0.6, fontSize:30, fontFace:FONT_TITLE,
      color:C.white, bold:true, align:"center", valign:"middle", margin:0,
    });

    // Separator
    s.addShape(pres.shapes.LINE, {
      x:3.5, y:1.15, w:3, h:0, line: { color: C.teal, width: 2 },
    });

    // Key achievements
    s.addText("项目成果", {
      x:0.6, y:1.4, w:4, h:0.4, fontSize:16, fontFace:FONT_BODY,
      color:C.tealLight, bold:true, align:"left", valign:"middle", margin:0,
    });

    const achievements = [
      "基于RV1126B实现端侧YOLOv8n实时缺陷检测，满足赛题全链路要求",
      "完整5线程并行流水线 + 纯C全栈实现，无外部运行时依赖",
      "嵌入式Web远程控制台 + MJPEG推流，手机/PC跨平台访问",
      "按类别阈值 + 帧间一致性双过滤，有效抑制误检",
      "反光/暗光场景端侧图像增强，提升实际环境检出率",
    ];
    s.addText(achievements.map((a, i) => ({
      text: a, options: { bullet: true, breakLine: true, indentLevel: 0 }
    })), {
      x:0.6, y:1.85, w:6, h:1.5,
      fontSize:11.5, fontFace:FONT_BODY, color:C.white,
      valign:"top", margin:0, paraSpaceAfter:6,
    });

    // Future work
    s.addText("后续展望", {
      x:0.6, y:3.3, w:4, h:0.4, fontSize:16, fontFace:FONT_BODY,
      color:C.tealLight, bold:true, align:"left", valign:"middle", margin:0,
    });

    const futures = [
      "PWM舵机控制：实现二轴自动巡检扫描",
      "模型优化：更轻量级网络结构提升帧率",
      "检测记录持久化：截图与日志本地存储",
    ];
    s.addText(futures.map((f, i) => ({
      text: f, options: { bullet: true, breakLine: true, indentLevel: 0 }
    })), {
      x:0.6, y:3.75, w:6, h:1.2,
      fontSize:11.5, fontFace:FONT_BODY, color:C.white,
      valign:"top", margin:0, paraSpaceAfter:6,
    });

    // Thank you at center-right
    s.addText("感谢聆听", {
      x:6.5, y:2.5, w:3, h:0.7, fontSize:32, fontFace:FONT_TITLE,
      color:C.white, bold:true, align:"center", valign:"middle", margin:0,
    });
    s.addText("Solar Defect Detection Team", {
      x:6.5, y:3.2, w:3, h:0.4, fontSize:11, fontFace:FONT_BODY,
      color:C.tealLight, align:"center", valign:"middle", margin:0,
    });
  }

  // ── Write ─────────────────────────────────────────
  await pres.writeFile({ fileName: "光伏板缺陷巡检系统_竞赛PPT_修改版.pptx" });
  console.log("PPTX generated: 光伏板缺陷巡检系统_竞赛PPT_修改版.pptx");
}

main().catch(console.error);
