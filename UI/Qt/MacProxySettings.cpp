/*
 * Copyright (c) 2026, Thoth Browser contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/MacProxySettings.h>

#include <AK/StringBuilder.h>
#include <cstring>

namespace Ladybird {

static bool setting_is_enabled(CFDictionaryRef settings, CFStringRef key)
{
    auto value = static_cast<CFTypeRef>(CFDictionaryGetValue(settings, key));
    if (!value)
        return false;
    if (CFGetTypeID(value) == CFBooleanGetTypeID())
        return CFBooleanGetValue(static_cast<CFBooleanRef>(value));
    if (CFGetTypeID(value) != CFNumberGetTypeID())
        return false;

    int enabled = 0;
    return CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &enabled) && enabled != 0;
}

static Optional<ByteString> string_setting(CFDictionaryRef settings, CFStringRef key)
{
    auto value = static_cast<CFTypeRef>(CFDictionaryGetValue(settings, key));
    if (!value || CFGetTypeID(value) != CFStringGetTypeID())
        return {};

    char buffer[4096];
    if (!CFStringGetCString(static_cast<CFStringRef>(value), buffer, sizeof(buffer), kCFStringEncodingUTF8))
        return {};
    return ByteString(buffer);
}

static Optional<u16> port_setting(CFDictionaryRef settings, CFStringRef key)
{
    auto value = static_cast<CFTypeRef>(CFDictionaryGetValue(settings, key));
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID())
        return {};

    int port = 0;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberIntType, &port) || port <= 0 || port > 65535)
        return {};
    return static_cast<u16>(port);
}

static Optional<HTTP::Proxy> proxy_from_dictionary(CFDictionaryRef dictionary)
{
    auto type = static_cast<CFTypeRef>(CFDictionaryGetValue(dictionary, kCFProxyTypeKey));
    if (!type || CFGetTypeID(type) != CFStringGetTypeID())
        return {};

    StringView scheme;
    if (CFEqual(type, kCFProxyTypeHTTP))
        scheme = "http"sv;
    else if (CFEqual(type, kCFProxyTypeHTTPS))
        scheme = "https"sv;
    else if (CFEqual(type, kCFProxyTypeSOCKS))
        scheme = "socks5h"sv;
    else
        return {};

    auto host = string_setting(dictionary, kCFProxyHostNameKey);
    auto port = port_setting(dictionary, kCFProxyPortNumberKey);
    if (!host.has_value() || host->is_empty() || !port.has_value())
        return {};

    StringBuilder url;
    url.append(scheme);
    url.append("://"sv);
    if (host->contains(':'))
        url.appendff("[{}]", *host);
    else
        url.append(*host);
    url.appendff(":{}", *port);
    return HTTP::Proxy::parse(url.to_byte_string());
}

static Vector<HTTP::Proxy> ordered_proxies_for_url(CFDictionaryRef settings, CFURLRef url)
{
    Vector<HTTP::Proxy> proxies;
    auto system_proxies = CFNetworkCopyProxiesForURL(url, settings);
    if (!system_proxies)
        return proxies;

    auto array = system_proxies;
    for (CFIndex index = 0; index < CFArrayGetCount(array); ++index) {
        auto item = static_cast<CFTypeRef>(CFArrayGetValueAtIndex(array, index));
        if (!item || CFGetTypeID(item) != CFDictionaryGetTypeID())
            continue;
        if (auto proxy = proxy_from_dictionary(static_cast<CFDictionaryRef>(item)); proxy.has_value())
            proxies.append(proxy.release_value());
    }
    CFRelease(system_proxies);
    return proxies;
}

static void split_proxy_chain(Vector<HTTP::Proxy> proxies, Optional<HTTP::Proxy>& primary, Vector<HTTP::Proxy>& fallbacks)
{
    if (proxies.is_empty())
        return;
    primary = proxies.take_first();
    fallbacks = move(proxies);
}

static Vector<ByteString> proxy_exceptions(CFDictionaryRef settings)
{
    Vector<ByteString> exclusions;
    auto value = static_cast<CFTypeRef>(CFDictionaryGetValue(settings, kCFNetworkProxiesExceptionsList));
    if (!value || CFGetTypeID(value) != CFArrayGetTypeID())
        return exclusions;

    auto array = static_cast<CFArrayRef>(value);
    for (CFIndex index = 0; index < CFArrayGetCount(array); ++index) {
        auto item = static_cast<CFTypeRef>(CFArrayGetValueAtIndex(array, index));
        if (!item || CFGetTypeID(item) != CFStringGetTypeID())
            continue;
        char buffer[4096];
        if (CFStringGetCString(static_cast<CFStringRef>(item), buffer, sizeof(buffer), kCFStringEncodingUTF8))
            exclusions.append(ByteString(buffer).to_lowercase());
    }
    return exclusions;
}

MacProxySettings mac_proxy_settings_from_dictionary(CFDictionaryRef settings)
{
    if (!settings)
        return {};

    auto http_url = CFURLCreateWithString(kCFAllocatorDefault, CFSTR("http://thoth-proxy-resolution.invalid/"), nullptr);
    auto https_url = CFURLCreateWithString(kCFAllocatorDefault, CFSTR("https://thoth-proxy-resolution.invalid/"), nullptr);
    auto http_proxies = ordered_proxies_for_url(settings, http_url);
    auto https_proxies = ordered_proxies_for_url(settings, https_url);
    CFRelease(http_url);
    CFRelease(https_url);

    Optional<HTTP::Proxy> http_proxy;
    Optional<HTTP::Proxy> https_proxy;
    Vector<HTTP::Proxy> http_fallbacks;
    Vector<HTTP::Proxy> https_fallbacks;
    split_proxy_chain(move(http_proxies), http_proxy, http_fallbacks);
    split_proxy_chain(move(https_proxies), https_proxy, https_fallbacks);

    auto exclude_simple_hostnames = setting_is_enabled(settings, kCFNetworkProxiesExcludeSimpleHostnames);
    auto configuration = HTTP::ProxyConfiguration {
        move(http_proxy),
        move(https_proxy),
        proxy_exceptions(settings),
        exclude_simple_hostnames,
        move(http_fallbacks),
        move(https_fallbacks),
    };

    return { move(configuration) };
}

MacProxySettings current_mac_proxy_settings()
{
    auto settings = CFNetworkCopySystemProxySettings();
    if (!settings)
        return {};
    auto result = mac_proxy_settings_from_dictionary(settings);
    CFRelease(settings);
    return result;
}

}
