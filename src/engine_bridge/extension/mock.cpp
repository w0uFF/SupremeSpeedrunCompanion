#include "src/engine_bridge/extension/contract.h"
#include <new>
struct ssr_context { unsigned marker = 0; };
namespace {
uint32_t SSR_CALL open_mock(const ssr_host_v1* host, ssr_context** out) noexcept {
    if (!host || !out || host->size != sizeof(ssr_host_v1) || host->version != SSR_ABI_V1) return SSR_BAD_ABI;
    if (*out) return SSR_BAD_STATE;
    if (host->phase != SSR_PHASE_OFFLINE_SAFE) return SSR_UNSAFE_PHASE;
    if (host->enabled != 1) return SSR_DISABLED;
    *out = new (std::nothrow) ssr_context{};
    return *out ? SSR_OK : SSR_BAD_STATE;
}
uint32_t SSR_CALL close_mock(ssr_context** ctx) noexcept {
    if (!ctx || !*ctx) return SSR_BAD_STATE;
    delete *ctx;
    *ctx = nullptr;
    return SSR_OK;
}
}
extern "C" uint32_t SSR_CALL ssr_offline_query(uint32_t version, uint32_t size, ssr_api_v1* out) {
    if (!out || version != SSR_ABI_V1 || size != sizeof(ssr_api_v1)) return SSR_BAD_ABI;
    *out = {sizeof(ssr_api_v1), SSR_ABI_V1, open_mock, close_mock};
    return SSR_OK;
}
