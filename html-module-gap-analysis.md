# HTML 模块基础功能缺口分析

审计对象：`src/formats/html/`（`src/formats/html/litehtml/` 为 litehtml 替换层，不含 `src/thirdparty`）
规模：约 **60,439 行** C++ / **111 个** CSS 源文件 / 74 个 `HTML*` 类绑定
约束前提：**不支持 JavaScript，脚本后端只有 GDScript**

---

## 0. 总体结论

这个模块的"渲染层"完成度远高于预期，不是半成品：

| 层 | 状态 |
| --- | --- |
| DOM/CSSOM API（查询、增删、克隆、adopt/import、Range、Selection、MutationObserver） | **接近完整**，有 WPT 移植测试 |
| CSS（Grid + Subgrid、容器查询、`:has()/is()/where()`、`@layer`、`@scope`、`oklch/color-mix`、masks、3D transform、scroll snap） | **接近现代浏览器** |
| 表单（22 种 input 状态、日期/时间/取色器、`<datalist>`、约束校验、`method=dialog`） | **完整** |
| 媒体（`<video>`/`<audio>` 控件、字幕除外） | **基本完整** |
| **交互 / 事件层** | **基础层已实现**：派发、输入/触摸、控件编辑、状态事件与监听 API；仍有浏览器标准覆盖差距 |
| **脚本运行时（GDScript 后端）** | **只有一个"挂载钩子"** |

一句话：**渲染、DOM 和 GDScript 行为宿主都已存在；本次补齐了事件层的可用基础设施。**
事件层目前支持元素/document/window 目标、capture/target/bubble、取消默认动作、pointer capture、GDScript 行为处理器、控件编辑事件和常见组件状态事件；它仍是面向项目需求的子集，不等同于完整浏览器运行时。

---

## 1. DOM 事件模型状态

### 1.1 已实现能力

| 能力 | 当前行为 |
| --- | --- |
| 事件目标 | 元素、document、window；通过 `HTMLEventTarget` 监听和派发 |
| 传播 | capture、target、bubble；`stop_propagation()`、`stop_immediate_propagation()` 和 `prevent_default()` |
| 监听器 | `add_event_listener()` / `remove_event_listener()` 保持 Error API；`listen()` 返回可 `cancel()` 的句柄；支持 `once`、`capture`、`passive` |
| 默认 passive | 未指定时，window/document 上的 `wheel`、`touchstart`、`touchmove` 为 passive；显式选项覆盖默认值 |
| GDScript handler | `onclick="_on_click()"` 或 `onclick="_on_click(event)"` 在行为脚本中按文档顺序解析；Callable `on*` 属性覆盖同一事件的属性处理器 |
| 输入 | 鼠标、键盘、触摸、pointer capture；`mousemove`、`pointermove`、`touchmove` 合帧，wheel 保持同步 |
| 编辑 | `beforeinput` / `input`、IME composition、受限 `text/plain` 剪贴板、控件 `select` 和 document `selectionchange` |
| 组件状态 | 表单 `invalid`、details `beforetoggle` / `toggle`、dialog `cancel` / `close`、`scrollend`、媒体与 CSS animation/transition 生命周期事件 |
| 宿主生命周期 | 实际视口尺寸变化时在 window 派发 `resize`；有效树可见性变化时在 document 派发 `visibilitychange`，并可读 `document.visibility_state`；OS 窗口焦点变化映射为 window `focus` / `blur` |
| 诊断 | `HTMLLabel.event_debug` 输出派发摘要，`get_event_listener_count(type)` 查询显式监听器数量 |

事件对象在派发期间保留 `composed_path()` 快照，派发结束后路径清空。`composed` 可读写，但当前没有 Shadow DOM，因此它不会改变传播路径。`DOMContentLoaded` 在首次成功布局后发往 document，随后 `load` 发往 window。

### 1.2 剩余覆盖差距

- 没有 JavaScript 引擎；`on*` 属性只接受方法调用形式，不执行任意内联表达式
- 没有 Shadow DOM、slotting 或跨 shadow root 传播语义
- 移动事件只保留每帧最后坐标；没有公开 `event_throttle_ms`，wheel 不做合并
- 媒体事件由当前本地音频/视频后端状态映射；下载进度、网络缓冲和播放速率等后端未提供的状态不会伪造
- 事件类型工厂和 `on*` 属性集合是显式白名单；未列出的 DOM 标准事件仍可用基础 `HTMLEvent` 手动派发，但不会自动产生
- 行为脚本中的属性处理器没有 JavaScript 的 `this` 绑定、词法环境或表达式求值；返回布尔 `false` 只在事件可取消时阻止默认动作

