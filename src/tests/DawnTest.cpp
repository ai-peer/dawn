// Copyright 2017 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tests/DawnTest.h"

#include "common/Assert.h"
#include "common/GPUInfo.h"
#include "common/Log.h"
#include "common/Math.h"
#include "common/Platform.h"
#include "common/SystemUtils.h"
#include "dawn/dawn_proc.h"
#include "dawn_native/DawnNative.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireServer.h"
#include "utils/PlatformDebugLogger.h"
#include "utils/SystemUtils.h"
#include "utils/TerribleCommandBuffer.h"
#include "utils/TestUtils.h"
#include "utils/WGPUHelpers.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>

#if defined(DAWN_ENABLE_BACKEND_OPENGL)
#    include "GLFW/glfw3.h"
#    include "dawn_native/OpenGLBackend.h"
#endif  // DAWN_ENABLE_BACKEND_OPENGL

namespace {

    std::string ParamName(wgpu::BackendType type) {
        switch (type) {
            case wgpu::BackendType::D3D12:
                return "D3D12";
            case wgpu::BackendType::Metal:
                return "Metal";
            case wgpu::BackendType::Null:
                return "Null";
            case wgpu::BackendType::OpenGL:
                return "OpenGL";
            case wgpu::BackendType::OpenGLES:
                return "OpenGLES";
            case wgpu::BackendType::Vulkan:
                return "Vulkan";
            default:
                UNREACHABLE();
        }
    }

    const char* AdapterTypeName(wgpu::AdapterType type) {
        switch (type) {
            case wgpu::AdapterType::DiscreteGPU:
                return "Discrete GPU";
            case wgpu::AdapterType::IntegratedGPU:
                return "Integrated GPU";
            case wgpu::AdapterType::CPU:
                return "CPU";
            case wgpu::AdapterType::Unknown:
                return "Unknown";
            default:
                UNREACHABLE();
        }
    }

    struct MapReadUserdata {
        DawnTestBase* test;
        size_t slot;
    };

    DawnTestEnvironment* gTestEnv = nullptr;

    template <typename T>
    void printBuffer(testing::AssertionResult& result, const T* buffer, const size_t count) {
        static constexpr unsigned int kBytes = sizeof(T);

        for (size_t index = 0; index < count; ++index) {
            auto byteView = reinterpret_cast<const uint8_t*>(buffer + index);
            for (unsigned int b = 0; b < kBytes; ++b) {
                char buf[4];
                sprintf(buf, "%02X ", byteView[b]);
                result << buf;
            }
        }
        result << std::endl;
    }

}  // anonymous namespace

const RGBA8 RGBA8::kZero = RGBA8(0, 0, 0, 0);
const RGBA8 RGBA8::kBlack = RGBA8(0, 0, 0, 255);
const RGBA8 RGBA8::kRed = RGBA8(255, 0, 0, 255);
const RGBA8 RGBA8::kGreen = RGBA8(0, 255, 0, 255);
const RGBA8 RGBA8::kBlue = RGBA8(0, 0, 255, 255);
const RGBA8 RGBA8::kYellow = RGBA8(255, 255, 0, 255);
const RGBA8 RGBA8::kWhite = RGBA8(255, 255, 255, 255);

BackendTestConfig::BackendTestConfig(wgpu::BackendType backendType,
                                     std::initializer_list<const char*> forceEnabledWorkarounds,
                                     std::initializer_list<const char*> forceDisabledWorkarounds)
    : backendType(backendType),
      forceEnabledWorkarounds(forceEnabledWorkarounds),
      forceDisabledWorkarounds(forceDisabledWorkarounds) {
}

