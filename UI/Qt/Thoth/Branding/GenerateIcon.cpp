/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

static QColor average_background_color(QImage const& source)
{
    constexpr int sample_size = 16;
    quint64 red = 0;
    quint64 green = 0;
    quint64 blue = 0;
    quint64 count = 0;
    for (int y = 0; y < sample_size; ++y) {
        for (int x = 0; x < sample_size; ++x) {
            for (auto [sample_x, sample_y] : {
                     std::pair { x, y },
                     std::pair { source.width() - sample_size + x, y },
                     std::pair { x, source.height() - sample_size + y },
                     std::pair { source.width() - sample_size + x, source.height() - sample_size + y },
                 }) {
                auto color = source.pixelColor(sample_x, sample_y);
                red += color.red();
                green += color.green();
                blue += color.blue();
                ++count;
            }
        }
    }
    return QColor(red / count, green / count, blue / count);
}

static QImage render_app_icon(QImage const& artwork, QColor background, int size)
{
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    constexpr qreal backplate_scale = 0.78;
    auto backplate_size = size * backplate_scale;
    auto backplate_inset = (size - backplate_size) / 2;
    QRectF backplate_rect(backplate_inset, backplate_inset, backplate_size, backplate_size);

    QPainterPath clip;
    auto radius = backplate_size * 0.23;
    clip.addRoundedRect(backplate_rect, radius, radius);
    painter.setClipPath(clip);
    painter.fillPath(clip, background);

    // Keep the emblem and rounded backplate inset together within the transparent canvas.
    constexpr qreal artwork_scale = 0.78;
    auto artwork_size = size * artwork_scale;
    auto inset = (size - artwork_size) / 2;
    painter.drawImage(QRectF(inset, inset, artwork_size, artwork_size), artwork);
    return image;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QString const root = "UI/Qt/Thoth/Branding";
    QImage dark_background_source(root + "/originals/thoth-dark.png");
    QImage light_background_source(root + "/originals/thoth-light.png");
    QImage dark_artwork(root + "/marks/thoth-mark-dark.png");
    QImage light_artwork(root + "/marks/thoth-mark-light.png");
    if (dark_background_source.isNull() || light_background_source.isNull() || dark_artwork.isNull() || light_artwork.isNull())
        return 1;

    auto dark_background = average_background_color(dark_background_source);
    auto light_background = average_background_color(light_background_source);
    QDir(root + "/app_icon.iconset").removeRecursively();
    QDir().mkpath(root + "/app_icon.iconset");

    if (!render_app_icon(dark_artwork, dark_background, 512).save("Base/res/icons/thoth-dark.png")
        || !render_app_icon(light_artwork, light_background, 512).save("Base/res/icons/thoth-light.png")
        || !render_app_icon(dark_artwork, dark_background, 512).save(root + "/thoth-dark.png")
        || !render_app_icon(light_artwork, light_background, 512).save(root + "/thoth-light.png")
        || !render_app_icon(dark_artwork, dark_background, 512).save("Base/res/icons/thoth.png")
        || !render_app_icon(dark_artwork, dark_background, 512).save(root + "/thoth.png"))
        return 1;

    for (int size : { 16, 32, 128, 256, 512 }) {
        auto base_name = QString("/app_icon.iconset/icon_%1x%1").arg(size);
        if (!render_app_icon(dark_artwork, dark_background, size).save(root + base_name + ".png")
            || !render_app_icon(dark_artwork, dark_background, size * 2).save(root + base_name + "@2x.png"))
            return 1;
    }
    return 0;
}
