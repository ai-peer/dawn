RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdy_feb40f() {
  float3 res = ddy(float3(1.0f, 1.0f, 1.0f));
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  dpdy_feb40f();
  return;
}
