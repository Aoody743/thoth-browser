/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Function.h>
#include <AK/Optional.h>
#include <AK/Vector.h>
#include <LibIPC/Forward.h>
#include <LibURL/Forward.h>

namespace HTTP {

struct Proxy {
    enum class Type : u8 {
        HTTP,
        HTTPS,
        SOCKS4,
        SOCKS4A,
        SOCKS5,
        SOCKS5Hostname,
    };

    static Optional<Proxy> parse(StringView);

    bool resolves_hostnames() const;

    // For CURLOPT_PROXY.
    ByteString to_curl_url() const;

    Type type { Type::HTTP };
    ByteString host;
    u16 port { 0 };
    ByteString userinfo;
};

// Proxy environment variables with libcurl precedence.
class ProxyConfiguration {
public:
    using EnvironmentLookup = Function<Optional<StringView>(StringView)>;

    static ProxyConfiguration from_environment();
    static ProxyConfiguration from_environment(EnvironmentLookup const&);

    ProxyConfiguration() = default;
    ProxyConfiguration(
        Optional<Proxy> http_proxy,
        Optional<Proxy> https_proxy,
        Vector<ByteString> no_proxy,
        bool exclude_simple_hostnames = false,
        Vector<Proxy> http_proxy_fallbacks = {},
        Vector<Proxy> https_proxy_fallbacks = {})
        : m_http_proxy(move(http_proxy))
        , m_https_proxy(move(https_proxy))
        , m_no_proxy(move(no_proxy))
        , m_exclude_simple_hostnames(exclude_simple_hostnames)
        , m_http_proxy_fallbacks(move(http_proxy_fallbacks))
        , m_https_proxy_fallbacks(move(https_proxy_fallbacks))
    {
    }

    Optional<Proxy const&> proxy_for(URL::URL const&) const;
    Vector<Proxy> proxies_for(URL::URL const&) const;
    ProxyConfiguration with_fallback(ProxyConfiguration const&) const;

    bool is_empty() const { return !m_http_proxy.has_value() && !m_https_proxy.has_value() && m_http_proxy_fallbacks.is_empty() && m_https_proxy_fallbacks.is_empty(); }

    Optional<Proxy> const& http_proxy() const { return m_http_proxy; }
    Optional<Proxy> const& https_proxy() const { return m_https_proxy; }
    Vector<ByteString> const& no_proxy() const { return m_no_proxy; }
    bool exclude_simple_hostnames() const { return m_exclude_simple_hostnames; }
    Vector<Proxy> const& http_proxy_fallbacks() const { return m_http_proxy_fallbacks; }
    Vector<Proxy> const& https_proxy_fallbacks() const { return m_https_proxy_fallbacks; }

private:
    bool is_excluded(StringView host) const;

    Optional<Proxy> m_http_proxy;
    Optional<Proxy> m_https_proxy;
    Vector<ByteString> m_no_proxy;
    bool m_exclude_simple_hostnames { false };
    Vector<Proxy> m_http_proxy_fallbacks;
    Vector<Proxy> m_https_proxy_fallbacks;
};

}

namespace IPC {

template<>
ErrorOr<void> encode(Encoder&, HTTP::Proxy const&);

template<>
ErrorOr<HTTP::Proxy> decode(Decoder&);

template<>
ErrorOr<void> encode(Encoder&, HTTP::ProxyConfiguration const&);

template<>
ErrorOr<HTTP::ProxyConfiguration> decode(Decoder&);

}
