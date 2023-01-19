type a = vec3f;

fn f() {
  {
    const vec3f_1 = 1;
    const b = vec3f_1;
  }
  const c : a = a();
  const d : vec3f = vec3f();
}
