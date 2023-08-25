RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdx_c487fa() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = ddx(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  dpdx_c487fa();
  return;
}
