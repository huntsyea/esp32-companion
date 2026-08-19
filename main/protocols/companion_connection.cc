#include "companion_connection.h"

#include <algorithm>
#include <cctype>

namespace {

bool HasPrefix(const std::string& value, const char* prefix) { return value.rfind(prefix, 0) == 0; }

bool IsSafePath(const std::string& path) {
    return !path.empty() && path.front() == '/' && path.find('?') == std::string::npos &&
           path.find('#') == std::string::npos;
}

bool IsSafeHostname(const std::string& host) {
    if (host.empty() || host.size() > 253 || host.front() == '.' || host.back() == '.') {
        return false;
    }
    size_t label_length = 0;
    bool previous_was_hyphen = false;
    for (unsigned char character : host) {
        if (character == '.') {
            if (label_length == 0 || previous_was_hyphen) {
                return false;
            }
            label_length = 0;
            previous_was_hyphen = false;
            continue;
        }
        if (!std::isalnum(character) && character != '-') {
            return false;
        }
        if (label_length == 0 && character == '-') {
            return false;
        }
        ++label_length;
        if (label_length > 63) {
            return false;
        }
        previous_was_hyphen = character == '-';
    }
    return label_length > 0 && !previous_was_hyphen;
}

std::string ExtractHost(const std::string& url) {
    const size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        return {};
    }
    const size_t host_start = scheme_end + 3;
    const size_t host_end = url.find_first_of(":/", host_start);
    return url.substr(host_start, host_end - host_start);
}

bool IsLoopbackUrl(const std::string& url) {
    if (!HasPrefix(url, "ws://")) {
        return false;
    }
    std::string host = ExtractHost(url);
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return host == "localhost" || host == "127.0.0.1";
}

CompanionEndpoint Reject(const char* error) {
    CompanionEndpoint endpoint;
    endpoint.error = error;
    return endpoint;
}

}  // namespace

CompanionEndpoint ResolveCompanionEndpoint(const CompanionConnectionConfig& config,
                                           const CompanionHostProbe& host_probe) {
    if (!IsSafePath(config.path)) {
        return Reject("invalid_path");
    }

    if (config.mode == CompanionConnectionMode::kPhysical) {
        if (config.ca_certificate_pem.empty()) {
            return Reject("ca_trust_anchor_required");
        }
        if (!config.mdns_host.empty() && !IsSafeHostname(config.mdns_host)) {
            return Reject("invalid_mdns_host");
        }
        if (!config.mdns_host.empty() && host_probe && host_probe(config.mdns_host)) {
            CompanionEndpoint endpoint;
            endpoint.url =
                "wss://" + config.mdns_host + ":" + std::to_string(config.port) + config.path;
            endpoint.source = CompanionEndpointSource::kMdns;
            return endpoint;
        }
        if (!HasPrefix(config.fallback_url, "wss://")) {
            return Reject("physical_mode_requires_wss");
        }
    } else if (config.mode == CompanionConnectionMode::kLoopbackDevelopment) {
        if (!IsLoopbackUrl(config.fallback_url)) {
            return Reject("plaintext_requires_loopback");
        }
    } else {
        return Reject("companion_mode_not_enabled");
    }

    if (config.fallback_url.empty()) {
        return Reject("endpoint_unavailable");
    }
    CompanionEndpoint endpoint;
    endpoint.url = config.fallback_url;
    endpoint.source = CompanionEndpointSource::kFallback;
    return endpoint;
}
