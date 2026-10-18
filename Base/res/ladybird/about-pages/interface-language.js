/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

const translations = new Map(Object.entries({
    "Welcome to Thoth": "欢迎使用 Thoth",
    "A new place to wander": "开启一段新的探索",
    "A calmer way to browse.": "让浏览回归从容。",
    "Get started": "开始设置",
    "You can change these choices later.": "这些选项之后都可以更改。",
    "Make it yours": "按你的习惯设置",
    "Language": "界面语言",
    "Choose English or 简体中文.": "选择 English 或简体中文。",
    "Chinese UI translation is coming soon.": "",
    "Search": "搜索",
    "Choose the engine for address bar searches.": "选择地址栏使用的搜索引擎。",
    "Search engine": "搜索引擎",
    "Disable search": "停用搜索",
    "Search suggestions": "搜索建议",
    "Choose a search engine to use search suggestions.": "选择搜索引擎后即可启用搜索建议。",
    "Privacy": "隐私",
    "Choose whether to block common ads and trackers.": "选择是否拦截常见广告和跟踪器。",
    "Block ads and trackers": "拦截广告和跟踪器",
    "Keep pages calmer and reduce common tracking.": "减少常见跟踪，让页面更清爽。",
    "Choose filter lists": "选择过滤规则列表",
    "Blocks most ads.": "拦截大多数广告。",
    "Blocks trackers.": "拦截跟踪器。",
    "Blocks pop-ups and other annoyances.": "拦截弹窗和其他干扰内容。",
    "Back": "上一步",
    "Continue": "继续",
    "Start browsing": "开始浏览",
    "Saving your choices…": "正在保存设置…",
    "Could not save your settings. Please try again.": "无法保存设置，请重试。",

    "Thoth Settings": "Thoth 设置",
    "General": "常规",
    "Personalization": "个性化",
    "Permissions": "权限",
    "Blocking": "拦截",
    "Services": "服务",
    "Network": "网络",
    "Advanced": "高级",
    "Startup": "启动",
    "New tab page URL": "新标签页网址",
    "Restore default": "恢复默认值",
    "Appearance": "外观",
    "Show system menu bar": "显示系统菜单栏",
    "Show bookmarks bar": "显示书签栏",
    "Browser appearance": "浏览器外观",
    "Follow system": "跟随系统",
    "Light": "浅色",
    "Dark": "深色",
    "Use vertical tabs": "使用垂直标签页",
    "Expand vertical tabs on hover when collapsed": "收起时悬停展开垂直标签页",
    "Vertical tabs side": "垂直标签页位置",
    "Left": "左侧",
    "Right": "右侧",
    "Content": "内容",
    "Default zoom level": "默认缩放比例",
    "Force dark mode": "强制深色模式",
    "Darken web pages that don’t provide a dark theme of their own. Photos are left as they are; icons and line art are darkened along with the page.": "为未提供深色主题的网页启用深色效果。照片保持原样，图标和线稿会随页面一同变暗。",
    "Languages": "语言",
    "Settings...": "设置…",
    "Browsing behavior": "浏览行为",
    "Enable autoscrolling": "启用自动滚动",
    "Scroll pages by pressing the middle mouse button and moving the mouse.": "按住鼠标中键并移动鼠标即可滚动页面。",
    "Enable primary pasting": "启用中键粘贴",
    "Paste text from the clipboard when the middle mouse button is clicked.": "点击鼠标中键时粘贴剪贴板中的文本。",
    "Crash reports": "崩溃报告",
    "Open the folder containing saved crash reports.": "打开保存崩溃报告的文件夹。",
    "Open folder": "打开文件夹",
    "Interface typeface": "界面字体",
    "Choose a character for browser labels, controls, and the sidebar.": "选择浏览器标签、控件和侧边栏的字体风格。",
    "Typeface": "字体",
    "System": "系统默认",
    "Bricolage Grotesque · expressive": "Bricolage Grotesque · 活泼",
    "Space Grotesk · geometric": "Space Grotesk · 几何",
    "Fraunces · soft serif": "Fraunces · 柔和衬线",
    "Interface icons": "界面图标",
    "Adjust the visual weight of browser controls and sidebar icons.": "调整浏览器控件和侧边栏图标的视觉粗细。",
    "Icon weight": "图标粗细",
    "Fine line": "纤细",
    "Balanced": "均衡",
    "Expressive": "醒目",
    "Color palette": "配色",
    "Set the accent color for browser controls and highlights.": "设置浏览器控件和高亮内容的强调色。",
    "Lavender": "薰衣草",
    "Ocean": "海洋",
    "Sage": "鼠尾草",
    "Rose": "玫瑰",
    "Sand": "砂岩",
    "Graphite": "石墨",
    "Plum": "梅紫",
    "Terracotta": "陶土",
    "Interface language": "界面语言",
    "Choose the language used in browser controls and menus.": "选择浏览器控件和菜单使用的语言。",
    "English": "English",
    "简体中文": "简体中文",
    "Default search engine": "默认搜索引擎",
    "Select the search engine to use in the address bar.": "选择地址栏使用的搜索引擎。",
    "Show search suggestions": "显示搜索建议",
    "Autoplay": "自动播放",
    "This setting is controlled via the command line": "此设置由命令行控制",
    "Enable geolocation": "启用地理位置",
    "Allow websites to request your actual location.": "允许网站请求获取你的实际位置。",
    "Browsing data": "浏览数据",
    "Maximum disk cache size": "磁盘缓存上限",
    "Limit the space used for the HTTP disk cache.": "限制 HTTP 磁盘缓存占用的空间。",
    "MiB": "MiB",
    "GiB": "GiB",
    "Clear browsing data": "清除浏览数据",
    "Clear...": "清除…",
    "Global Privacy Control": "全局隐私控制",
    "Tell websites not to sell or share your data.": "告知网站不要出售或共享你的数据。",
    "Enable ad blocking": "启用广告拦截",
    "Use filter lists to block ads, trackers, and other unwanted content.": "使用过滤规则列表拦截广告、跟踪器和其他不需要的内容。",
    "Show the number of blocked requests on the ad blocking icon": "在广告拦截图标上显示已拦截请求数",
    "Remove": "移除",
    "Filter lists": "过滤规则列表",
    "Update enabled lists": "更新已启用的列表",
    "Show language-specific lists": "显示特定语言的列表",
    "Add a subscription": "添加订阅",
    "Add a filter list maintained by a source you trust. Automatic updates reveal your IP address to that source.": "添加由可信来源维护的过滤规则列表。自动更新会向该来源公开你的 IP 地址。",
    "Add": "添加",
    "Local filter lists": "本地过滤列表",
    "Import Adblock-compatible filter lists stored on this computer.": "导入此电脑上保存的 Adblock 兼容过滤列表。",
    "Import list...": "导入列表…",
    "Custom filters": "自定义过滤规则",
    "Add Adblock-compatible rules, one per line.": "逐行添加 Adblock 兼容规则。",
    "Save changes": "保存更改",
    "Services are automatic connections Ladybird makes for browser features in the background. These controls do not affect normal browsing or actions you explicitly request, such as updating filter lists.": "服务是 Ladybird 为浏览器功能在后台建立的自动连接。这些选项不会影响正常浏览，也不会影响你主动发起的操作，例如更新过滤列表。",
    "Allow automatic service connections": "允许自动连接服务",
    "Let individually enabled services connect in the background.": "允许单独启用的服务在后台连接。",
    "Automatic service connections are paused. Individual service choices are preserved.": "自动服务连接已暂停，各项服务的选择会保留。",
    "Filter list updates": "过滤列表更新",
    "Periodically download updates for enabled filter list subscriptions.": "定期下载已启用过滤列表订阅的更新。",
    "DNS settings": "DNS 设置",
    "DNS Upstream": "DNS 上游",
    "System DNS": "系统 DNS",
    "Custom DNS Server": "自定义 DNS 服务器",
    "Type": "类型",
    "UDP": "UDP",
    "TLS": "TLS",
    "Validate DNSSEC Locally": "在本地验证 DNSSEC",
    "DNS Server (IP or hostname)": "DNS 服务器（IP 地址或主机名）",
    "Port": "端口",
    "Compatibility": "兼容性",
    "User agent": "用户代理",
    "Choose the browser identity sent to websites.": "选择发送给网站的浏览器标识。",
    "Default": "默认",
    "Configuration": "配置",
    "Choose languages for websites in order of preference": "选择网站首选语言，并按优先级排序",
    "Select a language to add...": "选择要添加的语言…",
    "Search Settings": "搜索设置",
    "Manage custom search engines": "管理自定义搜索引擎",
    "Search Engine Name": "搜索引擎名称",
    "Search URL": "搜索网址",
    "Use %s as a placeholder for the search query": "使用 %s 作为搜索词的占位符",
    "Suggestions URL (optional)": "搜索建议网址（可选）",
    "Use %s for the query. The response must use the OpenSearch JSON format.": "使用 %s 表示搜索词。响应必须采用 OpenSearch JSON 格式。",
    "Add Engine": "添加搜索引擎",
    "Default for all sites": "所有网站默认值",
    "Allowlist": "允许列表",
    "Add Site": "添加网站",
    "Remove All Sites": "移除所有网站",
    "No custom search engines added": "尚未添加自定义搜索引擎",
    "Local file": "本地文件",
    "Updating…": "正在更新…",
    "Enter an HTTP or HTTPS URL.": "请输入 HTTP 或 HTTPS 网址。",
    "The selected list is larger than 64 MiB.": "所选列表超过 64 MiB。",
    "The encoded list is too large to import.": "编码后的列表过大，无法导入。",
    "Custom filters must be 4 MiB or smaller.": "自定义过滤规则不能超过 4 MiB。",
    "Saved": "已保存",
    "Show WebContent process ID in tab titles": "在标签页标题中显示网页内容进程 ID",
    "Show Advanced Debug Menu": "显示高级调试菜单",
    "Show tab performance monitor": "显示标签页性能监视器",
    "Use client-side window decorations": "使用客户端窗口装饰",
    "Maximum connections per download": "每个下载任务的最大连接数",
    "Split downloads without server validators": "服务器未提供验证器时仍拆分下载",
    "Restart stalled download connections": "重新启动停滞的下载连接",
    "Append the active WebContent process ID to each tab title and tooltip.": "将当前网页内容进程 ID 添加到每个标签页标题和提示中。",
    "Show the advanced Debug menu in the application menu.": "在应用程序菜单中显示高级调试菜单。",
    "Show per-tab CPU, estimated owned memory, network rates and compositor frame rate in the toolbar.": "在工具栏中显示每个标签页的 CPU、估算内存、网络速率和合成器帧率。",
    "Use custom title bar and window controls instead of the system window frame.": "使用自定义标题栏和窗口控件代替系统窗口边框。",
    "Download a file over up to this many parallel connections when the server supports byte ranges. Set to 1 to always download over a single connection.": "服务器支持字节范围请求时，最多使用此数量的并行连接下载文件。设为 1 可始终使用单个连接。",
    "Allow multi-connection downloads even when the server sends no ETag or Last-Modified header, without which the browser cannot verify that every connection is being served the same file.": "即使服务器未发送 ETag 或 Last-Modified 标头，也允许使用多个连接下载；缺少这些标头时，浏览器无法验证各连接收到的是同一文件。",
    "When one connection of a multi-connection download stops receiving data, drop it and request the rest of its byte range over a new connection.": "多连接下载中的某个连接停止接收数据时，关闭该连接并通过新连接继续请求剩余字节范围。",
    "Remove browsing data from:": "清除以下时间范围内的浏览数据：",
    "Last hour": "过去一小时",
    "Last 4 hours": "过去四小时",
    "Today": "今天",
    "All time": "所有时间",
    "Cached files": "缓存文件",
    "Remove items that help pages load faster": "移除用于加快页面加载的文件",
    "Browsing history": "浏览历史",
    "Remove visited pages from history and address bar suggestions": "从历史记录和地址栏建议中移除访问过的页面",
    "Download history": "下载记录",
    "Remove records of downloaded files, but keep the files themselves": "移除下载记录，但保留下载的文件",
    "Cookies and site data": "Cookie 和网站数据",
    "Remove items that may sign you out of most sites": "移除可能使你退出大多数网站登录状态的数据",
    "Remove Data": "清除数据",
    "Settings": "设置",
    "Bookmark Manager": "书签管理器",
    "Bookmarks": "书签",
    "Bookmark options": "书签选项",
    "Import from HTML...": "从 HTML 导入…",
    "Export to HTML...": "导出为 HTML…",
    "Downloads": "下载",
    "Loading downloads...": "正在加载下载…",
    "Clear finished downloads": "清除已完成的下载",
    "History": "历史记录",
    "Search history": "搜索历史记录",
    "Loading history...": "正在加载历史记录…",
    "Load more": "加载更多",
    "About Thoth Browser": "关于 Thoth Browser",
    "About Thoth": "关于 Thoth",
    "Thoth Browser · About": "Thoth Browser · 关于",
    "Browser features": "浏览器功能",
    "Workspaces": "工作区",
    "Keep pages grouped by context.": "按场景整理页面。",
    "Split view": "分屏浏览",
    "Read two pages side by side.": "并排查看两个页面。",
    "Privacy controls": "隐私控制",
    "Choose how ads and trackers are filtered.": "选择广告和跟踪器的拦截方式。",
    "Alpha 2.0": "Alpha 2.0",
    "Version": "版本",
    "Build details": "构建信息",
    "Commit": "提交版本",
    "Architecture": "架构",
    "Operating system": "操作系统",
    "Runtime": "运行环境",
    "Command line": "命令行",
    "Application path": "应用程序路径",
    "No sites added": "尚未添加网站",
    "Move up": "上移",
    "Move down": "下移",
    "Allow Audio and Video": "允许音频和视频",
    "Block Audio": "阻止音频",
    "Block Audio and Video": "阻止音频和视频",
    "No custom search engines": "没有自定义搜索引擎",
    "No filter lists": "没有过滤列表",
    "About URLs": "关于页面地址",
}));

