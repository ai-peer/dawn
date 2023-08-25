static bool bool_var1 = true;
static bool bool_var2 = true;
static bool bool_var3 = true;
static int i32_var1 = 1;
static int i32_var2 = 1;
static int i32_var3 = 1;
static uint u32_var1 = 1u;
static uint u32_var2 = 1u;
static uint u32_var3 = 1u;
static bool3 v3bool_var1 = bool3(true, true, true);
static bool3 v3bool_var2 = bool3(true, true, true);
static bool3 v3bool_var3 = bool3(true, true, true);
static int3 v3i32_var1 = int3(1, 1, 1);
static int3 v3i32_var2 = int3(1, 1, 1);
static int3 v3i32_var3 = int3(1, 1, 1);
static uint3 v3u32_var1 = uint3(1u, 1u, 1u);
static uint3 v3u32_var2 = uint3(1u, 1u, 1u);
static uint3 v3u32_var3 = uint3(1u, 1u, 1u);
static bool3 v3bool_var4 = bool3(true, true, true);
static bool4 v4bool_var5 = bool4(true, false, true, false);

[numthreads(1, 1, 1)]
void main() {
  bool_var1 = false;
  bool_var2 = false;
  bool_var3 = false;
  i32_var1 = 0;
  i32_var2 = 0;
  i32_var3 = 0;
  u32_var1 = 0u;
  u32_var2 = 0u;
  u32_var3 = 0u;
  v3bool_var1 = bool3(false, false, false);
  v3bool_var2 = bool3(false, false, false);
  v3bool_var3 = bool3(false, false, false);
  v3bool_var4 = bool3(false, false, false);
  v4bool_var5 = bool4(false, false, false, false);
  v3i32_var1 = int3(0, 0, 0);
  v3i32_var2 = int3(0, 0, 0);
  v3i32_var3 = int3(0, 0, 0);
  v3u32_var1 = uint3(0u, 0u, 0u);
  v3u32_var2 = uint3(0u, 0u, 0u);
  v3u32_var3 = uint3(0u, 0u, 0u);
  return;
}
