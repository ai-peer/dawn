struct MyStruct {
    f1 : f32,
};

type MyArray = array<f32, 10>;

var<private> v1 = 1;
var<private> v2 = 1u;
var<private> v3 = 1.0;

var<private> v4 = vec3<i32>(1, 1, 1);
var<private> v5 = vec3<u32>(1u, 2u, 3u);
var<private> v6 = vec3<f32>(1.0, 2.0, 3.0);

var<private> v7 = MyStruct(1.0);
var<private> v8 = MyArray();

var<private> v9 = i32();
var<private> v10 = u32();
var<private> v11 = f32();
var<private> v12 = MyStruct();
var<private> v13 = MyStruct();
var<private> v14 = MyArray();

var<private> v15 = vec3(1, 2, 3);
var<private> v16 = vec3(1.0, 2.0, 3.0);