const originalText = new WeakMap();
const originalAttributes = new WeakMap();
let interfaceLanguage = "en";
let applying = false;

function localized(source) {
    if (interfaceLanguage !== "zh-CN") return source;
    const direct = translations.get(source);
    if (direct !== undefined) return direct;
    const removePrefix = source.match(/^Remove (.+)$/);
    if (removePrefix) return `移除 ${removePrefix[1]}`;
    const dataUsage = source.match(/^Your browsing data is currently using (.+) of disk space$/);
    if (dataUsage) return `当前浏览数据占用磁盘空间：${dataUsage[1]}`;
    const sizeDetail = source.match(/^ \(remove (.+)\)$/);
    if (sizeDetail) return `（移除 ${sizeDetail[1]}）`;
    return source;
}

function localizeTextNode(node) {
    if (!originalText.has(node))
        originalText.set(node, node.nodeValue);
    const source = originalText.get(node);
    const start = source.search(/\S/);
    if (start < 0) return;
    const end = source.trimEnd().length;
    const trimmed = source.slice(start, end).replace(/\s+/g, " ");
    const translated = localized(trimmed);
    const leading = source.slice(0, start);
    const trailing = source.slice(end);
    const next = `${leading}${translated}${trailing}`;
    if (node.nodeValue !== next)
        node.nodeValue = next;
}

