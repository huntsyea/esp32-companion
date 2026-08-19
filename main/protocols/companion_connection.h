#ifndef COMPANION_CONNECTION_H
#define COMPANION_CONNECTION_H

#include <cstdint>
#include <functional>
#include <string>

enum class CompanionConnectionMode {
    kLegacy,
    kPhysical,
    kLoopbackDevelopment,
};

enum class CompanionEndpointSource {
    kNone,
    kMdns,
    kFallback,
};

struct CompanionConnectionConfig {
    CompanionConnectionMode mode = CompanionConnectionMode::kLegacy;
    std::string mdns_host;
    uint16_t port = 8765;
    std::string path = "/xiaozhi/v1";
    std::string fallback_url;
    std::string ca_certificate_pem;
};

struct CompanionEndpoint {
    std::string url;
    CompanionEndpointSource source = CompanionEndpointSource::kNone;
    std::string error;

    bool ok() const { return error.empty() && !url.empty(); }
};

using CompanionHostProbe = std::function<bool(const std::string& host)>;

/** Resolve the configured endpoint while enforcing the physical transport policy. */
CompanionEndpoint ResolveCompanionEndpoint(const CompanionConnectionConfig& config,
                                           const CompanionHostProbe& host_probe);

#endif  // COMPANION_CONNECTION_H
