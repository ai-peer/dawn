SamplerState samp : register(s0, space0);
cbuffer cbuffer_params : register(b1, space0) {
  uint4 params[1];
};
Texture2D<float4> inputTex : register(t1, space1);
RWTexture2D<float4> outputTex : register(u2, space1);

cbuffer cbuffer_flip : register(b3, space1) {
  uint4 flip[1];
};
groupshared float3 tile[4][256];

struct tint_symbol_5 {
  uint3 LocalInvocationID : SV_GroupThreadID;
  uint local_invocation_index : SV_GroupIndex;
  uint3 WorkGroupID : SV_GroupID;
};

void main_inner(uint3 WorkGroupID, uint3 LocalInvocationID, uint local_invocation_index) {
  {
    [loop] for(uint idx = local_invocation_index; (idx < 1024u); idx = (idx + 64u)) {
      const uint i_1 = (idx / 256u);
      const uint i_2 = (idx % 256u);
      tile[i_1][i_2] = (0.0f).xxx;
    }
  }
  GroupMemoryBarrierWithGroupSync();
  const uint filterOffset = ((params[0].x - 1u) / 2u);
  int3 tint_tmp;
  inputTex.GetDimensions(0, tint_tmp.x, tint_tmp.y, tint_tmp.z);
  const int2 dims = tint_tmp.xy;
  const int2 baseIndex = (int2(((WorkGroupID.xy * uint2(params[0].y, 4u)) + (LocalInvocationID.xy * uint2(4u, 1u)))) - int2(int(filterOffset), 0));
  {
    [loop] for(uint r = 0u; (r < 4u); r = (r + 1u)) {
      {
        [loop] for(uint c = 0u; (c < 4u); c = (c + 1u)) {
          int2 loadIndex = (baseIndex + int2(int(c), int(r)));
          if ((flip[0].x != 0u)) {
            loadIndex = loadIndex.yx;
          }
          const uint tint_symbol = r;
          const uint tint_symbol_1 = ((4u * LocalInvocationID.x) + c);
          tile[tint_symbol][tint_symbol_1] = inputTex.SampleLevel(samp, ((float2(loadIndex) + (0.25f).xx) / float2(dims)), 0.0f).rgb;
        }
      }
    }
  }
  GroupMemoryBarrierWithGroupSync();
  {
    [loop] for(uint r = 0u; (r < 4u); r = (r + 1u)) {
      {
        [loop] for(uint c = 0u; (c < 4u); c = (c + 1u)) {
          int2 writeIndex = (baseIndex + int2(int(c), int(r)));
          if ((flip[0].x != 0u)) {
            writeIndex = writeIndex.yx;
          }
          const uint center = ((4u * LocalInvocationID.x) + c);
          bool tint_tmp_2 = (center >= filterOffset);
          if (tint_tmp_2) {
            tint_tmp_2 = (center < (256u - filterOffset));
          }
          bool tint_tmp_1 = (tint_tmp_2);
          if (tint_tmp_1) {
            tint_tmp_1 = all((writeIndex < dims));
          }
          if ((tint_tmp_1)) {
            float3 acc = (0.0f).xxx;
            {
              [loop] for(uint f = 0u; (f < params[0].x); f = (f + 1u)) {
                uint i = ((center + f) - filterOffset);
                const uint tint_symbol_2 = r;
                const uint tint_symbol_3 = i;
                acc = (acc + ((1.0f / float(params[0].x)) * tile[tint_symbol_2][tint_symbol_3]));
              }
            }
            outputTex[writeIndex] = float4(acc, 1.0f);
          }
        }
      }
    }
  }
}

[numthreads(64, 1, 1)]
void main(tint_symbol_5 tint_symbol_4) {
  main_inner(tint_symbol_4.WorkGroupID, tint_symbol_4.LocalInvocationID, tint_symbol_4.local_invocation_index);
  return;
}