function localizeElement(element) {
    for (const attribute of ["title", "placeholder", "aria-label", "aria-description", "aria-valuetext"]) {
        if (!element.hasAttribute(attribute)) continue;
        let sources = originalAttributes.get(element);
        if (!sources) {
            sources = new Map();
            originalAttributes.set(element, sources);
        }
        if (!sources.has(attribute))
            sources.set(attribute, element.getAttribute(attribute));
        const source = sources.get(attribute);
        const next = localized(source);
        if (element.getAttribute(attribute) !== next)
            element.setAttribute(attribute, next);
    }
}

function localizeTree(root = document) {
    if (applying) return;
    applying = true;
    const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
        acceptNode(node) {
            const parent = node.parentElement;
            if (!parent || parent.closest("script, style, textarea, code, pre"))
                return NodeFilter.FILTER_REJECT;
            return NodeFilter.FILTER_ACCEPT;
        },
    });
    while (walker.nextNode())
        localizeTextNode(walker.currentNode);
    if (root instanceof Element)
        localizeElement(root);
    for (const element of root.querySelectorAll?.("*") ?? [])
        localizeElement(element);
    document.documentElement.lang = interfaceLanguage;
    applying = false;
}

function setInterfaceLanguage(language) {
    interfaceLanguage = language === "zh-CN" ? "zh-CN" : "en";
    localizeTree();
    document.dispatchEvent(new CustomEvent("ThothInterfaceLanguageChanged", { detail: interfaceLanguage }));
}

document.addEventListener("WebUILoaded", () => {
    ladybird.sendMessage("loadInterfaceLanguage");
});

document.addEventListener("WebUIMessage", event => {
    if (event.detail.name === "interfaceLanguage" || event.detail.name === "interfaceLanguageChanged")
        setInterfaceLanguage(event.detail.data);
});

document.addEventListener("ThothInterfaceLanguagePreview", event => {
    setInterfaceLanguage(event.detail);
});

new MutationObserver(records => {
    if (applying) return;
    for (const record of records) {
        for (const node of record.addedNodes) {
            if (node.nodeType === Node.TEXT_NODE)
                localizeTextNode(node);
            else if (node instanceof Element)
                localizeTree(node);
        }
    }
}).observe(document.documentElement, { childList: true, subtree: true });
