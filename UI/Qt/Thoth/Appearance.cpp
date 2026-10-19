/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "Appearance.h"

#include <UI/Qt/ChromeStyle.h>
#include <QApplication>
#include <QColor>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

namespace Thoth {

struct PaletteChoice {
    char const* id;
    QColor light;
    QColor dark;
};

static PaletteChoice const PALETTES[] {
    { "lavender", QColor("#7866a6"), QColor("#c1aceb") },
    { "ocean", QColor("#4f778f"), QColor("#91c0d5") },
    { "sage", QColor("#577b66"), QColor("#9dc5a8") },
    { "rose", QColor("#9a657a"), QColor("#dca5b8") },
    { "sand", QColor("#92713f"), QColor("#dbba7c") },
    { "graphite", QColor("#687382"), QColor("#bdc6d3") },
    { "plum", QColor("#80577f"), QColor("#d0a6cf") },
    { "terracotta", QColor("#a65f48"), QColor("#e5a18a") },
};

static QString load_font_resource(QString const& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    auto application_font_id = QFontDatabase::addApplicationFontFromData(file.readAll());
    if (application_font_id < 0)
        return {};
    auto families = QFontDatabase::applicationFontFamilies(application_font_id);
    return families.isEmpty() ? QString {} : families.first();
}

static QString font_family_for_id(QString const& font_id)
{
    static bool fonts_loaded = false;
    static QString bricolage;
    static QString fraunces;
    static QString space_grotesk;
    if (!fonts_loaded) {
        bricolage = load_font_resource(":/Thoth/Branding/Fonts/BricolageGrotesque[opsz,wdth,wght].ttf");
        fraunces = load_font_resource(":/Thoth/Branding/Fonts/Fraunces[SOFT,WONK,opsz,wght].ttf");
        space_grotesk = load_font_resource(":/Thoth/Branding/Fonts/SpaceGrotesk[wght].ttf");
        fonts_loaded = true;
    }

    if (font_id == "bricolage")
        return bricolage;
    if (font_id == "fraunces")
        return fraunces;
    if (font_id == "space-grotesk")
        return space_grotesk;
    return {};
}

static QColor palette_accent(QString const& palette_id, bool dark)
{
    for (auto const& palette : PALETTES) {
        if (palette_id == QLatin1StringView(palette.id))
            return dark ? palette.dark : palette.light;
    }
    return {};
}

QString browser_palette_accent(QString const& palette_id, bool dark)
{
    auto accent = palette_accent(palette_id, dark);
    return accent.isValid() ? accent.name(QColor::HexRgb) : QString {};
}

QIcon brand_logo_icon(bool dark)
{
    static QIcon light_icon(QStringLiteral(":/Thoth/Branding/thoth-light.png"));
    static QIcon dark_icon(QStringLiteral(":/Thoth/Branding/thoth-dark.png"));
    return dark ? dark_icon : light_icon;
}

QColor chrome_background(bool dark)
{
    return QColor(dark ? "#211f28" : "#f5f4f8");
}

void apply_browser_ui_appearance(QString const& font_id, QString const& palette_id, QString const& icon_weight, QString const& color_scheme)
{
    auto* application = qApp;
    if (!application)
        return;

    static QFont system_font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    auto font = system_font;
    if (auto family = font_family_for_id(font_id); !family.isEmpty())
        font.setFamily(family);
    application->setFont(font);

    application->setProperty("ThothIconWeight", icon_weight);
    application->setProperty("ThothColorPalette", palette_id);

    static QPalette original_palette = application->palette();
    if (color_scheme == "dark")
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
    else if (color_scheme == "light")
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Light);
    else
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
    auto system_color_scheme = QGuiApplication::styleHints()->colorScheme();
    auto dark = system_color_scheme == Qt::ColorScheme::Dark || (system_color_scheme == Qt::ColorScheme::Unknown && original_palette.color(QPalette::Window).lightness() < 128);
    auto accent = palette_accent(palette_id, dark);
    if (!accent.isValid())
        accent = palette_accent("lavender", dark);

    QPalette palette = original_palette;
    auto window = QColor(dark ? "#1c1a21" : "#f6f3fa");
    auto base = QColor(dark ? "#15141a" : "#fffefd");
    auto text = QColor(dark ? "#e9e5ee" : "#302d36");
    auto muted = QColor(dark ? "#a8a2af" : "#77727f");
    palette.setColor(QPalette::Window, window);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, dark ? QColor("#1c1a21") : QColor("#f6f3fa"));
    palette.setColor(QPalette::Button, window);
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    palette.setColor(QPalette::Highlight, accent);
    palette.setColor(QPalette::HighlightedText, dark ? QColor("#241d2d") : QColor("#ffffff"));
    palette.setColor(QPalette::Link, accent);
    application->setPalette(palette);
}

}
