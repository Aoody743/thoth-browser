/* Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause */
#pragma once
#include <functional>
class QMenu;
namespace Thoth {
// The callback persists ordinary sessions and returns false while restarting is unsafe.
void add_update_menu(QMenu&, std::function<bool()> prepare_restart, std::function<void()> restart_cancelled);
}
