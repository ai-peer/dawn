#version 310 es

shared uint arg_0;
void compute_main(uint local_invocation_index) {
  {
    atomicExchange(arg_0, 0u);
  }
  barrier();
  uint x = 1u;
  uint y = 2u;
  uint res = atomicAdd(arg_0, -(x + y));
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main(gl_LocalInvocationIndex);
  return;
}
