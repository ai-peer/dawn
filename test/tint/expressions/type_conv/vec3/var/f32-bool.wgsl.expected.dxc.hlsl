[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float3 u = float3(1.0f, 1.0f, 1.0f);

void f() {
  const bool3 v = bool3(u);
}
