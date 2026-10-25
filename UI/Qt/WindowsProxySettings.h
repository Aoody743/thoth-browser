/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibHTTP/Proxy.h>

namespace Ladybird {

// Reads the WinINET proxy configuration (the one the Settings app and Clash-family proxies
// write) so the browser follows the system proxy like it does on macOS.
struct WindowsProxySettings {
    HTTP::ProxyConfiguration proxy_configuration;
};

WindowsProxySettings current_windows_proxy_settings();

}
