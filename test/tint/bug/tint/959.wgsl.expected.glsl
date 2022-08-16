#version 310 es
precision mediump float;

struct S {
  float a;
};

layout(binding = 0, std430) buffer S_1 {
  float a;
} b0;
layout(binding = 0, std430) buffer S_2 {
  float a;
} b1;
layout(binding = 0, std430) buffer S_3 {
  float a;
} b2;
layout(binding = 0, std430) buffer S_4 {
  float a;
} b3;
layout(binding = 0, std430) buffer S_5 {
  float a;
} b4;
layout(binding = 0, std430) buffer S_6 {
  float a;
} b5;
layout(binding = 0, std430) buffer S_7 {
  float a;
} b6;
layout(binding = 0, std430) buffer S_8 {
  float a;
} b7;
layout(binding = 1) uniform S_9 {
  S _;
} b8;

layout(binding = 1) uniform S_10 {
  S _;
} b9;

layout(binding = 1) uniform S_11 {
  S _;
} b10;

layout(binding = 1) uniform S_12 {
  S _;
} b11;

layout(binding = 1) uniform S_13 {
  S _;
} b12;

layout(binding = 1) uniform S_14 {
  S _;
} b13;

layout(binding = 1) uniform S_15 {
  S _;
} b14;

layout(binding = 1) uniform S_16 {
  S _;
} b15;

void tint_symbol() {
}

void main() {
  tint_symbol();
  return;
}
