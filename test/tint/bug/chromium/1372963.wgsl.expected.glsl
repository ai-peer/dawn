#version 310 es
precision mediump float;

void gn() {
  dFdx(-127.0f);
}

void main() {
  gn();
  return;
}
