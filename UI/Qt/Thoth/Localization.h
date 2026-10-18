/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

class QAction;
class QMenu;
class QWidget;
class QString;

namespace Thoth {

QString localized_text(QString const& source_text);
void localize_action(QAction&);
void localize_menu(QMenu&);
void localize_widget_tree(QWidget&);
void localize_application_ui();

}
