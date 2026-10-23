/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#include "Updater.h"
#include "Localization.h"
#include <QMenu>
namespace Thoth {
void add_update_menu(QMenu& menu, std::function<bool()>, std::function<void()>)
{
    auto* action = menu.addAction(localized_text("Automatic updates are currently available on macOS."));
    action->setEnabled(false);
}
}
