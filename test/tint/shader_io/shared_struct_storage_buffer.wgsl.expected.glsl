SKIP: FAILED

#version 310 es
precision mediump float;

layout(location = 0) in float f_1;
layout(location = 1) flat in uint u_1;
struct S {
  float f;
  uint u;
  uint pad;
  uint pad_1;
  uint pad_2;
  uint pad_3;
  uint pad_4;
  uint pad_5;
  uint pad_6;
  uint pad_7;
  uint pad_8;
  uint pad_9;
  uint pad_10;
  uint pad_11;
  uint pad_12;
  uint pad_13;
  uint pad_14;
  uint pad_15;
  uint pad_16;
  uint pad_17;
  uint pad_18;
  uint pad_19;
  uint pad_20;
  uint pad_21;
  uint pad_22;
  uint pad_23;
  uint pad_24;
  uint pad_25;
  uint pad_26;
  uint pad_27;
  uint pad_28;
  uint pad_29;
  vec4 v;
  uint pad_30;
  uint pad_31;
  uint pad_32;
  uint pad_33;
  uint pad_34;
  uint pad_35;
  uint pad_36;
  uint pad_37;
  uint pad_38;
  uint pad_39;
  uint pad_40;
  uint pad_41;
  uint pad_42;
  uint pad_43;
  uint pad_44;
  uint pad_45;
  uint pad_46;
  uint pad_47;
  uint pad_48;
  uint pad_49;
  uint pad_50;
  uint pad_51;
  uint pad_52;
  uint pad_53;
  uint pad_54;
  uint pad_55;
  uint pad_56;
  uint pad_57;
};

layout(binding = 0, std430) buffer S_1 {
  float f;
  uint u;
  uint pad_58;
  uint pad_59;
  uint pad_60;
  uint pad_61;
  uint pad_62;
  uint pad_63;
  uint pad_64;
  uint pad_65;
  uint pad_66;
  uint pad_67;
  uint pad_68;
  uint pad_69;
  uint pad_70;
  uint pad_71;
  uint pad_72;
  uint pad_73;
  uint pad_74;
  uint pad_75;
  uint pad_76;
  uint pad_77;
  uint pad_78;
  uint pad_79;
  uint pad_80;
  uint pad_81;
  uint pad_82;
  uint pad_83;
  uint pad_84;
  uint pad_85;
  uint pad_86;
  uint pad_87;
  vec4 v;
  uint pad_88;
  uint pad_89;
  uint pad_90;
  uint pad_91;
  uint pad_92;
  uint pad_93;
  uint pad_94;
  uint pad_95;
  uint pad_96;
  uint pad_97;
  uint pad_98;
  uint pad_99;
  uint pad_100;
  uint pad_101;
  uint pad_102;
  uint pad_103;
  uint pad_104;
  uint pad_105;
  uint pad_106;
  uint pad_107;
  uint pad_108;
  uint pad_109;
  uint pad_110;
  uint pad_111;
  uint pad_112;
  uint pad_113;
  uint pad_114;
  uint pad_115;
} tint_symbol;
void frag_main(S tint_symbol_1) {
  float f = tint_symbol_1.f;
  uint u = tint_symbol_1.u;
  vec4 v = tint_symbol_1.v;
  tint_symbol = tint_symbol_1;
}

void main() {
  S tint_symbol_2 = S(f_1, u_1, gl_FragCoord);
  frag_main(tint_symbol_2);
  return;
}
Error parsing GLSL shader:
ERROR: 0:137: 'assign' :  cannot convert from ' in structure{ global mediump float f,  global mediump uint u,  global mediump uint pad,  global mediump uint pad_1,  global mediump uint pad_2,  global mediump uint pad_3,  global mediump uint pad_4,  global mediump uint pad_5,  global mediump uint pad_6,  global mediump uint pad_7,  global mediump uint pad_8,  global mediump uint pad_9,  global mediump uint pad_10,  global mediump uint pad_11,  global mediump uint pad_12,  global mediump uint pad_13,  global mediump uint pad_14,  global mediump uint pad_15,  global mediump uint pad_16,  global mediump uint pad_17,  global mediump uint pad_18,  global mediump uint pad_19,  global mediump uint pad_20,  global mediump uint pad_21,  global mediump uint pad_22,  global mediump uint pad_23,  global mediump uint pad_24,  global mediump uint pad_25,  global mediump uint pad_26,  global mediump uint pad_27,  global mediump uint pad_28,  global mediump uint pad_29,  global mediump 4-component vector of float v,  global mediump uint pad_30,  global mediump uint pad_31,  global mediump uint pad_32,  global mediump uint pad_33,  global mediump uint pad_34,  global mediump uint pad_35,  global mediump uint pad_36,  global mediump uint pad_37,  g
ERROR: 0:137: '' : compilation terminated
ERROR: 2 compilation errors.  No code generated.



