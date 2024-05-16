#include "dawn/webgpu.h"

/* Converts Kotlin objects representing Dawn structures into native structures that can be passed
   into the native Dawn API. */

{% for structure in by_category['structure'] %}
    const {{ as_cType(structure.name) }} convert{{ structure.name.CamelCase() -}}
        (JNIEnv *env, jobject obj);
{% endfor %}
