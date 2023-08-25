[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer v : register(u0);

int4 tint_div(int4 lhs, int4 rhs) {
  return (lhs / (((rhs == int4(0, 0, 0, 0)) | ((lhs == int4(-2147483648, -2147483648, -2147483648, -2147483648)) & (rhs == int4(-1, -1, -1, -1)))) ? int4(1, 1, 1, 1) : rhs));
}

void foo() {
  v.Store4(0u, asuint(tint_div(asint(v.Load4(0u)), int4(2, 2, 2, 2))));
}
