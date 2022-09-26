struct tint_symbol_7 {
  float2 a_particlePos : TEXCOORD0;
  float2 a_particleVel : TEXCOORD1;
  float2 a_pos : TEXCOORD2;
};
struct tint_symbol_8 {
  float4 value : SV_Position;
};

float4 vert_main_inner(float2 a_particlePos, float2 a_particleVel, float2 a_pos) {
  float angle = -(atan2(a_particleVel.x, a_particleVel.y));
  float2 pos = float2(((a_pos.x * cos(angle)) - (a_pos.y * sin(angle))), ((a_pos.x * sin(angle)) + (a_pos.y * cos(angle))));
  return float4((pos + a_particlePos), 0.0f, 1.0f);
}

tint_symbol_8 vert_main(tint_symbol_7 tint_symbol_6) {
  const float4 inner_result = vert_main_inner(tint_symbol_6.a_particlePos, tint_symbol_6.a_particleVel, tint_symbol_6.a_pos);
  tint_symbol_8 wrapper_result = (tint_symbol_8)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

struct tint_symbol_9 {
  float4 value : SV_Target0;
};

float4 frag_main_inner() {
  return (1.0f).xxxx;
}

tint_symbol_9 frag_main() {
  const float4 inner_result_1 = frag_main_inner();
  tint_symbol_9 wrapper_result_1 = (tint_symbol_9)0;
  wrapper_result_1.value = inner_result_1;
  return wrapper_result_1;
}

cbuffer cbuffer_params : register(b0, space0) {
  uint4 params[2];
};
RWByteAddressBuffer particlesA : register(u1, space0);
RWByteAddressBuffer particlesB : register(u2, space0);

struct tint_symbol_11 {
  uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

void comp_main_inner(uint3 gl_GlobalInvocationID) {
  uint index = gl_GlobalInvocationID.x;
  if ((index >= 5u)) {
    return;
  }
  const uint tint_symbol = index;
  float2 vPos = asfloat(particlesA.Load2((16u * tint_symbol)));
  const uint tint_symbol_1 = index;
  float2 vVel = asfloat(particlesA.Load2(((16u * tint_symbol_1) + 8u)));
  float2 cMass = (0.0f).xx;
  float2 cVel = (0.0f).xx;
  float2 colVel = (0.0f).xx;
  int cMassCount = 0;
  int cVelCount = 0;
  float2 pos = float2(0.0f, 0.0f);
  float2 vel = float2(0.0f, 0.0f);
  {
    [loop] for(uint i = 0u; (i < 5u); i = (i + 1u)) {
      if ((i == index)) {
        continue;
      }
      const uint tint_symbol_2 = i;
      pos = asfloat(particlesA.Load2((16u * tint_symbol_2))).xy;
      const uint tint_symbol_3 = i;
      vel = asfloat(particlesA.Load2(((16u * tint_symbol_3) + 8u))).xy;
      if ((distance(pos, vPos) < asfloat(params[0].y))) {
        cMass = (cMass + pos);
        cMassCount = (cMassCount + 1);
      }
      if ((distance(pos, vPos) < asfloat(params[0].z))) {
        colVel = (colVel - (pos - vPos));
      }
      if ((distance(pos, vPos) < asfloat(params[0].w))) {
        cVel = (cVel + vel);
        cVelCount = (cVelCount + 1);
      }
    }
  }
  if ((cMassCount > 0)) {
    cMass = ((cMass / float2(float(cMassCount), float(cMassCount))) - vPos);
  }
  if ((cVelCount > 0)) {
    cVel = (cVel / float2(float(cVelCount), float(cVelCount)));
  }
  vVel = (((vVel + (cMass * asfloat(params[1].x))) + (colVel * asfloat(params[1].y))) + (cVel * asfloat(params[1].z)));
  vVel = (normalize(vVel) * clamp(length(vVel), 0.0f, 0.100000001f));
  vPos = (vPos + (vVel * asfloat(params[0].x)));
  if ((vPos.x < -1.0f)) {
    vPos.x = 1.0f;
  }
  if ((vPos.x > 1.0f)) {
    vPos.x = -1.0f;
  }
  if ((vPos.y < -1.0f)) {
    vPos.y = 1.0f;
  }
  if ((vPos.y > 1.0f)) {
    vPos.y = -1.0f;
  }
  const uint tint_symbol_4 = index;
  particlesB.Store2((16u * tint_symbol_4), asuint(vPos));
  const uint tint_symbol_5 = index;
  particlesB.Store2(((16u * tint_symbol_5) + 8u), asuint(vVel));
}

[numthreads(1, 1, 1)]
void comp_main(tint_symbol_11 tint_symbol_10) {
  comp_main_inner(tint_symbol_10.gl_GlobalInvocationID);
  return;
}
