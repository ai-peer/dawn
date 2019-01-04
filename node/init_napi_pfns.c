#include "js_native_api.h"
#include "node_api.h"

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

#define X(ret, name, args) PFN_##name name;
_NAPI_PFNS_JS_NATIVE_API
_NAPI_PFNS_JS_NATIVE_API_EXPERIMENTAL
_NAPI_PFNS_NODE_API
_NAPI_PFNS_NODE_API_GE2
_NAPI_PFNS_NODE_API_GE3
_NAPI_PFNS_NODE_API_EXPERIMENTAL
#undef X

void init_napi_pfns() {
    void* self = dlopen(NULL, RTLD_LAZY);
    assert(self);

#define X(ret, name, args)                 \
    name = (PFN_##name)dlsym(self, #name); \
    assert(name);
    _NAPI_PFNS_JS_NATIVE_API
    _NAPI_PFNS_JS_NATIVE_API_EXPERIMENTAL
    _NAPI_PFNS_NODE_API
    _NAPI_PFNS_NODE_API_GE2
    _NAPI_PFNS_NODE_API_GE3
    _NAPI_PFNS_NODE_API_EXPERIMENTAL
#undef X
}