---

## 2. 脚本宿主（GDScript 后端）本身的缺口

整个运行时约 **124 行**（`src/nodes/html_label_behaviors.cpp`）。

### 2.1 选择机制

```cpp
for (auto& element : query_selector_all("script")) {
    if (declaration->get_type().nocasecmp_to("text/gdscript") != 0) continue;
    const String name = declaration->get_src().strip_edges();
    auto script = label->behavior_scripts.get(StringName(name));   // 字典 key
    ...
}
```

- `type` 现在会忽略 MIME 参数，`text/gdscript; charset=utf-8` 也可匹配
- `src` 先按 `behavior_scripts` 精确匹配别名，未命中时按文档生效基准解析本地路径。支持 `res://`、`user://`、`uid://` 和相对路径；远程脚本 URL 被拒绝
- 内联 `<script>…</script>` 的 body 永不执行，只当数据
- `<template>` 内和 SVG foreign content 内的 `<script>` 不会被选中
- 挂载失败通过 `push_warning` 和 `HTMLLabel.get_behavior_diagnostics()` 报告，包含原始 `src`、解析路径和失败原因
- `render_error` 非空时仍会跳过 behavior 挂载，并记录文档渲染失败诊断

### 2.2 能力矩阵

| 能力 | 状态 | 说明 |
| --- | --- | --- |
| 计时器 / 帧回调 | 已实现 | `HTMLBehavior.set_timeout/set_interval/on_frame`；以行为实例为所有者，`_detach` 后自动清理 |
| HTTP 请求 | 已实现 | `HTMLBehavior.request` 支持常见方法、headers、文本 body、异步响应和取消，并复用请求队列 |
| 行为存储 | 已实现 | `local_storage` 按文档/自定义命名空间写入 `user://`；`session_storage` 随每个 `HTMLLabel` 存活 |
| 导航 / 历史 | 已实现 | `HTMLLabel.navigate/back/forward/go` 支持本地 HTML 和 HTTP(S) HTML；同页 fragment 不新增历史项 |
| `console` / 错误上报 | **部分（只能用 Godot 的）** | 无页面日志缓冲、无按 `<script>` 归因、无运行期错误捕获（GDScript 无 try/catch） |
| 多脚本顺序 | 可用 | 文档顺序、同步、每个匹配元素一个独立实例 |
| `defer` / `async` / `nomodule` | **属性是死的** | 只反射属性（`html_metadata_elements.cpp:125-138`），挂载只看 `type` 和 `src` |
| 模块 / import | **无** | 扁平 `Dictionary[StringName, GDScript]`，无解析、无 import map、无依赖图 |
| 动态插入 `<script>` | **无** | `create_element("script")` + append 不执行；改 `src` 不重跑 |
| 热重载 / remount API | 部分实现 | `HTMLLabel.reload_behaviors()` 显式刷新资源并重挂；不自动监视文件变化 |
| 拿到宿主 label | 已实现 | `HTMLBehavior.label` / `get_label()` 在两个 attach hook 和 `_detach` 期间有效 |

---

## 3. 元素（标签）缺口

元素分派有两层：容器（`src/formats/html/backend/litehtml_container.cpp:141-161`）特化 `ruby/details/dialog/output/template/video/audio/meter/progress/select/option/textarea/input/script/style/link/source/img`；之后 litehtml 自己再特化 `br/p/img/table/td/th/link/title/a/tr/style/base/body/div/script/font`（`litehtml/document/src/document.cpp:363-436`）。其余标签成为通用 `litehtml::html_tag`——**有盒模型、可查询、可样式化，但无专属行为**。DOM 句柄层注册了 51 个元素类（`src/register_types.cpp:85-138`），未映射的标签拿到裸 `HTMLElement`。

注意：标签名不在 `css_string_id.h:18-141` 的表里也**不会被丢弃**——`_id()` 会内联任意字符串（`css_string_id.cpp:43-51`），未知标签照样成为同名通用元素。下面说的是"引擎支持度"，不是"能否解析"。

**没有对应 `HTML*` 类或专属代码路径的标准元素**：

