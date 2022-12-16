#version 310 es

shared bool arg_0;
bool tint_workgroupUniformLoad_arg_0() {
  barrier();
  bool result = arg_0;
  barrier();
  return result;
}

void workgroupUniformLoad_b75d53() {
  bool res = tint_workgroupUniformLoad_arg_0();
}

void compute_main(uint local_invocation_index) {
  {
    arg_0 = false;
  }
  barrier();
  workgroupUniformLoad_b75d53();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main(gl_LocalInvocationIndex);
  return;
}
