/*
 * Copyright (c) 2023, Tim Flynn <trflynn89@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibCore/Resource.h>
#include <UI/Qt/ChromeStyle.h>
#include <UI/Qt/Icon.h>
#include <UI/Qt/StringUtils.h>

#include <QPainter>
#include <QGuiApplication>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPixmapCache>
#include <QPointF>
#include <QRectF>

namespace Ladybird {

QIcon load_icon_from_uri(StringView uri)
{
    auto resource = MUST(Core::Resource::load_from_uri(uri));
    auto path = qstring_from_ak_string(resource->filesystem_path());

    return QIcon { path };
}

QIcon icon_from_png(ReadonlyBytes favicon_png, int logical_size)
{
    QPixmap pixmap;
    if (!pixmap.loadFromData(favicon_png.data(), static_cast<uint>(favicon_png.size()), "PNG"))
        return {};

    QIcon icon;
    for (auto device_pixel_ratio : ICON_DEVICE_PIXEL_RATIOS) {
        auto size = logical_size * device_pixel_ratio;
        auto scaled_pixmap = pixmap.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        scaled_pixmap.setDevicePixelRatio(device_pixel_ratio);
        icon.addPixmap(scaled_pixmap);
    }
    return icon;
}

static QPixmap create_transparent_icon_pixmap(QSize logical_size, qreal device_pixel_ratio)
{
    QPixmap pixmap(physical_size_for_device_pixel_ratio(logical_size, device_pixel_ratio));
    pixmap.setDevicePixelRatio(device_pixel_ratio);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

QPixmap library_icon_pixmap(QString const& name, QColor color, QSize logical_size, qreal device_pixel_ratio)
{
    auto weight = qApp ? qApp->property("ThothIconWeight").toString() : QString {};
    if (weight != "fine" && weight != "expressive")
        weight = "regular";
    auto cache_key = QString("ThothLucide/%1/%2/%3/%4/%5x%6")
                         .arg(name, weight, color.name(QColor::HexArgb))
                         .arg(device_pixel_ratio).arg(logical_size.width()).arg(logical_size.height());
    QPixmap cached;
    if (QPixmapCache::find(cache_key, &cached))
        return cached;
    QPixmap source(QString(":/Thoth/Icons/lucide/%1-%2.png").arg(name, weight));
    if (source.isNull()) {
        qWarning("Missing bundled Lucide icon: %s", qPrintable(name));
        return {};
    }
    // Tint the upstream asset's alpha mask; no icon geometry is generated here.
    QPainter tint(&source);
    tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tint.fillRect(source.rect(), color);
    tint.end();
    auto pixmap = create_transparent_icon_pixmap(logical_size, device_pixel_ratio);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(QRect(QPoint(), logical_size), source);
    painter.end();
    QPixmapCache::insert(cache_key, pixmap);
    return pixmap;
}

static QString chrome_icon_name(ChromeIcon icon)
{
    switch (icon) {
    case ChromeIcon::Back: return QStringLiteral("arrow-left");
    case ChromeIcon::Forward: return QStringLiteral("arrow-right");
    case ChromeIcon::Reload: return QStringLiteral("rotate-cw");
    case ChromeIcon::Stop: return QStringLiteral("x");
    case ChromeIcon::NewTab: return QStringLiteral("plus");
    case ChromeIcon::Close: return QStringLiteral("x");
    case ChromeIcon::TabClose: return QStringLiteral("x");
    case ChromeIcon::Menu: return QStringLiteral("menu");
    case ChromeIcon::Star: return QStringLiteral("star");
    case ChromeIcon::StarFilled: return QStringLiteral("star-filled");
    case ChromeIcon::Search: return QStringLiteral("search");
    case ChromeIcon::Globe: return QStringLiteral("globe");
    case ChromeIcon::Unlock: return QStringLiteral("lock-keyhole-open");
    case ChromeIcon::Lock: return QStringLiteral("lock-keyhole");
    case ChromeIcon::Folder: return QStringLiteral("folder");
    case ChromeIcon::Download: return QStringLiteral("arrow-down-to-line");
    case ChromeIcon::DownloadActive: return QStringLiteral("arrow-down-to-line");
    case ChromeIcon::Volume: return QStringLiteral("volume-2");
    case ChromeIcon::VolumeMuted: return QStringLiteral("volume-x");
    case ChromeIcon::ChevronUp: return QStringLiteral("chevron-up");
    case ChromeIcon::ChevronDown: return QStringLiteral("chevron-down");
    case ChromeIcon::ChevronRight: return QStringLiteral("chevron-right");
    case ChromeIcon::VerticalTabBarCollapse: return QStringLiteral("panel-left-close");
    case ChromeIcon::VerticalTabBarCollapseRight: return QStringLiteral("panel-right-close");
    case ChromeIcon::VerticalTabBarExpand: return QStringLiteral("panel-left-open");
    case ChromeIcon::VerticalTabBarExpandRight: return QStringLiteral("panel-right-open");
    case ChromeIcon::WindowMinimize: return QStringLiteral("minus");
    case ChromeIcon::WindowMaximize: return QStringLiteral("square");
    case ChromeIcon::WindowRestore: return QStringLiteral("copy");
    case ChromeIcon::WindowClose: return QStringLiteral("x");
    case ChromeIcon::Book: return QStringLiteral("book-open");
    case ChromeIcon::Diamond: return QStringLiteral("diamond");
    case ChromeIcon::Orbit: return QStringLiteral("orbit");
    case ChromeIcon::Shield: return QStringLiteral("shield-check");
    case ChromeIcon::Settings: return QStringLiteral("settings");
    case ChromeIcon::History: return QStringLiteral("history");
    case ChromeIcon::Home: return QStringLiteral("house");
    case ChromeIcon::Copy: return QStringLiteral("copy");
    case ChromeIcon::Link: return QStringLiteral("link");
    case ChromeIcon::ExternalLink: return QStringLiteral("square-arrow-out-up-right");
    case ChromeIcon::Pin: return QStringLiteral("pin");
    case ChromeIcon::Unpin: return QStringLiteral("pin-off");
    case ChromeIcon::Sleep: return QStringLiteral("moon");
    case ChromeIcon::Trash: return QStringLiteral("trash-2");
    case ChromeIcon::Split: return QStringLiteral("square-split-horizontal");
    case ChromeIcon::Camera: return QStringLiteral("camera");
    case ChromeIcon::Microphone: return QStringLiteral("mic");
    case ChromeIcon::File: return QStringLiteral("file");
    case ChromeIcon::Info: return QStringLiteral("info");
    case ChromeIcon::FolderPlus: return QStringLiteral("folder-plus");
    case ChromeIcon::Update: return QStringLiteral("cloud-download");
    case ChromeIcon::Help: return QStringLiteral("circle-help");
    }
    Q_UNREACHABLE();
}

static QPixmap create_chrome_icon_pixmap(ChromeIcon icon, QColor color, qreal device_pixel_ratio)
{
    return library_icon_pixmap(chrome_icon_name(icon), color, { 20, 20 }, device_pixel_ratio);
}

QIcon create_chrome_icon(ChromeIcon icon, QPalette const& palette)
{
    auto normal = ChromeStyle::chrome_button_text(palette);
    auto normal_alpha = 216;
    auto warning = ChromeStyle::is_dark(palette) ? QColor(224, 142, 136) : QColor(144, 62, 56);
    if (icon == ChromeIcon::DownloadActive) {
        normal = ChromeStyle::chrome_accent(palette);
        normal_alpha = 244;
    } else if (icon == ChromeIcon::Unlock) {
        normal = warning;
        normal_alpha = 236;
    } else if (icon == ChromeIcon::Close || icon == ChromeIcon::TabClose)
        normal_alpha = 172;
    else if (icon == ChromeIcon::Star)
        normal_alpha = 204;
    else if (icon == ChromeIcon::Globe || icon == ChromeIcon::Search || icon == ChromeIcon::Menu)
        normal_alpha = 188;
    normal.setAlpha(normal_alpha);

    auto active = ChromeStyle::chrome_button_text(palette);
    active.setAlpha(icon == ChromeIcon::Close || icon == ChromeIcon::TabClose ? 220 : 236);
    if (icon == ChromeIcon::Unlock) {
        active = warning;
        active.setAlpha(248);
    }

    auto disabled = ChromeStyle::chrome_muted_text(palette);
    disabled.setAlpha(icon == ChromeIcon::Close || icon == ChromeIcon::TabClose ? 78 : 82);

    QIcon qicon;

    for (auto device_pixel_ratio : ICON_DEVICE_PIXEL_RATIOS) {
        qicon.addPixmap(create_chrome_icon_pixmap(icon, normal, device_pixel_ratio), QIcon::Normal);
        qicon.addPixmap(create_chrome_icon_pixmap(icon, active, device_pixel_ratio), QIcon::Active);
        qicon.addPixmap(create_chrome_icon_pixmap(icon, disabled, device_pixel_ratio), QIcon::Disabled);
        qicon.addPixmap(create_chrome_icon_pixmap(icon, active, device_pixel_ratio), QIcon::Selected);
    }

    return qicon;
}

static QPixmap create_loading_spinner_icon_pixmap(QPalette const& palette, int frame, int device_pixel_ratio)
{
    auto source = library_icon_pixmap("loader-circle", ChromeStyle::chrome_text(palette), { 16, 16 }, device_pixel_ratio);
    auto pixmap = create_transparent_icon_pixmap({ 16, 16 }, device_pixel_ratio);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.translate(8, 8);
    painter.rotate((frame % 12) * 30);
    painter.drawPixmap(QRect(-8, -8, 16, 16), source);

    return pixmap;
}

QIcon loading_spinner_icon(QPalette const& palette, int frame)
{
    QIcon icon;
    for (auto device_pixel_ratio : ICON_DEVICE_PIXEL_RATIOS)
        icon.addPixmap(create_loading_spinner_icon_pixmap(palette, frame, device_pixel_ratio));
    return icon;
}

QSize physical_size_for_device_pixel_ratio(QSize size, qreal device_pixel_ratio)
{
    return {
        static_cast<int>(ceil(size.width() * device_pixel_ratio)),
        static_cast<int>(ceil(size.height() * device_pixel_ratio)),
    };
}

}
