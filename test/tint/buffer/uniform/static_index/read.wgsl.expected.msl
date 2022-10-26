SKIP: FAILED

uniform/static_index/read.wgsl:30:37 error: uniform storage requires that array elements be aligned to 16 bytes, but array element alignment is currently 8. Consider using the @size attribute on the last struct member.
    @align(16) array_struct_inner : array<Inner, 4>,
                                    ^^^^^^^^^^^^^^^

uniform/static_index/read.wgsl:6:1 note: see layout of struct:
/*             align(16) size(592) */ struct S {
/* offset(  0) align( 4) size(  4) */   scalar_f32 : f32;
/* offset(  4) align( 4) size(  4) */   scalar_i32 : i32;
/* offset(  8) align( 4) size(  4) */   scalar_u32 : u32;
/* offset( 12) align( 1) size(  4) */   // -- implicit field alignment padding --;
/* offset( 16) align( 8) size(  8) */   vec2_f32 : vec2<f32>;
/* offset( 24) align( 8) size(  8) */   vec2_i32 : vec2<i32>;
/* offset( 32) align( 8) size(  8) */   vec2_u32 : vec2<u32>;
/* offset( 40) align( 1) size(  8) */   // -- implicit field alignment padding --;
/* offset( 48) align(16) size( 12) */   vec3_f32 : vec3<f32>;
/* offset( 60) align( 1) size(  4) */   // -- implicit field alignment padding --;
/* offset( 64) align(16) size( 12) */   vec3_i32 : vec3<i32>;
/* offset( 76) align( 1) size(  4) */   // -- implicit field alignment padding --;
/* offset( 80) align(16) size( 12) */   vec3_u32 : vec3<u32>;
/* offset( 92) align( 1) size(  4) */   // -- implicit field alignment padding --;
/* offset( 96) align(16) size( 16) */   vec4_f32 : vec4<f32>;
/* offset(112) align(16) size( 16) */   vec4_i32 : vec4<i32>;
/* offset(128) align(16) size( 16) */   vec4_u32 : vec4<u32>;
/* offset(144) align( 8) size( 16) */   mat2x2_f32 : mat2x2<f32>;
/* offset(160) align(16) size( 32) */   mat2x3_f32 : mat2x3<f32>;
/* offset(192) align(16) size( 32) */   mat2x4_f32 : mat2x4<f32>;
/* offset(224) align( 8) size( 24) */   mat3x2_f32 : mat3x2<f32>;
/* offset(248) align( 1) size(  8) */   // -- implicit field alignment padding --;
/* offset(256) align(16) size( 48) */   mat3x3_f32 : mat3x3<f32>;
/* offset(304) align(16) size( 48) */   mat3x4_f32 : mat3x4<f32>;
/* offset(352) align( 8) size( 32) */   mat4x2_f32 : mat4x2<f32>;
/* offset(384) align(16) size( 64) */   mat4x3_f32 : mat4x3<f32>;
/* offset(448) align(16) size( 64) */   mat4x4_f32 : mat4x4<f32>;
/* offset(512) align(16) size( 32) */   arr2_vec3_f32 : array<vec3<f32>, 2>;
/* offset(544) align(16) size(  8) */   struct_inner : Inner;
/* offset(552) align( 1) size(  8) */   // -- implicit field alignment padding --;
/* offset(560) align(16) size( 32) */   array_struct_inner : array<Inner, 4>;
/*                                 */ };
struct S {
^^^^^^

uniform/static_index/read.wgsl:33:36 note: see declaration of variable
@binding(0) @group(0) var<uniform> ub : S;
                                   ^^

