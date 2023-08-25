Texture2DMS<float4> texture0 : register(t0);
Texture2DMS<float4> texture1 : register(t1);

RWByteAddressBuffer results : register(u2);

[numthreads(1, 1, 1)]
void main() {
  {
    for(int i = 0; (i < 4); i = (i + 1)) {
      results.Store((4u * uint(i)), asuint(texture0.Load(int2(0, 0), i).x));
      results.Store((16u + (4u * uint(i))), asuint(texture1.Load(int3(0, 0, 0), i).x));
    }
  }
  return;
}
