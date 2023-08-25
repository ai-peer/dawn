RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdxCoarse_f64d7b() {
  float3 res = ddx_coarse(float3(1.0f, 1.0f, 1.0f));
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  dpdxCoarse_f64d7b();
  return;
}
