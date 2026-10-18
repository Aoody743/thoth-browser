/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <QIcon>
#include <QString>

namespace Thoth {

void apply_browser_ui_appearance(QString const& font_id, QString const& palette_id, QString const& icon_weight, QString const& color_scheme = "system");
QString browser_palette_accent(QString const& palette_id, bool dark);
QIcon brand_logo_icon(bool dark);

}
