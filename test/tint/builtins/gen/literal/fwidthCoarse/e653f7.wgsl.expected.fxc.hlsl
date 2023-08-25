RWByteAddressBuffer prevent_dce : register(u0, space2);

void fwidthCoarse_e653f7() {
  float2 res = fwidth(float2(1.0f, 1.0f));
  prevent_dce.Store2(0u, asuint(res));
}

void fragment_main() {
  fwidthCoarse_e653f7();
  return;
}
