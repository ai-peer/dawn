
#include <memory>
#include <vector>


#include "dawn/common/Log.h"
#include "dawn/native/DawnNative.h"
#include "dawn/dawn_proc.h"

int main() {
  dawnProcSetProcs(&dawn::native::GetProcs());

  auto instance = std::make_unique<dawn::native::Instance>();
  auto adapter = instance->EnumerateAdapters()[0];

  std::vector<const char*> enabled_toggles;
  enabled_toggles.push_back("disable_lazy_clear_for_mapped_at_creation_buffer");

  wgpu::DawnTogglesDescriptor toggles_desc;
  toggles_desc.enabledToggles = enabled_toggles.data();
  toggles_desc.enabledToggleCount = enabled_toggles.size();

  wgpu::DeviceDescriptor device_desc;
  device_desc.nextInChain = &toggles_desc;

  auto device = wgpu::Device::Acquire(adapter.CreateDevice(&device_desc));

  wgpu::BufferDescriptor buffer_desc = {};
  buffer_desc.mappedAtCreation = true;
  buffer_desc.size = 1024;
  buffer_desc.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;

  wgpu::Buffer buffer = device.CreateBuffer(&buffer_desc);

  wgpu::Future future = buffer.MapAsyncF(wgpu::MapMode::Write, 0, buffer_desc.size, {nullptr, wgpu::CallbackMode::WaitAnyOnly, [](WGPUBufferMapAsyncStatus s, void* userData) {

  }, nullptr});

  wgpu::FutureWaitInfo waitInfo = {future};
  DAWN_DEBUG() << "Success: " << (wgpu::Instance(instance->Get()).WaitAny(1, &waitInfo, 0) == wgpu::WaitStatus::Success);
  DAWN_DEBUG() << "Completed: " << waitInfo.completed;
  return 0;
}