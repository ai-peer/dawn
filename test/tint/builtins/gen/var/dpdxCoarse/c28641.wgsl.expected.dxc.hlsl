RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdxCoarse_c28641() {
  float4 arg_0 = float4(1.0f, 1.0f, 1.0f, 1.0f);
  float4 res = ddx_coarse(arg_0);
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  dpdxCoarse_c28641();
  return;
}
