RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdy_feb40f() {
  float3 arg_0 = float3(1.0f, 1.0f, 1.0f);
  float3 res = ddy(arg_0);
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  dpdy_feb40f();
  return;
}
