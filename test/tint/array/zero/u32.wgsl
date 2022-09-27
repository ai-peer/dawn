var<private> a = array<u32, 2>();

fn func() {
  // These should all re-use the methods defined above
  var a = array<u32, 2>();

  // Not defined above, should get own method
  var z = array<u32, 156>();
}