| 缺失 | 严重度 | 说明 |
| --- | --- | --- |
| `<canvas>` | **部分已实现** | GDScript 2D context 提供 backing image、基础形状/路径/变换、线性渐变、图案、文本绘制、图像读写和数据 URL，且经 CSS 绘制树合成；径向渐变、strokeText 和 Path2D 尚缺 |
| `contenteditable` | **基础编辑已实现** | true/plaintext-only/false 继承、designMode、焦点、基础键盘修改、beforeinput/input 和受限编辑命令已接入；IME composition、HTML clipboard paste、撤销/重做和编辑算法的完整浏览器语义仍缺 |
| `drag` / `drop`（HTML5 DnD） | **文本路径已实现** | 选区拖出 plain/html MIME、跨 HTMLLabel 的可编辑投放和 Godot 原生字符串拖放已接通；任意 draggable 元素、文件拖入和完整 move/copy 语义尚缺 |
| `<iframe>` | 中 | 无浏览上下文、无 `srcdoc`/`sandbox`；Gumbo 把其内容当 RAWTEXT（`parser.c:3950-3956`），DOM 侧是 raw text（`html_dom.cpp:83-87`），最终按普通 inline 文本流式渲染 |
| `<object>` / `<embed>` / `<param>` | 中 | 惰性，无插件/嵌入对象处理；`object` 的子节点只当 fallback flow |
| `<math>`（MathML） | 低* | Gumbo 认识该标签并打 `mathml` 命名空间，但**无 tag id、无 UA 规则、无 MathML 布局**，公式退化为普通文本。（*本项目 LaTeX 覆盖数学需求，故标低） |
| `<map>` / `<area>` | 低 | 无图像地图、无热点命中测试；`area` 仅通过 `area[href]` 参与 `:link/:any-link` 匹配 |
| `<slot>` / Shadow DOM / 自定义元素 | 中 | 文档明说"不支持 declarative shadow root"；无 `attachShadow`/`customElements`/`getRootNode`。连字符自定义标签能创建能查询，但**无升级、无生命周期回调、无样式边界** |
| 内联 `<svg>` 图形 | 中 | SVG **确实有真实命名空间**（`element_namespace::svg`），几何解析完整（path/rect/circle/ellipse/polygon/polyline/line + transform + `<use>` + `<tspan>`），**但没有任何内联绘制通路**：形状永远画不出来，几何只服务 `clip-path`/`mask`；mask 是把内联 SVG 子树重新序列化后用 `Image::load_svg_from_string` 光栅化（`html_svg_mask.cpp:152-245`）。外部 `.svg` 仅能当 `<img>`/CSS 背景图光栅化使用 |
| `<track>`（字幕） | 低 | 仅列为 void 元素，无 TextTrack/字幕代码 |
| `<frameset>` / `<frame>` / `<noframes>` | 低 | 无 frame 布局；`get_body()` 只是把 frameset 当 body 替身 |
| `accesskey` | 低 | 无 |

已确认**支持**、无需担心：`<picture>`/`srcset`、`<datalist>`、`<details>/<summary>`、`<dialog>`（含 top layer/backdrop）、`<ruby>/<rt>/<rp>`、`<template>`（含 `content` fragment，行为完整且有 WPT 测试）、`<meter>/<progress>`、`<output>`、`<video>/<audio>`、`<table>` 全家桶（含 insert_row/insert_cell）、`<pre>`、`<wbr>`、`<plaintext>`/`<xmp>`（解析层）、`autofocus`、22 种 input。

### 3.1 顺带发现的细节缺陷（都不是大功能，但属于"不完整"）

- **`<noscript>` 的 fallback 内容总是渲染**：Gumbo 以 `scripting disabled` 解析，导致 `<noscript>` 里的内容直接被当普通 DOM 渲染（`parser.c:3962-3965`）。在"永远没有脚本"的前提下这算符合预期，但如果将来接 GDScript 行为，语义就是错的。
- **`<optgroup>` 的 label 不绘制**：下拉绘制循环只用 option 的 label（`html_select_node.cpp:216-228`），分组标题看不见。
- **`<big>` 无 UA 规则**：退化成普通字号（`css_default_stylesheet.h:54-70` 里没有 `big`）。
- **`<meta charset>` 被忽略**：源码一律按 UTF-8 解码（`litehtml_document_adapter.cpp:56-58`）；`<base target>` 只反射不使用。
- **表格遗留属性未映射**：`cellpadding`、`valign`、`td/th` 的 `align` 都不映射（`el_table.cpp:23-46`），只有 `width/height/cellspacing/border/bgcolor` 生效。
- **`<hr>` 的遗留属性只反射不生效**（align/color/noshade/size/width）。
- **`title` / `abbr[title]` 没有悬停提示气泡**：`HTMLLabel` 侧无 tooltip 代码（MarkdownLabel 只有链接/图片的 tooltip）。

