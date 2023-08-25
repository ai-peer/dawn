[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static uint2 u = uint2(1u, 1u);

void f() {
  const float2 v = float2(u);
}
