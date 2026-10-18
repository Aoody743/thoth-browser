/*
 * Copyright (c) 2026, Thoth Browser contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <CFNetwork/CFNetwork.h>
#include <LibHTTP/Proxy.h>

namespace Ladybird {

struct MacProxySettings {
    HTTP::ProxyConfiguration proxy_configuration;
};

MacProxySettings mac_proxy_settings_from_dictionary(CFDictionaryRef);
MacProxySettings current_mac_proxy_settings();

}
