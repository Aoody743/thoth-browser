/*
 * Copyright (c) 2026, Thoth Browser contributors.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibURL/Parser.h>
#include <UI/Qt/MacProxySettings.h>

static void set_number(CFMutableDictionaryRef settings, CFStringRef key, int value)
{
    auto number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFDictionarySetValue(settings, key, number);
    CFRelease(number);
}
static void set_string(CFMutableDictionaryRef settings, CFStringRef key, char const* value)
{
    auto string = CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
    CFDictionarySetValue(settings, key, string);
    CFRelease(string);
}

TEST_CASE(system_proxy_settings_keep_ordered_fallbacks_and_bypass_rules)
{
    auto settings = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    set_number(settings, kCFNetworkProxiesHTTPEnable, 1);
    set_string(settings, kCFNetworkProxiesHTTPProxy, "primary-proxy.invalid");
    set_number(settings, kCFNetworkProxiesHTTPPort, 8080);
    set_number(settings, kCFNetworkProxiesHTTPSEnable, 1);
    set_string(settings, kCFNetworkProxiesHTTPSProxy, "secure-proxy.invalid");
    set_number(settings, kCFNetworkProxiesHTTPSPort, 8443);
    set_number(settings, kCFNetworkProxiesSOCKSEnable, 1);
    set_string(settings, kCFNetworkProxiesSOCKSProxy, "backup-proxy.invalid");
    set_number(settings, kCFNetworkProxiesSOCKSPort, 1080);
    set_number(settings, kCFNetworkProxiesExcludeSimpleHostnames, 1);

    auto exception = CFStringCreateWithCString(kCFAllocatorDefault, "*.internal.example", kCFStringEncodingUTF8);
    void const* exception_values[] { exception };
    auto exceptions = CFArrayCreate(kCFAllocatorDefault, exception_values, 1, &kCFTypeArrayCallBacks);
    CFDictionarySetValue(settings, kCFNetworkProxiesExceptionsList, exceptions);
    auto configured = Ladybird::mac_proxy_settings_from_dictionary(settings);
    auto http = configured.proxy_configuration.proxies_for(URL::Parser::basic_parse("http://www.example.com/"sv).release_value());
    auto https = configured.proxy_configuration.proxies_for(URL::Parser::basic_parse("https://www.example.com/"sv).release_value());
    EXPECT_EQ(http.size(), 2u);
    EXPECT_EQ(http[0].host, "primary-proxy.invalid"sv);
    EXPECT_EQ(http[0].port, 8080);
    EXPECT_EQ(http[1].host, "backup-proxy.invalid"sv);
    EXPECT_EQ(http[1].type, HTTP::Proxy::Type::SOCKS5Hostname);
    EXPECT_EQ(https.size(), 2u);
    EXPECT_EQ(https[0].host, "secure-proxy.invalid"sv);
    EXPECT_EQ(https[1].host, "backup-proxy.invalid"sv);
    EXPECT(configured.proxy_configuration.proxies_for(URL::Parser::basic_parse("http://printer/"sv).release_value()).is_empty());
    EXPECT(configured.proxy_configuration.proxies_for(URL::Parser::basic_parse("http://a.internal.example/"sv).release_value()).is_empty());
    CFRelease(exceptions);
    CFRelease(exception);
    CFRelease(settings);
}
