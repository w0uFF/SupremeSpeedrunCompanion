#ifndef SSR_EXTENSION_CONTRACT_H
#define SSR_EXTENSION_CONTRACT_H
#include <stdint.h>
/* Experimental in-process C ABI, not a Display Config export. No game pointers. */
#if defined(_WIN32)
#define SSR_CALL __cdecl
#else
#define SSR_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define SSR_ABI_V1 1u
#define SSR_OK 0u
#define SSR_BAD_ABI 1u
#define SSR_DISABLED 2u
#define SSR_BAD_STATE 3u
#define SSR_UNSAFE_PHASE 4u
#define SSR_PHASE_OFFLINE_SAFE 1u
/* Only a synthetic host may use this revision. Native admission stays disabled. */
typedef struct ssr_host_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t phase;
    uint32_t enabled;
} ssr_host_v1;
typedef struct ssr_context ssr_context;
typedef struct ssr_api_v1 {
    uint32_t size;
    uint32_t version;
    uint32_t (SSR_CALL *open)(const ssr_host_v1 *, ssr_context **);
    uint32_t (SSR_CALL *close)(ssr_context **);
} ssr_api_v1;
/* Caller supplies a full writable table; no write on failure. */
uint32_t SSR_CALL ssr_offline_query(uint32_t version, uint32_t size, ssr_api_v1 *out);
#ifdef __cplusplus
}
#endif
#endif