BackendTestConfig D3D12Backend(std::initializer_list<const char*> forceEnabledWorkarounds,
                               std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::D3D12, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

BackendTestConfig MetalBackend(std::initializer_list<const char*> forceEnabledWorkarounds,
                               std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::Metal, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

BackendTestConfig NullBackend(std::initializer_list<const char*> forceEnabledWorkarounds,
                              std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::Null, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

BackendTestConfig OpenGLBackend(std::initializer_list<const char*> forceEnabledWorkarounds,
                                std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::OpenGL, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

BackendTestConfig OpenGLESBackend(std::initializer_list<const char*> forceEnabledWorkarounds,
                                  std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::OpenGLES, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

BackendTestConfig VulkanBackend(std::initializer_list<const char*> forceEnabledWorkarounds,
                                std::initializer_list<const char*> forceDisabledWorkarounds) {
    return BackendTestConfig(wgpu::BackendType::Vulkan, forceEnabledWorkarounds,
                             forceDisabledWorkarounds);
}

AdapterTestParam::AdapterTestParam(
    const BackendTestConfig& config,
    const utils::InstanceHolder::AdapterProperties& adapterProperties)
    : adapterProperties(adapterProperties),
      forceEnabledWorkarounds(config.forceEnabledWorkarounds),
      forceDisabledWorkarounds(config.forceDisabledWorkarounds) {
}

std::ostream& operator<<(std::ostream& os, const AdapterTestParam& param) {
    // Sanitize the adapter name for GoogleTest
    std::string sanitizedName =
        std::regex_replace(param.adapterProperties.adapterName, std::regex("[^a-zA-Z0-9]+"), "_");

    // Strip trailing underscores, if any.
    if (sanitizedName.back() == '_') {
        sanitizedName.back() = '\0';
    }

    os << ParamName(param.adapterProperties.backendType) << "_" << sanitizedName.c_str();

    // In a Windows Remote Desktop session there are two adapters named "Microsoft Basic Render
    // Driver" with different adapter types. We must differentiate them to avoid any tests using the
    // same name.
    if (param.adapterProperties.deviceID == 0x008C) {
        std::string adapterType = AdapterTypeName(param.adapterProperties.adapterType);
        std::replace(adapterType.begin(), adapterType.end(), ' ', '_');
        os << "_" << adapterType;
    }

    for (const char* forceEnabledWorkaround : param.forceEnabledWorkarounds) {
        os << "__e_" << forceEnabledWorkaround;
    }
    for (const char* forceDisabledWorkaround : param.forceDisabledWorkarounds) {
        os << "__d_" << forceDisabledWorkaround;
    }
    return os;
}

// Implementation of DawnTestEnvironment

void InitDawnEnd2EndTestEnvironment(int argc, char** argv) {
    gTestEnv = new DawnTestEnvironment(argc, argv);
    testing::AddGlobalTestEnvironment(gTestEnv);
}

// static
void DawnTestEnvironment::SetEnvironment(DawnTestEnvironment* env) {
    gTestEnv = env;
}

DawnTestEnvironment::DawnTestEnvironment(int argc, char** argv) {
    // Create a temporary instance to select available and preferred adapters. This is done
    // before test instantiation so GetAvailableAdapterTestParamsForBackends can generate test
    // parameterizations all selected adapters. We drop the instance at the end of this
    // function because the Vulkan validation layers use static global mutexes which behave badly
    // when Chromium's test launcher forks the test process. The instance will be recreated on
    // test environment setup.

    mOptions = utils::InstanceHolder::Options::FromCommandLine(argc, argv);
    utils::InstanceHolder config(mOptions);

    config.DiscoverDefaultAdapters();
    mAdapterProperties = config.ComputeSelectedAdapterProperties();

    auto log = dawn::InfoLog();
    log << "Dawn test configuration\n";
    log << "-----------------------\n";
    {
        std::ostringstream ss;
        mOptions.Print(ss, config.GetInstance());
        log << ss.str();
    }
    log << "\n\nSystem adapters:\n";
    for (const auto& properties : mAdapterProperties) {
        std::ostringstream ss;
        ss << properties;
        log << ss.str();
    }
    log << "-----------------------\n";

    if (mOptions.enableBackendValidation) {
        mPlatformDebugLogger =
            std::unique_ptr<utils::PlatformDebugLogger>(utils::CreatePlatformDebugLogger());
    }
}

DawnTestEnvironment::~DawnTestEnvironment() = default;

std::vector<AdapterTestParam> DawnTestEnvironment::GetAvailableAdapterTestParamsForBackends(
    const BackendTestConfig* params,
    size_t numParams) {
    std::vector<AdapterTestParam> testParams;
    for (size_t i = 0; i < numParams; ++i) {
        for (const auto& adapterProperties : mAdapterProperties) {
            if (params[i].backendType == adapterProperties.backendType &&
                adapterProperties.selected) {
                testParams.push_back(AdapterTestParam(params[i], adapterProperties));

                // HACK: This is a hack to get Tint generator enabled on all tests
                // without adding a new test suite in Chromium's infra config but skipping
                // that suite on all unsupported platforms. Once we have basic functionality and
                // test skips on all backends, we can remove this and use a test suite with
                // use_tint_generator in the command line args instead.
                if (params[i].backendType == wgpu::BackendType::Vulkan ||
                    params[i].backendType == wgpu::BackendType::OpenGL ||
                    params[i].backendType == wgpu::BackendType::OpenGLES) {
                    BackendTestConfig configWithTint = params[i];
                    configWithTint.forceEnabledWorkarounds.push_back("use_tint_generator");
                    testParams.push_back(AdapterTestParam(configWithTint, adapterProperties));
                }
            }
        }
    }
    return testParams;
}

void DawnTestEnvironment::SetUp() {
    mInstanceHolder = std::make_unique<utils::InstanceHolder>(mOptions);
    mInstanceHolder->DiscoverDefaultAdapters();
}

void DawnTestEnvironment::TearDown() {
    // When Vulkan validation layers are enabled, it's unsafe to call Vulkan APIs in the destructor
    // of a static/global variable, so the instance must be manually released beforehand.
    mInstanceHolder.reset();
}

class WireServerTraceLayer : public dawn_wire::CommandHandler {
  public:
    WireServerTraceLayer(const char* file, dawn_wire::CommandHandler* handler)
        : dawn_wire::CommandHandler(), mHandler(handler) {
        mFile.open(file, std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    }

    const volatile char* HandleCommands(const volatile char* commands, size_t size) override {
        mFile.write(const_cast<const char*>(commands), size);
        return mHandler->HandleCommands(commands, size);
    }

  private:
    dawn_wire::CommandHandler* mHandler;
    std::ofstream mFile;
};

// Implementation of DawnTest

DawnTestBase::DawnTestBase(const AdapterTestParam& param) : mParam(param) {
}

DawnTestBase::~DawnTestBase() {
    // We need to destroy child objects before the Device
    mReadbackSlots.clear();
    queue = wgpu::Queue();
    device = wgpu::Device();
    backendDevice = nullptr;

    FlushWire();
}

bool DawnTestBase::IsD3D12() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::D3D12;
}

bool DawnTestBase::IsMetal() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::Metal;
}

bool DawnTestBase::IsNull() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::Null;
}

bool DawnTestBase::IsOpenGL() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::OpenGL;
}

bool DawnTestBase::IsOpenGLES() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::OpenGLES;
}

bool DawnTestBase::IsVulkan() const {
    return mParam.adapterProperties.backendType == wgpu::BackendType::Vulkan;
}

bool DawnTestBase::IsAMD() const {
    return gpu_info::IsAMD(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsARM() const {
    return gpu_info::IsARM(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsImgTec() const {
    return gpu_info::IsImgTec(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsIntel() const {
    return gpu_info::IsIntel(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsNvidia() const {
    return gpu_info::IsNvidia(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsQualcomm() const {
    return gpu_info::IsQualcomm(mParam.adapterProperties.vendorID);
}

bool DawnTestBase::IsSwiftshader() const {
    return gpu_info::IsSwiftshader(mParam.adapterProperties.vendorID,
                                   mParam.adapterProperties.deviceID);
}

bool DawnTestBase::IsANGLE() const {
    return !mParam.adapterProperties.adapterName.find("ANGLE");
}

bool DawnTestBase::IsWARP() const {
    return gpu_info::IsWARP(mParam.adapterProperties.vendorID, mParam.adapterProperties.deviceID);
}

bool DawnTestBase::IsWindows() const {
#ifdef DAWN_PLATFORM_WINDOWS
    return true;
#else
    return false;
#endif
}

bool DawnTestBase::IsLinux() const {
#ifdef DAWN_PLATFORM_LINUX
    return true;
#else
    return false;
#endif
}

bool DawnTestBase::IsMacOS() const {
#ifdef DAWN_PLATFORM_APPLE
    return true;
#else
    return false;
#endif
}

bool DawnTestBase::UsesWire() const {
    return gTestEnv->GetInstanceHolder()->GetOptions().useWire;
}

bool DawnTestBase::IsBackendValidationEnabled() const {
    return gTestEnv->GetInstanceHolder()->GetOptions().enableBackendValidation;
}

bool DawnTestBase::HasWGSL() const {
#ifdef DAWN_ENABLE_WGSL
    return true;
#else
    return false;
#endif
}

bool DawnTestBase::IsAsan() const {
#if defined(ADDRESS_SANITIZER)
    return true;
#else
    return false;
#endif
}

bool DawnTestBase::HasToggleEnabled(const char* toggle) const {
    auto toggles = dawn_native::GetTogglesUsed(backendDevice);
    return std::find_if(toggles.begin(), toggles.end(), [toggle](const char* name) {
               return strcmp(toggle, name) == 0;
           }) != toggles.end();
}

bool DawnTestBase::HasVendorIdFilter() const {
    return gTestEnv->GetInstanceHolder()->GetOptions().hasDefaultVendorIdFilter;
}

uint32_t DawnTestBase::GetVendorIdFilter() const {
    return gTestEnv->GetInstanceHolder()->GetOptions().defaultVendorIdFilter;
}

wgpu::Instance DawnTestBase::GetInstance() const {
    return gTestEnv->GetInstanceHolder()->GetInstance()->Get();
}

dawn_native::Adapter DawnTestBase::GetAdapter() const {
    return mBackendAdapter;
}

std::vector<const char*> DawnTestBase::GetRequiredExtensions() {
    return {};
}

const wgpu::AdapterProperties& DawnTestBase::GetAdapterProperties() const {
    return mParam.adapterProperties;
}

bool DawnTestBase::SupportsExtensions(const std::vector<const char*>& extensions) {
    ASSERT(mBackendAdapter);
    std::set<std::string> supportedExtensionsSet;
    for (const char* supportedExtensionName : mBackendAdapter.GetSupportedExtensions()) {
        supportedExtensionsSet.insert(supportedExtensionName);
    }

    for (const char* extensionName : extensions) {
        if (supportedExtensionsSet.find(extensionName) == supportedExtensionsSet.end()) {
            return false;
        }
    }

    return true;
}

void DawnTestBase::SetUp() {
    // Ensure the procs have been set to the default in case a previous test set them.
    gTestEnv->GetInstanceHolder()->EnsureProcs();
    dawn_native::Instance* instance = gTestEnv->GetInstanceHolder()->GetInstance();

    {
        // Find the adapter that exactly matches our adapter properties.
        const auto& adapters = instance->GetAdapters();
        const auto& it = std::find_if(
            adapters.begin(), adapters.end(), [&](const dawn_native::Adapter& adapter) {
                wgpu::AdapterProperties properties;
                adapter.GetProperties(&properties);

                return (mParam.adapterProperties.selected &&
                        properties.deviceID == mParam.adapterProperties.deviceID &&
                        properties.vendorID == mParam.adapterProperties.vendorID &&
                        properties.adapterType == mParam.adapterProperties.adapterType &&
                        properties.backendType == mParam.adapterProperties.backendType &&
                        strcmp(properties.name, mParam.adapterProperties.adapterName.c_str()) == 0);
            });
        ASSERT(it != adapters.end());
        mBackendAdapter = *it;
    }

    // Setup the per-test platform. Tests can provide one by overloading CreateTestPlatform.
    mTestPlatform = CreateTestPlatform();
    instance->SetPlatform(mTestPlatform.get());

    // Create the device from the adapter
    for (const char* forceEnabledWorkaround : mParam.forceEnabledWorkarounds) {
        ASSERT(instance->GetToggleInfo(forceEnabledWorkaround) != nullptr);
    }
    for (const char* forceDisabledWorkaround : mParam.forceDisabledWorkarounds) {
        ASSERT(instance->GetToggleInfo(forceDisabledWorkaround) != nullptr);
    }
    dawn_native::DeviceDescriptor deviceDescriptor;
    deviceDescriptor.forceEnabledToggles = mParam.forceEnabledWorkarounds;
    deviceDescriptor.forceDisabledToggles = mParam.forceDisabledWorkarounds;
    deviceDescriptor.requiredExtensions = GetRequiredExtensions();

    for (const std::string& toggle : gTestEnv->GetInstanceHolder()->GetOptions().enabledToggles) {
        const dawn_native::ToggleInfo* info = instance->GetToggleInfo(toggle.c_str());
        ASSERT(info != nullptr);
        deviceDescriptor.forceEnabledToggles.push_back(info->name);
    }

    for (const std::string& toggle : gTestEnv->GetInstanceHolder()->GetOptions().disabledToggles) {
        const dawn_native::ToggleInfo* info = instance->GetToggleInfo(toggle.c_str());
        ASSERT(info != nullptr);
        deviceDescriptor.forceDisabledToggles.push_back(info->name);
    }

    mScopedWireTrace = gTestEnv->GetInstanceHolder()->BeginScopedWireTrace(
        std::string(::testing::UnitTest::GetInstance()->current_test_info()->test_suite_name()) +
        "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());

    std::tie(device, backendDevice) = gTestEnv->GetInstanceHolder()->RegisterDevice(
        mBackendAdapter.CreateDevice(&deviceDescriptor));

    queue = device.GetDefaultQueue();

    device.SetUncapturedErrorCallback(OnDeviceError, this);
    device.SetDeviceLostCallback(OnDeviceLost, this);
#if defined(DAWN_ENABLE_BACKEND_OPENGL)
    if (IsOpenGL()) {
        glfwMakeContextCurrent(
            gTestEnv->GetInstanceHolder()->GetGLFWwindow(wgpu::BackendType::OpenGL));
    } else if (IsOpenGLES()) {
        glfwMakeContextCurrent(
            gTestEnv->GetInstanceHolder()->GetGLFWwindow(wgpu::BackendType::OpenGLES));
    }
#endif

    // A very large number of tests hang on Intel D3D12 with the debug adapter after a driver
    // upgrade. Violently suppress this whole configuration until we figure out what to do.
    // See https://crbug.com/dawn/598
    DAWN_SKIP_TEST_IF(IsBackendValidationEnabled() && IsIntel() && IsD3D12());
}

void DawnTestBase::TearDown() {
    FlushWire();

    MapSlotsSynchronously();
    ResolveExpectations();

    for (size_t i = 0; i < mReadbackSlots.size(); ++i) {
        mReadbackSlots[i].buffer.Unmap();
    }

    if (!UsesWire()) {
        EXPECT_EQ(mLastWarningCount,
                  dawn_native::GetDeprecationWarningCountForTesting(device.Get()));
    }

    mScopedWireTrace.reset();
}

void DawnTestBase::StartExpectDeviceError() {
    mExpectError = true;
    mError = false;
}
bool DawnTestBase::EndExpectDeviceError() {
    mExpectError = false;
    return mError;
}

// static
void DawnTestBase::OnDeviceError(WGPUErrorType type, const char* message, void* userdata) {
    ASSERT(type != WGPUErrorType_NoError);
    DawnTestBase* self = static_cast<DawnTestBase*>(userdata);

    ASSERT_TRUE(self->mExpectError) << "Got unexpected device error: " << message;
    ASSERT_FALSE(self->mError) << "Got two errors in expect block";
    self->mError = true;
}

void DawnTestBase::OnDeviceLost(const char* message, void* userdata) {
    // Using ADD_FAILURE + ASSERT instead of FAIL to prevent the current test from continuing with a
    // corrupt state.
    ADD_FAILURE() << "Device Lost during test: " << message;
    ASSERT(false);
}

std::ostringstream& DawnTestBase::AddBufferExpectation(const char* file,
                                                       int line,
                                                       const wgpu::Buffer& buffer,
                                                       uint64_t offset,
                                                       uint64_t size,
                                                       detail::Expectation* expectation) {
    auto readback = ReserveReadback(size);

    // We need to enqueue the copy immediately because by the time we resolve the expectation,
    // the buffer might have been modified.
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToBuffer(buffer, offset, readback.buffer, readback.offset, size);

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    DeferredExpectation deferred;
    deferred.file = file;
    deferred.line = line;
    deferred.readbackSlot = readback.slot;
    deferred.readbackOffset = readback.offset;
    deferred.size = size;
    deferred.rowBytes = size;
    deferred.bytesPerRow = size;
    deferred.expectation.reset(expectation);

    mDeferredExpectations.push_back(std::move(deferred));
    mDeferredExpectations.back().message = std::make_unique<std::ostringstream>();
    return *(mDeferredExpectations.back().message.get());
}

std::ostringstream& DawnTestBase::AddTextureExpectationImpl(const char* file,
                                                            int line,
                                                            detail::Expectation* expectation,
                                                            const wgpu::Texture& texture,
                                                            uint32_t x,
                                                            uint32_t y,
                                                            uint32_t width,
                                                            uint32_t height,
                                                            uint32_t level,
                                                            uint32_t slice,
                                                            wgpu::TextureAspect aspect,
                                                            uint32_t dataSize,
                                                            uint32_t bytesPerRow) {
    if (bytesPerRow == 0) {
        bytesPerRow = Align(width * dataSize, kTextureBytesPerRowAlignment);
    } else {
        ASSERT(bytesPerRow >= width * dataSize);
        ASSERT(bytesPerRow == Align(bytesPerRow, kTextureBytesPerRowAlignment));
    }

    uint32_t rowsPerImage = height;
    uint32_t depth = 1;
    uint32_t size =
        utils::RequiredBytesInCopy(bytesPerRow, rowsPerImage, width, height, depth, dataSize);

    // TODO(enga): We should have the map async alignment in Contants.h. Also, it should change to 8
    // for Float64Array.
    auto readback = ReserveReadback(Align(size, 4));

    // We need to enqueue the copy immediately because by the time we resolve the expectation,
    // the texture might have been modified.
    wgpu::TextureCopyView textureCopyView =
        utils::CreateTextureCopyView(texture, level, {x, y, slice}, aspect);
    wgpu::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(readback.buffer, readback.offset, bytesPerRow, rowsPerImage);
    wgpu::Extent3D copySize = {width, height, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&textureCopyView, &bufferCopyView, &copySize);

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    DeferredExpectation deferred;
    deferred.file = file;
    deferred.line = line;
    deferred.readbackSlot = readback.slot;
    deferred.readbackOffset = readback.offset;
    deferred.size = size;
    deferred.rowBytes = width * dataSize;
    deferred.bytesPerRow = bytesPerRow;
    deferred.expectation.reset(expectation);

    mDeferredExpectations.push_back(std::move(deferred));
    mDeferredExpectations.back().message = std::make_unique<std::ostringstream>();
    return *(mDeferredExpectations.back().message.get());
}

void DawnTestBase::WaitABit() {
    device.Tick();
    FlushWire();

    utils::USleep(100);
}

void DawnTestBase::FlushWire() {
    gTestEnv->GetInstanceHolder()->FlushWire();
}

void DawnTestBase::WaitForAllOperations() {
    wgpu::Queue queue = device.GetDefaultQueue();
    wgpu::Fence fence = queue.CreateFence();

    // Force the currently submitted operations to completed.
    queue.Signal(fence, 1);
    while (fence.GetCompletedValue() < 1) {
        WaitABit();
    }
}

DawnTestBase::ReadbackReservation DawnTestBase::ReserveReadback(uint64_t readbackSize) {
    // For now create a new MapRead buffer for each readback
    // TODO(cwallez@chromium.org): eventually make bigger buffers and allocate linearly?
    ReadbackSlot slot;
    slot.bufferSize = readbackSize;

    // Create and initialize the slot buffer so that it won't unexpectedly affect the count of
    // resource lazy clear in the tests.
    const std::vector<uint8_t> initialBufferData(readbackSize, 0u);
    slot.buffer =
        utils::CreateBufferFromData(device, initialBufferData.data(), readbackSize,
                                    wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    ReadbackReservation reservation;
    reservation.buffer = slot.buffer;
    reservation.slot = mReadbackSlots.size();
    reservation.offset = 0;

    mReadbackSlots.push_back(std::move(slot));
    return reservation;
}

void DawnTestBase::MapSlotsSynchronously() {
    // Initialize numPendingMapOperations before mapping, just in case the callback is called
    // immediately.
    mNumPendingMapOperations = mReadbackSlots.size();

    // Map all readback slots
    for (size_t i = 0; i < mReadbackSlots.size(); ++i) {
        MapReadUserdata* userdata = new MapReadUserdata{this, i};

        const ReadbackSlot& slot = mReadbackSlots[i];
        slot.buffer.MapAsync(wgpu::MapMode::Read, 0, 0, SlotMapCallback, userdata);
    }

    // Busy wait until all map operations are done.
    while (mNumPendingMapOperations != 0) {
        WaitABit();
    }
}

// static
void DawnTestBase::SlotMapCallback(WGPUBufferMapAsyncStatus status, void* userdata_) {
    DAWN_ASSERT(status == WGPUBufferMapAsyncStatus_Success);

    std::unique_ptr<MapReadUserdata> userdata(static_cast<MapReadUserdata*>(userdata_));
    DawnTestBase* test = userdata->test;
    ReadbackSlot* slot = &test->mReadbackSlots[userdata->slot];

    slot->mappedData = slot->buffer.GetConstMappedRange();
    test->mNumPendingMapOperations--;
}

void DawnTestBase::ResolveExpectations() {
    for (const auto& expectation : mDeferredExpectations) {
        DAWN_ASSERT(mReadbackSlots[expectation.readbackSlot].mappedData != nullptr);

        // Get a pointer to the mapped copy of the data for the expectation.
        const char* data =
            static_cast<const char*>(mReadbackSlots[expectation.readbackSlot].mappedData);
        data += expectation.readbackOffset;

        uint32_t size;
        std::vector<char> packedData;
        if (expectation.rowBytes != expectation.bytesPerRow) {
            DAWN_ASSERT(expectation.bytesPerRow > expectation.rowBytes);
            uint32_t rowCount =
                (expectation.size + expectation.bytesPerRow - 1) / expectation.bytesPerRow;
            uint32_t packedSize = rowCount * expectation.rowBytes;
            packedData.resize(packedSize);
            for (uint32_t r = 0; r < rowCount; ++r) {
                for (uint32_t i = 0; i < expectation.rowBytes; ++i) {
                    packedData[i + r * expectation.rowBytes] =
                        data[i + r * expectation.bytesPerRow];
                }
            }
            data = packedData.data();
            size = packedSize;
        } else {
            size = expectation.size;
        }

        // Get the result for the expectation and add context to failures
        testing::AssertionResult result = expectation.expectation->Check(data, size);
        if (!result) {
            result << " Expectation created at " << expectation.file << ":" << expectation.line
                   << std::endl;
            result << expectation.message->str();
        }

        EXPECT_TRUE(result);
    }
}

std::unique_ptr<dawn_platform::Platform> DawnTestBase::CreateTestPlatform() {
    return nullptr;
}

bool RGBA8::operator==(const RGBA8& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
}

bool RGBA8::operator!=(const RGBA8& other) const {
    return !(*this == other);
}

bool RGBA8::operator<=(const RGBA8& other) const {
    return (r <= other.r && g <= other.g && b <= other.b && a <= other.a);
}

bool RGBA8::operator>=(const RGBA8& other) const {
    return (r >= other.r && g >= other.g && b >= other.b && a >= other.a);
}

std::ostream& operator<<(std::ostream& stream, const RGBA8& color) {
    return stream << "RGBA8(" << static_cast<int>(color.r) << ", " << static_cast<int>(color.g)
                  << ", " << static_cast<int>(color.b) << ", " << static_cast<int>(color.a) << ")";
}

namespace detail {
    std::vector<AdapterTestParam> GetAvailableAdapterTestParamsForBackends(
        const BackendTestConfig* params,
        size_t numParams) {
        ASSERT(gTestEnv != nullptr);
        return gTestEnv->GetAvailableAdapterTestParamsForBackends(params, numParams);
    }

    // Helper classes to set expectations

    template <typename T>
    ExpectEq<T>::ExpectEq(T singleValue) {
        mExpected.push_back(singleValue);
    }

    template <typename T>
    ExpectEq<T>::ExpectEq(const T* values, const unsigned int count) {
        mExpected.assign(values, values + count);
    }

    template <typename T>
    testing::AssertionResult ExpectEq<T>::Check(const void* data, size_t size) {
        DAWN_ASSERT(size == sizeof(T) * mExpected.size());

        const T* actual = static_cast<const T*>(data);

        for (size_t i = 0; i < mExpected.size(); ++i) {
            if (actual[i] != mExpected[i]) {
                testing::AssertionResult result = testing::AssertionFailure()
                                                  << "Expected data[" << i << "] to be "
                                                  << mExpected[i] << ", actual " << actual[i]
                                                  << std::endl;

                if (mExpected.size() <= 1024) {
                    result << "Expected:" << std::endl;
                    printBuffer(result, mExpected.data(), mExpected.size());

                    result << "Actual:" << std::endl;
                    printBuffer(result, actual, mExpected.size());
                }

                return result;
            }
        }

        return testing::AssertionSuccess();
    }

    template class ExpectEq<uint8_t>;
    template class ExpectEq<uint16_t>;
    template class ExpectEq<uint32_t>;
    template class ExpectEq<uint64_t>;
    template class ExpectEq<RGBA8>;
    template class ExpectEq<float>;

    template <typename T>
    ExpectBetweenColors<T>::ExpectBetweenColors(T value0, T value1) {
        T l, h;
        l.r = std::min(value0.r, value1.r);
        l.g = std::min(value0.g, value1.g);
        l.b = std::min(value0.b, value1.b);
        l.a = std::min(value0.a, value1.a);

        h.r = std::max(value0.r, value1.r);
        h.g = std::max(value0.g, value1.g);
        h.b = std::max(value0.b, value1.b);
        h.a = std::max(value0.a, value1.a);

        mLowerColorChannels.push_back(l);
        mHigherColorChannels.push_back(h);

        mValues0.push_back(value0);
        mValues1.push_back(value1);
    }

    template <typename T>
    testing::AssertionResult ExpectBetweenColors<T>::Check(const void* data, size_t size) {
        DAWN_ASSERT(size == sizeof(T) * mLowerColorChannels.size());
        DAWN_ASSERT(mHigherColorChannels.size() == mLowerColorChannels.size());
        DAWN_ASSERT(mValues0.size() == mValues1.size());
        DAWN_ASSERT(mValues0.size() == mLowerColorChannels.size());

        const T* actual = static_cast<const T*>(data);

        for (size_t i = 0; i < mLowerColorChannels.size(); ++i) {
            if (!(actual[i] >= mLowerColorChannels[i] && actual[i] <= mHigherColorChannels[i])) {
                testing::AssertionResult result = testing::AssertionFailure()
                                                  << "Expected data[" << i << "] to be between "
                                                  << mValues0[i] << " and " << mValues1[i]
                                                  << ", actual " << actual[i] << std::endl;

                if (mLowerColorChannels.size() <= 1024) {
                    result << "Expected between:" << std::endl;
                    printBuffer(result, mValues0.data(), mLowerColorChannels.size());
                    result << "and" << std::endl;
                    printBuffer(result, mValues1.data(), mLowerColorChannels.size());

                    result << "Actual:" << std::endl;
                    printBuffer(result, actual, mLowerColorChannels.size());
                }

                return result;
            }
        }

        return testing::AssertionSuccess();
    }

    template class ExpectBetweenColors<RGBA8>;
}  // namespace detail
