struct S {
  a: i32,
  b: f32,
}

struct T {
  c: array<S, 2>,
  d: i32,
}

var<private> a = array<S, 2>();

// Nested array inside struct
var<private> b = array<T, 2>();

fn func() {
  // Should not duplicate
  var c = array<S, 2>();

  // Nested array inside struct
  var d = array<T, 2>();

  // Not defined above, should get own method
  var z = array<S, 156>();
}

