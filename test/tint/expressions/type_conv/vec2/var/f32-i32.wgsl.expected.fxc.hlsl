[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

int2 tint_ftoi(float2 v) {
  return ((v < float2(2147483520.0f, 2147483520.0f)) ? ((v < float2(-2147483648.0f, -2147483648.0f)) ? int2(-2147483648, -2147483648) : int2(v)) : int2(2147483647, 2147483647));
}

static float2 u = float2(1.0f, 1.0f);

void f() {
  const int2 v = tint_ftoi(u);
}
