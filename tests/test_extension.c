#include "src/engine_bridge/extension/contract.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    ssr_api_v1 api = {0};
    ssr_context* ctx = 0;
    ssr_host_v1 host = {sizeof(host), SSR_ABI_V1, SSR_PHASE_OFFLINE_SAFE, 1};
    CHECK(ssr_offline_query(2, sizeof(api), &api) == SSR_BAD_ABI && api.version == 0);
    CHECK(ssr_offline_query(1, sizeof(api)-1, &api) == SSR_BAD_ABI);
    CHECK(ssr_offline_query(1, sizeof(api), 0) == SSR_BAD_ABI);
    CHECK(ssr_offline_query(1, sizeof(api), &api) == SSR_OK && api.version == 1);
    CHECK(api.open(0, &ctx) == SSR_BAD_ABI);
    host.size = 0; CHECK(api.open(&host, &ctx) == SSR_BAD_ABI && !ctx); host.size = sizeof(host);
    host.phase = 0; CHECK(api.open(&host, &ctx) == SSR_UNSAFE_PHASE && !ctx); host.phase = SSR_PHASE_OFFLINE_SAFE;
    host.enabled = 0; CHECK(api.open(&host, &ctx) == SSR_DISABLED && !ctx); host.enabled = 1;
    CHECK(api.open(&host, &ctx) == SSR_OK && ctx);
    CHECK(api.open(&host, &ctx) == SSR_BAD_STATE && ctx);
    CHECK(api.close(&ctx) == SSR_OK && !ctx);
    CHECK(api.close(&ctx) == SSR_BAD_STATE);
    CHECK(api.open(&host, &ctx) == SSR_OK && ctx);
    CHECK(api.close(&ctx) == SSR_OK && !ctx);
    puts("C caller / C++ mock lifecycle PASS"); return 0;
}
