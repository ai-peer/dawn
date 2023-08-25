[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

int4 tint_ftoi(float4 v) {
  return ((v < float4(2147483520.0f, 2147483520.0f, 2147483520.0f, 2147483520.0f)) ? ((v < float4(-2147483648.0f, -2147483648.0f, -2147483648.0f, -2147483648.0f)) ? int4(-2147483648, -2147483648, -2147483648, -2147483648) : int4(v)) : int4(2147483647, 2147483647, 2147483647, 2147483647));
}

static float4 u = float4(1.0f, 1.0f, 1.0f, 1.0f);

void f() {
  const int4 v = tint_ftoi(u);
}
