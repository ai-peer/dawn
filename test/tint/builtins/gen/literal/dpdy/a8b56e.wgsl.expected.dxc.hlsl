RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdy_a8b56e() {
  float2 res = ddy(float2(1.0f, 1.0f));
  prevent_dce.Store2(0u, asuint(res));
}

void fragment_main() {
  dpdy_a8b56e();
  return;
}
