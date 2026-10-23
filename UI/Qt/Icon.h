/*
 * Copyright (c) 2023, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/Span.h>
#include <AK/StringView.h>

#include <QIcon>
#include <QSize>

class QPalette;
class QColor;

namespace Ladybird {

enum class ChromeIcon {
    Back,
    Forward,
    Reload,
    Stop,
    NewTab,
    Close,
    TabClose,
    Menu,
    Star,
    StarFilled,
    Search,
    Globe,
    Unlock,
    Folder,
    Download,
    DownloadActive,
    Volume,
    VolumeMuted,
    ChevronUp,
    ChevronDown,
    VerticalTabBarCollapse,
    VerticalTabBarCollapseRight,
    VerticalTabBarExpand,
    VerticalTabBarExpandRight,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowClose,
    Lock,
    ChevronRight,
    Book,
    Diamond,
    Orbit,
    Shield,
    Settings,
    History,
    Home,
    Copy,
    Link,
    ExternalLink,
    Pin,
    Unpin,
    Sleep,
    Trash,
    Split,
    Camera,
    Microphone,
    File,
    Info,
    FolderPlus,
    Update,
    Help,

};

constexpr inline auto ICON_DEVICE_PIXEL_RATIOS = to_array({ 1, 2, 3 });

QIcon load_icon_from_uri(StringView);
QIcon icon_from_png(ReadonlyBytes, int logical_size);
QPixmap library_icon_pixmap(QString const& name, QColor, QSize logical_size, qreal device_pixel_ratio);
QIcon create_chrome_icon(ChromeIcon, QPalette const&);
QIcon loading_spinner_icon(QPalette const& palette, int frame);

QSize physical_size_for_device_pixel_ratio(QSize size, qreal device_pixel_ratio);

}
