[numthreads(1, 1, 1)]
void unused_entry_point() {
  return;
}

RWByteAddressBuffer v : register(u0);

int4 tint_mod(int4 lhs, int rhs) {
  const int4 r = int4((rhs).xxxx);
  const int4 rhs_or_one = (((r == int4(0, 0, 0, 0)) | ((lhs == int4(-2147483648, -2147483648, -2147483648, -2147483648)) & (r == int4(-1, -1, -1, -1)))) ? int4(1, 1, 1, 1) : r);
  if (any(((uint4((lhs | rhs_or_one)) & uint4(2147483648u, 2147483648u, 2147483648u, 2147483648u)) != uint4(0u, 0u, 0u, 0u)))) {
    return (lhs - ((lhs / rhs_or_one) * rhs_or_one));
  } else {
    return (lhs % rhs_or_one);
  }
}

void foo() {
  v.Store4(0u, asuint(tint_mod(asint(v.Load4(0u)), 2)));
}