---

## 4. CSS 缺口（按"文档用途"排序）

现有实现已覆盖 Grid+Subgrid、容器查询、`:has/:is/:where`、`@layer`、`@scope`、`oklch/lab/lch/color-mix`、相对颜色、mask、clip-path、3D transform、scroll snap、`@counter-style`、`::marker`。真正的缺口偏"文档型 / CJK / 无障碍"：

| # | 缺口 | 证据 |
| --- | --- | --- |
| 1 | **分页 / 打印媒体部分实现** | `HTMLDocument.create_print_layout()` 已启用 `@media print`，支持常见 `@page` 尺寸/方向/边距、`:first/:left/:right` 与命名页选项，并提供分页几何和 PDF 写出。分页仍以连续布局切片，支持简单选择器与内联的强制 `break-before/after: page|always` 及旧式别名；`break-inside`、widows/orphans、命名页的元素级分配、复杂级联和行内断页未实现。所有页面共用一个文本重排宽度。PDF 保留基础拉丁文字/矩形/圆形/图像对象，尚未嵌入字体，Helvetica 无法编码的字形（包括中日韩文字）会省略；材质效果、精确 overflow clip 与变换也未保留 |
| 2 | **多列布局缺失** | `columns` / `column-count` / `column-width` / `column-rule*` / `column-span` / `column-fill` 不在属性表中（`css/css_declarations.cpp:74-386`） |
| 3 | **竖排缺失** | 无 `writing-mode`、无 `text-orientation`；`writing-mode: vertical-rl` 被列为 `unknown_property` 的示例（`html_css_support.md:776`）；引擎显式只做水平书写（`css_declarations.cpp:570`、`css_units.cpp:46`）。古籍/竖排诗词不可行 |
| 4 | **无障碍/输入类媒体特性部分实现** | `HTMLLabel` 已投射元素级 Godot 无障碍树，包含常见 HTML/ARIA 角色、名称与描述、表单状态、列表/表格元数据及焦点/点击/滚动动作；heading/landmark 以 role description 表示。媒体查询支持 `prefers-reduced-motion`、`prefers-contrast: more`、`hover`、`pointer`、`any-hover`、`any-pointer`。`forced-colors` 仍缺失，因为跨平台 API 没有暴露该模式和系统色板；`less/custom` 对比度偏好也无法区分 |
| 5 | `@property` 部分实现 | 可注册 `*`、`<number>`、`<integer>`、`<length>`、`<percentage>`、`<length-percentage>`、`<color>`、`<custom-ident>` 及其 `|` 组合；支持 `inherits`、`initial-value`、常见值校验和按媒体条件生效。完整语法描述符、CSSOM 注册 API 与类型化动画插值尚缺 |
| 6 | `::first-line` / `::first-letter` | 在解析器里被注释掉（`css/css_selectors.cpp:566,587`），首字下沉/首行样式不可表达 |
| 7 | `unicode-bidi` | `:dir(ltr|rtl)` 已支持继承的 `dir` 属性与常见字母表的 `dir="auto"` 方向判断。`unicode-bidi` 仍未接入文字 shaping；现有 `direction` 主要影响单个 Godot 文本 run |
| 8 | `shape-outside` / `shape-margin` | 无属性，非矩形绕排不可用 |
| 9 | 低频：`mask-border`、`background-clip: text`、`@namespace`、`@starting-style`、scroll-driven `animation-timeline`、`::part`/`::slotted` | 各自已标注为 pending 或直接不支持 |

---

## 5. 其他基础能力

- **无障碍**：`HTMLLabel` 现在将标题、landmark、表单、链接、列表和表格等元素投射到 Godot accessibility tree，并提供常见状态和动作。Godot 的角色枚举不含 heading/landmark，跨平台 screen-reader 行为由 Godot 后端决定；Shadow DOM 和 slotting 尚不存在，因此组件边界与 `::part`/`::slotted` 仍未覆盖。
- **HTML → PDF / 分页导出**：已有 `HTMLPrintLayout` 和 PDFium 导出，可获得页数、纸张尺寸与可打印区域，也能提取 PDF 中的文本。CSS 分页控制、命名页自动切换、复杂效果栅格回退和完整字形嵌入仍需完善。
- **坐标/文档模型**：一个 label 一个文档、一个 selection，无多文档、无跨文档选区。
- **文档**：`doc_classes/HTMLLayout.xml` 与 `html_css_support.md` 已更新关键 CSS 能力和已知不支持边界；完整属性矩阵仍以 CSS support reference 为准。

