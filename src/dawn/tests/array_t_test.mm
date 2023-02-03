#include <array>
#include <Metal/Metal.h>

#define LOG(FORMAT, ...) \
    printf("%s\n", [[NSString stringWithFormat:FORMAT, ##__VA_ARGS__] UTF8String]);

constexpr char msl[] = R"(
#include <metal_stdlib>
using namespace metal;

template<typename T, size_t N>
struct array_wrapper {
    const constant T& operator[](size_t i) const constant { return elements[i]; }
    device T& operator[](size_t i) device { return elements[i]; }
    const device T& operator[](size_t i) const device { return elements[i]; }
    thread T& operator[](size_t i) thread { return elements[i]; }
    const thread T& operator[](size_t i) const thread { return elements[i]; }
    threadgroup T& operator[](size_t i) threadgroup { return elements[i]; }
    const threadgroup T& operator[](size_t i) const threadgroup { return elements[i]; }
    T elements[N ? N : 1];
};

constant array_wrapper<float2, 3> global_arr = array_wrapper<float2, 3>({
  {float2(1., 2.), float2(3., 4.), float2(5., 6.)}
});

vertex void test(
  uint id [[vertex_id]],
  device float* buffer [[buffer(0)]]
) {
  array_wrapper<float2, 3> global_arr_copy = global_arr;

  array_wrapper<array_wrapper<float, 2>, 3> local_float_arr;
  local_float_arr[0][0] = 7.0;
  local_float_arr[0][1] = 8.0;
  local_float_arr[1][0] = 9.0;
  local_float_arr[1][1] = 10.0;
  local_float_arr[2][0] = 11.0;
  local_float_arr[2][1] = 12.0;

  array_wrapper<array_wrapper<float2, 2>, 3> local_float2_arr;
  local_float2_arr[0][0] = float2(13.0, 14.0);
  local_float2_arr[0][1] = float2(15.0, 16.0);
  local_float2_arr[1][0] = float2(17.0, 18.0);
  local_float2_arr[1][1] = float2(19.0, 20.0);
  local_float2_arr[2][0] = float2(21.0, 22.0);
  local_float2_arr[2][1] = float2(23.0, 24.0);

  buffer[2 * id] = global_arr[id][0];
  buffer[2 * id + 1] = global_arr[id][1];

  buffer[6 + 2 * id] = global_arr_copy[id][0];
  buffer[6 + 2 * id + 1] = global_arr_copy[id][1];

  buffer[12 + 2 * id] = local_float_arr[id][0];
  buffer[12 + 2 * id + 1] = local_float_arr[id][1];

  buffer[18 + 4 * id] = local_float2_arr[id][0][0];
  buffer[18 + 4 * id + 1] = local_float2_arr[id][0][1];
  buffer[18 + 4 * id + 2] = local_float2_arr[id][1][0];
  buffer[18 + 4 * id + 3] = local_float2_arr[id][1][1];
}

)";

int main(int argc, char* argv[]) {
  auto osVersion = [[NSProcessInfo processInfo] operatingSystemVersionString];
  for (id<MTLDevice> device : MTLCopyAllDevices()) {
    LOG(@"\n\nTesting with %@ on OS %@", [device name], osVersion);

    NSError* error = nullptr;
    id<MTLLibrary> lib = [device newLibraryWithSource:[NSString stringWithUTF8String:msl] options:nil error:&error];
    if (error != nullptr) {
        LOG(@"%@", error);
        return 1;
    }

    MTLRenderPipelineDescriptor* pipelineStateDesc = [MTLRenderPipelineDescriptor new];
    pipelineStateDesc.rasterizationEnabled = false;
    pipelineStateDesc.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
    pipelineStateDesc.vertexFunction = [lib newFunctionWithName:@"test"];
    id<MTLRenderPipelineState> pipelineState =
        [device newRenderPipelineStateWithDescriptor:pipelineStateDesc error:&error];
    if (error != nullptr) {
        LOG(@"%@", error);
        return 1;
    }
    std::array<float, 30> expected = {
      1., 2., 3., 4., 5., 6.,
      1., 2., 3., 4., 5., 6.,
      7., 8., 9., 10., 11., 12.,
      13., 14., 15., 16., 17., 18., 19., 20., 21., 22., 23., 24.,
    };
    id<MTLBuffer> buffer = [device newBufferWithLength:expected.size() * sizeof(float)
                                               options:MTLResourceStorageModeShared];

    id<MTLCommandQueue> commandQueue = [device newCommandQueue];
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    MTLRenderPassDescriptor* rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];
    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:rpDesc];
    [encoder setRenderPipelineState:pipelineState];
    [encoder setVertexBuffer:buffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:3];
    [encoder endEncoding];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    auto* contents = static_cast<float*>([buffer contents]);
    bool failed = false;
    for (unsigned i = 0; i < expected.size(); ++i) {
      if (abs(expected[i] - contents[i]) > 0.01) {
        failed = true;
        LOG(@"Expected %f at index %d. Got %f.", expected[i], i, contents[i]);
      }
    }
    if (!failed) {
      LOG(@"OK");
    }
  }
  return 0;
}
