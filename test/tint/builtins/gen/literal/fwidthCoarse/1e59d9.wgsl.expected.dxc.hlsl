RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidthCoarse_1e59d9() {
  float3 res = fwidth(float3(1.0f, 1.0f, 1.0f));
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  fwidthCoarse_1e59d9();
  return;
}