---

## 6. 建议优先级

**P0 — 事件层：已实现基础目标，后续以行为缺口为主**

1. 已有 `HTMLEvent` / `HTMLEventTarget`、事件路径与传播阶段、监听器 options、可取消默认动作、`listen()` 句柄、passive 默认值和事件诊断接口。
2. 已接入鼠标/键盘/触摸/pointer、表单提交与校验、文本输入/IME/剪贴板、选区、details/dialog、滚动、媒体和 CSS 动画/过渡事件。
3. 已实现 GDScript 行为方法形式的 `on*` 属性；不支持 JavaScript 内联脚本。
4. 已覆盖宿主生命周期事件、pointer capture 释放、多指触摸、非默认 wheel factor、监听器派发期间增删和 on* 动态属性；后续可继续扩展异步媒体后端顺序与动画移除时的 cancel/end 边界。

**P1 — 脚本宿主（运行时基础能力已实现）**

5. 脚本别名与本地路径加载并存；`res://`、`user://`、`uid://`、相对路径可解析，远程脚本拒绝。
6. 挂载失败通过警告和结构化诊断报告；`_attach_document(document)` 可通过 `behavior.label` 访问宿主。
7. `HTMLBehavior` 提供请求、计时器和帧回调；行为卸载时取消所有关联任务。
8. 持久/会话存储与 `HTMLLabel` 导航历史已接入；显式 `reload_behaviors()` 可刷新并重挂脚本。自动文件监视、内联 GDScript 正文和 JavaScript 仍未实现。

**P2 — 文档形态**

9. 已提供 `@media print`、基础 `@page`/强制断页、分页几何和 HTML→PDF 写出；后续补完整的 CSS Fragmentation、命名页自动切换、widows/orphans 与复杂效果栅格回退。
10. 多列布局；竖排 `writing-mode`。
11. 已提供基础 `contenteditable` 和编辑命令；后续补 IME、富文本粘贴/剪贴板 HTML 与 undo/redo。
12. 已提供 `<canvas>` 的 GDScript 2D backing image、文本、线性渐变和 pattern；后续补径向渐变、Path2D、strokeText 和浏览器 `ImageData` 兼容性。
13. 已提供选区文本和 `draggable="true"` 元素 drag/drop（含 plain/html 与自定义 MIME），并与 Godot 字符串拖放互通；后续补文件拖入和跨 `HTMLLabel` 的完整移动语义。
14. 已提供 `prefers-reduced-motion` / `prefers-contrast: more`、输入设备能力查询及元素级无障碍树；`forced-colors` 仍受 Godot 跨平台 API 限制。

---

## 附：关键文件索引

| 主题 | 文件 |
| --- | --- |
| 元素工厂 / 标签特化 | `src/formats/html/backend/litehtml_container.cpp:141-162` |
| DOM 变更 → 布局失效 | `src/formats/html/dom/html_dom_mutations.cpp`、`html_dom.cpp:1429+` |
| 行为挂载 | `src/nodes/html_label_behaviors.cpp`（无 `.h`，成员在 `html_label.h:430-437, 478-482`） |
| 行为基类 | `src/formats/html/html_behavior.h/.cpp` |
| 输入与信号派发 | `src/nodes/html_label.cpp:165-482`（鼠标）、`:584-708`（键盘） |
| 表单 | `src/nodes/html_label_forms.cpp`、`src/formats/html/elements/html_form_state.cpp` |
| 网络（内部） | `src/formats/http_requests.h`、`src/formats/remote_images.cpp`、`src/formats/html/resources/html_remote_stylesheets.cpp` |
| 资源路径解析（无 script 分支） | `src/formats/html/resources/html_resource_paths.cpp` |
| 媒体特性白名单 | `src/formats/html/css/css_media_query.cpp:546-598` |
| 属性总表 | `src/formats/html/css/css_declarations.cpp:74-386` |
| CSS 支持文档 | `addons/dockit/docs/html_css_support.md` |
| API / Web 标准对照 | `addons/dockit/docs/html_api_reference.md` |
