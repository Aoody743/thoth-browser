/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWebView/Application.h>
#include <UI/Qt/Thoth/Localization.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTabWidget>
#include <QTabBar>
#include <QString>
#include <QWidget>

namespace Thoth {

static bool is_simplified_chinese()
{
    return WebView::Application::settings().interface_language().view() == "zh-CN"sv;
}

static QHash<QString, QString> const& simplified_chinese_translations()
{
    static QHash<QString, QString> const translations {
        { QStringLiteral("File"), QStringLiteral("文件") },
        { QStringLiteral("Edit"), QStringLiteral("编辑") },
        { QStringLiteral("View"), QStringLiteral("查看") },
        { QStringLiteral("Bookmarks"), QStringLiteral("书签") },
        { QStringLiteral("History"), QStringLiteral("历史记录") },
        { QStringLiteral("Inspect"), QStringLiteral("检查") },
        { QStringLiteral("Debug"), QStringLiteral("调试") },
        { QStringLiteral("Help"), QStringLiteral("帮助") },
        { QStringLiteral("New Tab"), QStringLiteral("新建标签页") },
        { QStringLiteral("New &Tab"), QStringLiteral("新建标签页") },
        { QStringLiteral("New Window"), QStringLiteral("新建窗口") },
        { QStringLiteral("New &Window"), QStringLiteral("新建窗口") },
        { QStringLiteral("New Private Window"), QStringLiteral("新建无痕窗口") },
        { QStringLiteral("New Pri&vate Window"), QStringLiteral("新建无痕窗口") },
        { QStringLiteral("Reopen Recently Closed Tab"), QStringLiteral("重新打开最近关闭的标签页") },
        { QStringLiteral("&Reopen Recently Closed Tab"), QStringLiteral("重新打开最近关闭的标签页") },
        { QStringLiteral("Close Current Tab"), QStringLiteral("关闭当前标签页") },
        { QStringLiteral("&Close Current Tab"), QStringLiteral("关闭当前标签页") },
        { QStringLiteral("Open Next Tab"), QStringLiteral("切换到下一个标签页") },
        { QStringLiteral("Open &Next Tab"), QStringLiteral("切换到下一个标签页") },
        { QStringLiteral("Open Previous Tab"), QStringLiteral("切换到上一个标签页") },
        { QStringLiteral("Open &Previous Tab"), QStringLiteral("切换到上一个标签页") },
        { QStringLiteral("Open File..."), QStringLiteral("打开文件…") },
        { QStringLiteral("&Open File..."), QStringLiteral("打开文件…") },
        { QStringLiteral("Find in Page..."), QStringLiteral("在页面中查找…") },
        { QStringLiteral("&Find in Page..."), QStringLiteral("在页面中查找…") },
        { QStringLiteral("Quit"), QStringLiteral("退出") },
        { QStringLiteral("&Quit"), QStringLiteral("退出") },
        { QStringLiteral("Thoth"), QStringLiteral("Thoth") },
        { QStringLiteral("Downloads"), QStringLiteral("下载") },
        { QStringLiteral("Settings"), QStringLiteral("设置") },
        { QStringLiteral("About Thoth Browser"), QStringLiteral("关于 Thoth Browser") },
        { QStringLiteral("Zoom"), QStringLiteral("缩放") },
        { QStringLiteral("Zoom In"), QStringLiteral("放大") },
        { QStringLiteral("Zoom Out"), QStringLiteral("缩小") },
        { QStringLiteral("Reset Zoom"), QStringLiteral("重置缩放") },
        { QStringLiteral("Zoom out"), QStringLiteral("缩小") },
        { QStringLiteral("Reset zoom level"), QStringLiteral("重置缩放") },
        { QStringLiteral("Zoom in"), QStringLiteral("放大") },
        { QStringLiteral("Undo"), QStringLiteral("撤销") },
        { QStringLiteral("Redo"), QStringLiteral("重做") },
        { QStringLiteral("Cut"), QStringLiteral("剪切") },
        { QStringLiteral("Copy"), QStringLiteral("复制") },
        { QStringLiteral("Paste"), QStringLiteral("粘贴") },
        { QStringLiteral("Select All"), QStringLiteral("全选") },
        { QStringLiteral("Reload"), QStringLiteral("重新加载") },
        { QStringLiteral("Reload current page"), QStringLiteral("重新加载当前页面") },
        { QStringLiteral("Go Back"), QStringLiteral("后退") },
        { QStringLiteral("Go Forward"), QStringLiteral("前进") },
        { QStringLiteral("Bookmark this page"), QStringLiteral("为此页面添加书签") },
        { QStringLiteral("Enter web address"), QStringLiteral("输入网址") },
        { QStringLiteral("Show Menu"), QStringLiteral("显示菜单") },
        { QStringLiteral("Show &Menu"), QStringLiteral("显示菜单") },
        { QStringLiteral("Close Tab"), QStringLiteral("关闭标签页") },
        { QStringLiteral("Restore"), QStringLiteral("还原") },
        { QStringLiteral("Maximize"), QStringLiteral("最大化") },
        { QStringLiteral("Edit Location"), QStringLiteral("编辑地址") },
        { QStringLiteral("Toggle Sidebar"), QStringLiteral("切换侧边栏") },
        { QStringLiteral("Previous Workspace"), QStringLiteral("上一个工作区") },
        { QStringLiteral("Next Workspace"), QStringLiteral("下一个工作区") },
        { QStringLiteral("Toggle Split View"), QStringLiteral("切换分屏视图") },
        { QStringLiteral("New Workspace"), QStringLiteral("新建工作区") },
        { QStringLiteral("New Workspace…"), QStringLiteral("新建工作区…") },
        { QStringLiteral("Workspaces"), QStringLiteral("工作区") },
        { QStringLiteral("Workspace settings"), QStringLiteral("工作区设置") },
        { QStringLiteral("Workspace name"), QStringLiteral("工作区名称") },
        { QStringLiteral("Custom…"), QStringLiteral("自定义…") },
        { QStringLiteral("Choose Workspace Color"), QStringLiteral("选择工作区颜色") },
        { QStringLiteral("Browser, pages, and workspaces"), QStringLiteral("浏览器、页面与工作区") },
        { QStringLiteral("Browser and sidebar actions"), QStringLiteral("浏览器与侧边栏操作") },
        { QStringLiteral("Pages and Folders"), QStringLiteral("页面和文件夹") },
        { QStringLiteral("Show Sidebar"), QStringLiteral("显示侧边栏") },
        { QStringLiteral("Appearance"), QStringLiteral("外观") },
        { QStringLiteral("Follow System"), QStringLiteral("跟随系统") },
        { QStringLiteral("Follow system"), QStringLiteral("跟随系统") },
        { QStringLiteral("Light"), QStringLiteral("浅色") },
        { QStringLiteral("Dark"), QStringLiteral("深色") },
        { QStringLiteral("Auto"), QStringLiteral("自动") },
        { QStringLiteral("Organize Pages"), QStringLiteral("整理页面") },
        { QStringLiteral("New Folder…"), QStringLiteral("新建文件夹…") },
        { QStringLiteral("New Folder"), QStringLiteral("新建文件夹") },
        { QStringLiteral("Folder name"), QStringLiteral("文件夹名称") },
        { QStringLiteral("Create"), QStringLiteral("创建") },
        { QStringLiteral("Save"), QStringLiteral("保存") },
        { QStringLiteral("Cancel"), QStringLiteral("取消") },
        { QStringLiteral("OK"), QStringLiteral("确定") },
        { QStringLiteral("Open"), QStringLiteral("打开") },
        { QStringLiteral("Move to Favourites"), QStringLiteral("移至收藏夹") },
        { QStringLiteral("Pin Page"), QStringLiteral("固定页面") },
        { QStringLiteral("Unpin to Temporary"), QStringLiteral("取消固定并移至临时页面") },
        { QStringLiteral("Move to Folder"), QStringLiteral("移至文件夹") },
        { QStringLiteral("Swap Left and Right"), QStringLiteral("交换左右页面") },
        { QStringLiteral("Separate Pages"), QStringLiteral("分离页面") },
        { QStringLiteral("Close Pane"), QStringLiteral("关闭窗格") },
        { QStringLiteral("Close Left Pane"), QStringLiteral("关闭左侧窗格") },
        { QStringLiteral("Close Right Pane"), QStringLiteral("关闭右侧窗格") },
        { QStringLiteral("Close This Pane"), QStringLiteral("关闭此窗格") },
        { QStringLiteral("Create Split View…"), QStringLiteral("创建分屏视图…") },
        { QStringLiteral("Split with New Page"), QStringLiteral("与新页面分屏") },
        { QStringLiteral("Close"), QStringLiteral("关闭") },
        { QStringLiteral("Unload Page (Keep Pinned)"), QStringLiteral("卸载页面（保留固定状态）") },
        { QStringLiteral("Rename Folder…"), QStringLiteral("重命名文件夹…") },
        { QStringLiteral("Rename Folder"), QStringLiteral("重命名文件夹") },
        { QStringLiteral("Remove Folder, Keep Pages"), QStringLiteral("移除文件夹并保留页面") },
        { QStringLiteral("Move to Workspace"), QStringLiteral("移至工作区") },
        { QStringLiteral("Reopen Closed Tab"), QStringLiteral("重新打开已关闭的标签页") },
        { QStringLiteral("Rename Workspace…"), QStringLiteral("重命名工作区…") },
        { QStringLiteral("Rename Workspace"), QStringLiteral("重命名工作区") },
        { QStringLiteral("Color"), QStringLiteral("颜色") },
        { QStringLiteral("Lavender"), QStringLiteral("薰衣草") },
        { QStringLiteral("Sage"), QStringLiteral("鼠尾草") },
        { QStringLiteral("Rose"), QStringLiteral("玫瑰") },
        { QStringLiteral("Sand"), QStringLiteral("砂岩") },
        { QStringLiteral("Blue"), QStringLiteral("蓝色") },
        { QStringLiteral("Graphite"), QStringLiteral("石墨") },
        { QStringLiteral("Icon"), QStringLiteral("图标") },
        { QStringLiteral("Move Left"), QStringLiteral("左移") },
        { QStringLiteral("Move Right"), QStringLiteral("右移") },
        { QStringLiteral("Remove Workspace, Keep Pages"), QStringLiteral("移除工作区并保留页面") },
        { QStringLiteral("Duplicate Tab"), QStringLiteral("复制标签页") },
        { QStringLiteral("Move Tab"), QStringLiteral("移动标签页") },
        { QStringLiteral("Close Multiple Tabs"), QStringLiteral("关闭多个标签页") },
        { QStringLiteral("Close Tabs to the Left"), QStringLiteral("关闭左侧标签页") },
        { QStringLiteral("Close Tabs to the Right"), QStringLiteral("关闭右侧标签页") },
        { QStringLiteral("Close Other Tabs"), QStringLiteral("关闭其他标签页") },
        { QStringLiteral("Close Tabs to Left"), QStringLiteral("关闭左侧标签页") },
        { QStringLiteral("Close Tabs to Right"), QStringLiteral("关闭右侧标签页") },
        { QStringLiteral("Move to Start"), QStringLiteral("移至开头") },
        { QStringLiteral("Move to End"), QStringLiteral("移至末尾") },
        { QStringLiteral("Color Scheme"), QStringLiteral("配色方案") },
        { QStringLiteral("Contrast"), QStringLiteral("对比度") },
        { QStringLiteral("Motion"), QStringLiteral("动态效果") },
        { QStringLiteral("Less"), QStringLiteral("较少") },
        { QStringLiteral("More"), QStringLiteral("较多") },
        { QStringLiteral("No Preference"), QStringLiteral("无偏好") },
        { QStringLiteral("Show Menu Bar"), QStringLiteral("显示菜单栏") },
        { QStringLiteral("Toggle Vertical Tabs Expanded"), QStringLiteral("展开或收起垂直标签栏") },
        { QStringLiteral("Manage Bookmarks"), QStringLiteral("管理书签") },
        { QStringLiteral("Toggle Bookmark"), QStringLiteral("切换书签") },
        { QStringLiteral("Add Bookmark"), QStringLiteral("添加书签") },
        { QStringLiteral("Remove Bookmark"), QStringLiteral("移除书签") },
        { QStringLiteral("Bookmark All Tabs..."), QStringLiteral("为所有标签页添加书签…") },
        { QStringLiteral("Show Bookmarks Bar"), QStringLiteral("显示书签栏") },
        { QStringLiteral("View History"), QStringLiteral("查看历史记录") },
        { QStringLiteral("Clear Browsing Data"), QStringLiteral("清除浏览数据") },
        { QStringLiteral("View Source"), QStringLiteral("查看源代码") },
        { QStringLiteral("Enable DevTools"), QStringLiteral("启用开发者工具") },
        { QStringLiteral("Disable DevTools"), QStringLiteral("停用开发者工具") },
        { QStringLiteral("Reduce"), QStringLiteral("减少动态效果") },
        { QStringLiteral("Show Advanced Debug Menu"), QStringLiteral("显示高级调试菜单") },
        { QStringLiteral("Show Tab Performance Monitor"), QStringLiteral("显示标签页性能监视器") },
        { QStringLiteral("Show WebContent process ID in tab titles"), QStringLiteral("在标签页标题中显示网页内容进程 ID") },
        { QStringLiteral("Dump Session History Tree"), QStringLiteral("转储会话历史树") },
        { QStringLiteral("Dump DOM Tree"), QStringLiteral("转储 DOM 树") },
        { QStringLiteral("Dump Layout Tree"), QStringLiteral("转储布局树") },
        { QStringLiteral("Dump Stacking Context Tree"), QStringLiteral("转储层叠上下文树") },
        { QStringLiteral("Dump Site Isolation Process Tree"), QStringLiteral("转储站点隔离进程树") },
        { QStringLiteral("Dump Display List"), QStringLiteral("转储显示列表") },
        { QStringLiteral("Dump Style Sheets"), QStringLiteral("转储样式表") },
        { QStringLiteral("Dump All Resolved Styles"), QStringLiteral("转储所有已解析样式") },
        { QStringLiteral("Dump CSS Errors"), QStringLiteral("转储 CSS 错误") },
        { QStringLiteral("Dump Cookies"), QStringLiteral("转储 Cookie") },
        { QStringLiteral("Dump Local Storage"), QStringLiteral("转储本地存储") },
        { QStringLiteral("Dump Session Storage"), QStringLiteral("转储会话存储") },
        { QStringLiteral("Dump WASM Stats"), QStringLiteral("转储 WASM 统计信息") },
        { QStringLiteral("Collect Garbage"), QStringLiteral("执行垃圾回收") },
        { QStringLiteral("Crash Current Page"), QStringLiteral("使当前页面崩溃") },
        { QStringLiteral("Crash Compositor Process"), QStringLiteral("使合成器进程崩溃") },
        { QStringLiteral("Spoof User Agent"), QStringLiteral("模拟用户代理") },
        { QStringLiteral("Navigator Compatibility Mode"), QStringLiteral("Navigator 兼容模式") },
        { QStringLiteral("Enable Scripting"), QStringLiteral("启用脚本") },
        { QStringLiteral("Block Pop-ups"), QStringLiteral("拦截弹出窗口") },
        { QStringLiteral("Show Line Box Borders"), QStringLiteral("显示行框边界") },
        { QStringLiteral("Show Caret Hit Test Debug Overlay"), QStringLiteral("显示插入符命中测试调试覆盖层") },
        { QStringLiteral("Open Task Manager"), QStringLiteral("打开任务管理器") },
        { QStringLiteral("Private"), QStringLiteral("无痕") },
        { QStringLiteral("Close all private windows and start a fresh private browsing session"), QStringLiteral("关闭所有无痕窗口并开始新的无痕浏览会话") },
        { QStringLiteral("Start a fresh private session?"), QStringLiteral("要开始新的无痕会话吗？") },
        { QStringLiteral("This closes all private windows and deletes history and other site data from the current private browsing session."), QStringLiteral("这会关闭所有无痕窗口，并删除当前无痕会话中的历史记录和其他网站数据。") },
        { QStringLiteral("Restart Private Session"), QStringLiteral("重新开始无痕会话") },
        { QStringLiteral("Allow"), QStringLiteral("允许") },
        { QStringLiteral("Block"), QStringLiteral("阻止") },
        { QStringLiteral("Pause"), QStringLiteral("暂停") },
        { QStringLiteral("Resume"), QStringLiteral("继续") },
        { QStringLiteral("The browser is ready."), QStringLiteral("浏览器已就绪。") },
        { QStringLiteral("Open folder"), QStringLiteral("打开文件夹") },
        { QStringLiteral("Downloads are still in progress."), QStringLiteral("仍有下载任务正在进行。") },
        { QStringLiteral("Quitting will cancel downloads that cannot be resumed."), QStringLiteral("退出将取消无法恢复的下载任务。") },
        { QStringLiteral("Quit and Cancel Downloads"), QStringLiteral("退出并取消下载") },
        { QStringLiteral("Select save location"), QStringLiteral("选择保存位置") },
        { QStringLiteral("About"), QStringLiteral("关于") },
        { QStringLiteral("Version"), QStringLiteral("版本") },
        { QStringLiteral("Build details"), QStringLiteral("构建信息") },
        { QStringLiteral("Commit"), QStringLiteral("提交版本") },
        { QStringLiteral("Architecture"), QStringLiteral("架构") },
        { QStringLiteral("Operating system"), QStringLiteral("操作系统") },
        { QStringLiteral("User agent"), QStringLiteral("用户代理") },
        { QStringLiteral("Runtime"), QStringLiteral("运行环境") },
        { QStringLiteral("Command line"), QStringLiteral("命令行") },
        { QStringLiteral("Application path"), QStringLiteral("应用程序路径") },
        { QStringLiteral("Search or enter a web address"), QStringLiteral("搜索或输入网址") },
        { QStringLiteral("Press Enter to open · Esc to dismiss"), QStringLiteral("按 Enter 打开 · 按 Esc 关闭") },
        { QStringLiteral("NEW PAGE"), QStringLiteral("新页面") },
        { QStringLiteral("Show Sidebar · ⌘⇧L"), QStringLiteral("显示侧边栏 · ⌘⇧L") },
    };
    return translations;
}

QString localized_text(QString const& source_text)
{
    if (!is_simplified_chinese())
        return source_text;

    auto normalized = source_text;
    normalized.remove('&');
    if (auto it = simplified_chinese_translations().find(normalized); it != simplified_chinese_translations().end())
        return it.value();

    if (normalized.startsWith("Workspace ")) {
        auto suffix = normalized.sliced(QStringLiteral("Workspace ").size());
        if (!suffix.isEmpty() && suffix[0].isDigit())
            return QStringLiteral("工作区 ") + suffix;
        return QStringLiteral("工作区 ") + suffix;
    }
    if (normalized.startsWith("Toggle Sidebar ("))
        return QStringLiteral("切换侧边栏 (") + normalized.sliced(QStringLiteral("Toggle Sidebar (").size());
    if (normalized.startsWith("Show Sidebar · "))
        return QStringLiteral("显示侧边栏 · ") + normalized.sliced(QStringLiteral("Show Sidebar · ").size());
    if (normalized.endsWith(" · Workspace settings"))
        return normalized.left(normalized.size() - QStringLiteral(" · Workspace settings").size()) + QStringLiteral(" · 工作区设置");
    return source_text;
}

static QString translated_property(QObject& object, char const* source_property, char const* result_property, QString const& current_text)
{
    auto stored_source = object.property(source_property);
    auto last_result = object.property(result_property);
    auto source_text = stored_source.isValid() ? stored_source.toString() : current_text;
    if (stored_source.isValid() && current_text != last_result.toString())
        source_text = current_text;

    auto result = localized_text(source_text);
    object.setProperty(source_property, source_text);
    object.setProperty(result_property, result);
    return result;
}

void localize_action(QAction& action)
{
    action.setText(translated_property(action, "ThothSourceActionText", "ThothLocalizedActionText", action.text()));
    action.setToolTip(translated_property(action, "ThothSourceActionToolTip", "ThothLocalizedActionToolTip", action.toolTip()));
}

void localize_menu(QMenu& menu)
{
    menu.setTitle(translated_property(menu, "ThothSourceMenuTitle", "ThothLocalizedMenuTitle", menu.title()));
    for (auto* action : menu.actions()) {
        localize_action(*action);
        if (auto* submenu = action->menu())
            localize_menu(*submenu);
    }
}

static void localize_widget(QWidget& widget)
{
    if (auto* menu = qobject_cast<QMenu*>(&widget))
        localize_menu(*menu);

    if (auto* button = qobject_cast<QAbstractButton*>(&widget))
        button->setText(translated_property(widget, "ThothSourceText", "ThothLocalizedText", button->text()));
    if (auto* label = qobject_cast<QLabel*>(&widget))
        label->setText(translated_property(widget, "ThothSourceText", "ThothLocalizedText", label->text()));
    if (auto* group = qobject_cast<QGroupBox*>(&widget))
        group->setTitle(translated_property(widget, "ThothSourceText", "ThothLocalizedText", group->title()));
    if (auto* line_edit = qobject_cast<QLineEdit*>(&widget))
        line_edit->setPlaceholderText(translated_property(widget, "ThothSourcePlaceholder", "ThothLocalizedPlaceholder", line_edit->placeholderText()));
    if (!widget.windowTitle().isEmpty())
        widget.setWindowTitle(translated_property(widget, "ThothSourceWindowTitle", "ThothLocalizedWindowTitle", widget.windowTitle()));
    if (!widget.toolTip().isEmpty())
        widget.setToolTip(translated_property(widget, "ThothSourceToolTip", "ThothLocalizedToolTip", widget.toolTip()));
    if (!widget.accessibleName().isEmpty())
        widget.setAccessibleName(translated_property(widget, "ThothSourceAccessibleName", "ThothLocalizedAccessibleName", widget.accessibleName()));
    if (!widget.accessibleDescription().isEmpty())
        widget.setAccessibleDescription(translated_property(widget, "ThothSourceAccessibleDescription", "ThothLocalizedAccessibleDescription", widget.accessibleDescription()));
    for (auto* action : widget.actions())
        localize_action(*action);
    if (auto* tab_widget = qobject_cast<QTabWidget*>(&widget)) {
        for (int i = 0; i < tab_widget->count(); ++i) {
            auto source_property = QByteArray("ThothSourceTabText_") + QByteArray::number(i);
            auto result_property = QByteArray("ThothLocalizedTabText_") + QByteArray::number(i);
            tab_widget->setTabText(i, translated_property(*tab_widget, source_property.constData(), result_property.constData(), tab_widget->tabText(i)));
        }
    }
    if (auto* tab_bar = qobject_cast<QTabBar*>(&widget)) {
        for (int i = 0; i < tab_bar->count(); ++i) {
            auto source_property = QByteArray("ThothSourceTabText_") + QByteArray::number(i);
            auto result_property = QByteArray("ThothLocalizedTabText_") + QByteArray::number(i);
            tab_bar->setTabText(i, translated_property(*tab_bar, source_property.constData(), result_property.constData(), tab_bar->tabText(i)));
        }
    }
}

void localize_widget_tree(QWidget& root)
{
    localize_widget(root);
    for (auto* child : root.findChildren<QWidget*>())
        localize_widget(*child);
}

void localize_application_ui()
{
    for (auto* widget : QApplication::allWidgets())
        localize_widget(*widget);
    for (auto* action : qApp->findChildren<QAction*>())
        localize_action(*action);
}

}
