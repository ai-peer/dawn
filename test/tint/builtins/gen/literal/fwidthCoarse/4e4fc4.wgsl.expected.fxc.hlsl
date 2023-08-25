RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidthCoarse_4e4fc4() {
  float4 res = fwidth(float4(1.0f, 1.0f, 1.0f, 1.0f));
  prevent_dce.Store4(0u, asuint(res));
}

void fragment_main() {
  fwidthCoarse_4e4fc4();
  return;
}
