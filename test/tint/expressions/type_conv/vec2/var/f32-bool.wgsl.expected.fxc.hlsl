[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

static float2 u = float2(1.0f, 1.0f);

void f() {
  const bool2 v = bool2(u);
}
