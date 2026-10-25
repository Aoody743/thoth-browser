/*
 * Copyright (c) 2026, Thoth Browser contributors.
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "WindowsProxySettings.h"

#include <AK/Platform.h>
#if defined(AK_OS_WINDOWS)

#include <AK/ByteString.h>
#include <AK/Utf16View.h>
#include <AK/Vector.h>
#include <LibHTTP/Proxy.h>

#include <AK/Windows.h>

#include <string>

namespace Ladybird {

static Optional<ByteString> registry_string(HKEY root, wchar_t const* path, wchar_t const* value_name)
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegGetValueW(root, path, value_name, RRF_RT_REG_SZ, &type, nullptr, &size) != ERROR_SUCCESS || size == 0)
        return {};
    std::wstring value(static_cast<size_t>(size) / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, path, value_name, RRF_RT_REG_SZ, &type, value.data(), &size) != ERROR_SUCCESS)
        return {};
    while (!value.empty() && value.back() == L'\0')
        value.pop_back();
    auto utf8 = Utf16View { reinterpret_cast<char16_t const*>(value.data()), value.size() }.to_utf8();
    if (utf8.is_error())
        return {};
    return ByteString { utf8.value().bytes_as_string_view() };
}

static Optional<HTTP::Proxy> proxy_from_host_port(StringView host_port)
{
    if (host_port.is_empty())
        return {};
    auto proxy = HTTP::Proxy::parse(host_port);
    if (proxy.has_value() && proxy->type == HTTP::Proxy::Type::HTTP && proxy->port == 0)
        proxy->port = 80;
    return proxy;
}

static void split_on_semicolon(StringView text, Function<void(StringView)> const& on_entry)
{
    size_t begin = 0;
    for (size_t i = 0; i <= text.length(); ++i) {
        if (i != text.length() && text[i] != ';')
            continue;
        on_entry(text.substring_view(begin, i - begin));
        begin = i + 1;
    }
}

// ProxyServer is either "host:port" or a scheme map like
// "http=host:port;https=host:port;ftp=host:port;<local>".
static void parse_proxy_server(StringView server, HTTP::Proxy& http, HTTP::Proxy& https)
{
    if (server.contains('=')) {
        split_on_semicolon(server, [&](StringView entry) {
            auto assignment = entry.find('=');
            if (!assignment.has_value())
                return;
            auto scheme = entry.substring_view(0, assignment.value());
            auto value = entry.substring_view(assignment.value() + 1);
            if (value.is_empty())
                return;
            if (scheme == "http"sv)
                http = proxy_from_host_port(value).value_or(http);
            else if (scheme == "https"sv)
                https = proxy_from_host_port(value).value_or(https);
        });
        return;
    }
    auto proxy = proxy_from_host_port(server);
    if (proxy.has_value()) {
        http = *proxy;
        https = *proxy;
    }
}

WindowsProxySettings current_windows_proxy_settings()
{
    WindowsProxySettings settings;
    constexpr auto internet_settings = L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";

    DWORD proxy_enabled = 0;
    DWORD type = 0;
    DWORD size = sizeof(proxy_enabled);
    if (RegGetValueW(HKEY_CURRENT_USER, internet_settings, L"ProxyEnable", RRF_RT_REG_DWORD, &type, &proxy_enabled, &size) != ERROR_SUCCESS)
        return settings;
    if (proxy_enabled == 0)
        return settings;

    auto proxy_server = registry_string(HKEY_CURRENT_USER, internet_settings, L"ProxyServer");
    if (!proxy_server.has_value())
        return settings;

    HTTP::Proxy http_proxy;
    HTTP::Proxy https_proxy;
    parse_proxy_server(*proxy_server, http_proxy, https_proxy);

    auto no_proxy_hosts = Vector<ByteString> {};
    auto exclude_simple_hostnames = false;
    if (auto override_list = registry_string(HKEY_CURRENT_USER, internet_settings, L"ProxyOverride"); override_list.has_value()) {
        split_on_semicolon(*override_list, [&](StringView entry) {
            if (entry.is_empty())
                return;
            if (entry == "<local>"sv) {
                exclude_simple_hostnames = true;
                return;
            }
            no_proxy_hosts.append(entry);
        });
    }

    settings.proxy_configuration = HTTP::ProxyConfiguration(
        http_proxy.host.is_empty() ? Optional<HTTP::Proxy> {} : Optional<HTTP::Proxy> { http_proxy },
        https_proxy.host.is_empty() ? Optional<HTTP::Proxy> {} : Optional<HTTP::Proxy> { https_proxy },
        move(no_proxy_hosts),
        exclude_simple_hostnames);
    return settings;
}

}

#endif
