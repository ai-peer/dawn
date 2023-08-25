RWByteAddressBuffer prevent_dce : register(u0, space2);

void dpdx_0763f7() {
  float3 res = ddx(float3(1.0f, 1.0f, 1.0f));
  prevent_dce.Store3(0u, asuint(res));
}

void fragment_main() {
  dpdx_0763f7();
  return;
}
