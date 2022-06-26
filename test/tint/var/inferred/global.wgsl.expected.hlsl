[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

struct MyStruct {
  float f1;
};

static int v1 = 1;
static uint v2 = 1u;
static float v3 = 1.0f;
static int3 v4 = (1).xxx;
static uint3 v5 = uint3(1u, 2u, 3u);
static float3 v6 = float3(1.0f, 2.0f, 3.0f);
static MyStruct v7 = {1.0f};
static float v8[10] = (float[10])0;
static int v9 = 0;
static uint v10 = 0u;
static float v11 = 0.0f;
static MyStruct v12 = (MyStruct)0;
static MyStruct v13 = (MyStruct)0;
static float v14[10] = (float[10])0;
static int3 v15 = int3(1, 2, 3);
static float3 v16 = float3(1.0f, 2.0f, 3.0f);
