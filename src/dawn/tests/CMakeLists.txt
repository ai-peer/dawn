# Copyright 2024 The Dawn & Tint Authors
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if (NOT ${DAWN_BUILD_TESTS})
    return()
endif()

function(dawn_test_compile_options TARGET)
    common_compile_options(${TARGET})
    set_target_properties(${TARGET} PROPERTIES FOLDER "Tests")
endfunction()

###############################################################################
# Dawn mock API targets
###############################################################################

DawnJSONGenerator(
    TARGET "mock_api"
    PRINT_NAME "Dawn mock API"
    RESULT_VARIABLE "DAWN_MOCK_API_GEN_SOURCES"
)
add_library(dawn_mock_webgpu_gen STATIC ${DAWN_PLACEHOLDER_FILE})
common_compile_options(dawn_mock_webgpu_gen)
target_sources(dawn_mock_webgpu_gen PRIVATE ${DAWN_MOCK_API_GEN_SOURCES})
target_link_libraries(dawn_mock_webgpu_gen
    PUBLIC
    gmock
    PRIVATE
    dawn_common
    dawn_internal_config
)
if (MSVC)
    # Prevent fatal error C1128: number of sections exceeded object file format limit: compile with /bigobj
    target_compile_options(dawn_mock_webgpu_gen PRIVATE /bigobj)
endif (MSVC)

# Source code for mocks used for unit testing are separated from the rest of
# sources so that they aren't included in non-test builds.
add_library(dawn_native_mocks_sources STATIC ${DAWN_PLACEHOLDER_FILE})
dawn_test_compile_options(dawn_native_mocks_sources)
target_link_libraries(dawn_native_mocks_sources
    PUBLIC
    gtest
    dawn_mock_webgpu_gen
    dawn_proc
    dawn_native
    dawn_utils
    PRIVATE
    dawn_internal_config
    libtint
    absl_strings
)
target_sources(
    dawn_native_mocks_sources
    PRIVATE
    unittests/native/mocks/BindGroupLayoutMock.cpp
    unittests/native/mocks/BindGroupLayoutMock.h
    unittests/native/mocks/BindGroupMock.cpp
    unittests/native/mocks/BindGroupMock.h
    unittests/native/mocks/BufferMock.cpp
    unittests/native/mocks/BufferMock.h
    unittests/native/mocks/CommandBufferMock.cpp
    unittests/native/mocks/CommandBufferMock.h
    unittests/native/mocks/ComputePipelineMock.cpp
    unittests/native/mocks/ComputePipelineMock.h
    unittests/native/mocks/DawnMockTest.cpp
    unittests/native/mocks/DawnMockTest.h
    unittests/native/mocks/DeviceMock.cpp
    unittests/native/mocks/DeviceMock.h
    unittests/native/mocks/ExternalTextureMock.cpp
    unittests/native/mocks/ExternalTextureMock.h
    unittests/native/mocks/PipelineLayoutMock.cpp
    unittests/native/mocks/PipelineLayoutMock.h
    unittests/native/mocks/QuerySetMock.cpp
    unittests/native/mocks/QuerySetMock.h
    unittests/native/mocks/QueueMock.cpp
    unittests/native/mocks/QueueMock.h
    unittests/native/mocks/RenderPipelineMock.cpp
    unittests/native/mocks/RenderPipelineMock.h
    unittests/native/mocks/SamplerMock.cpp
    unittests/native/mocks/SamplerMock.h
    unittests/native/mocks/ShaderModuleMock.cpp
    unittests/native/mocks/ShaderModuleMock.h
    unittests/native/mocks/SwapChainMock.cpp
    unittests/native/mocks/SwapChainMock.h
    unittests/native/mocks/TextureMock.cpp
    unittests/native/mocks/TextureMock.h
)

add_library(dawn_platform_mocks_sources STATIC ${DAWN_PLACEHOLDER_FILE})
dawn_test_compile_options(dawn_platform_mocks_sources)
target_link_libraries(dawn_platform_mocks_sources
    PUBLIC
    gtest
    gmock
    dawn_common
    dawn_platform
    PRIVATE
    dawn_internal_config
    absl_strings
)
target_sources(
    dawn_platform_mocks_sources
    PRIVATE
    mocks/platform/CachingInterfaceMock.cpp
    mocks/platform/CachingInterfaceMock.h
)

###############################################################################
# Dawn unit tests targets
###############################################################################

add_executable(dawn_unittests)
dawn_test_compile_options(dawn_unittests)
target_link_libraries(dawn_unittests
    PRIVATE
    libtint
    dawn_native_mocks_sources
    dawn_platform_mocks_sources
    dawncpp
    dawn_proc
    dawn_common
    dawn_native
    dawn_utils
    dawn_wire
    absl_strings
    dawn_internal_config
)
if (MSVC)
    # Prevent fatal error C1128: number of sections exceeded object file format limit: compile with /bigobj
    target_compile_options(dawn_unittests PRIVATE /bigobj)
endif (MSVC)
set_target_properties(dawn_unittests
    PROPERTIES
    CXX_STANDARD 20 # Required by WGSLFeatureValidationTests
    CXX_STANDARD_REQUIRED ON
)
target_sources(dawn_unittests
    PRIVATE
    ${Dawn_SOURCE_DIR}/src/dawn/wire/client/ClientMemoryTransferService_mock.cpp
    ${Dawn_SOURCE_DIR}/src/dawn/wire/client/ClientMemoryTransferService_mock.h
    ${Dawn_SOURCE_DIR}/src/dawn/wire/server/ServerMemoryTransferService_mock.cpp
    ${Dawn_SOURCE_DIR}/src/dawn/wire/server/ServerMemoryTransferService_mock.h
    DawnNativeTest.cpp
    DawnNativeTest.h
    MockCallback.h
    ParamGenerator.h
    ToggleParser.cpp
    ToggleParser.h
    unittests/AsyncTaskTests.cpp
    unittests/BitSetIteratorTests.cpp
    unittests/BuddyAllocatorTests.cpp
    unittests/BuddyMemoryAllocatorTests.cpp
    unittests/ChainUtilsTests.cpp
    unittests/CommandAllocatorTests.cpp
    unittests/ConcurrentCacheTests.cpp
    unittests/ContentLessObjectCacheTests.cpp
    unittests/EnumClassBitmasksTests.cpp
    unittests/EnumMaskIteratorTests.cpp
    unittests/EnumeratorTests.cpp
    unittests/ErrorTests.cpp
    unittests/FeatureTests.cpp
    unittests/GPUInfoTests.cpp
    unittests/GetProcAddressTests.cpp
    unittests/ITypArrayTests.cpp
    unittests/ITypBitsetTests.cpp
    unittests/ITypSpanTests.cpp
    unittests/ITypVectorTests.cpp
    unittests/LinkedListTests.cpp
    unittests/MathTests.cpp
    unittests/MutexProtectedTests.cpp
    unittests/MutexTests.cpp
    unittests/NumericTests.cpp
    unittests/ObjectBaseTests.cpp
    unittests/PerStageTests.cpp
    unittests/PerThreadProcTests.cpp
    unittests/PlacementAllocatedTests.cpp
    unittests/RangeTests.cpp
    unittests/RefBaseTests.cpp
    unittests/RefCountedTests.cpp
    unittests/ResultTests.cpp
    unittests/RingBufferAllocatorTests.cpp
    unittests/SerialMapTests.cpp
    unittests/SerialQueueTests.cpp
    unittests/SlabAllocatorTests.cpp
    unittests/StackContainerTests.cpp
    unittests/SubresourceStorageTests.cpp
    unittests/SystemUtilsTests.cpp
    unittests/ToBackendTests.cpp
    unittests/ToggleTests.cpp
    unittests/TypedIntegerTests.cpp
    unittests/UnicodeTests.cpp
    unittests/WeakRefTests.cpp
    unittests/native/AllowedErrorTests.cpp
    unittests/native/BlobTests.cpp
    unittests/native/CacheRequestTests.cpp
    unittests/native/CommandBufferEncodingTests.cpp
    unittests/native/CreatePipelineAsyncTaskTests.cpp
    unittests/native/DestroyObjectTests.cpp
    unittests/native/DeviceAsyncTaskTests.cpp
    unittests/native/DeviceCreationTests.cpp
    unittests/native/LimitsTests.cpp
    unittests/native/ObjectContentHasherTests.cpp
    unittests/native/StreamTests.cpp
    unittests/validation/BindGroupValidationTests.cpp
    unittests/validation/BufferValidationTests.cpp
    unittests/validation/CommandBufferValidationTests.cpp
    unittests/validation/CompatValidationTests.cpp
    unittests/validation/ComputeIndirectValidationTests.cpp
    unittests/validation/ComputeValidationTests.cpp
    unittests/validation/CopyCommandsValidationTests.cpp
    unittests/validation/CopyTextureForBrowserTests.cpp
    unittests/validation/DebugMarkerValidationTests.cpp
    unittests/validation/DeviceValidationTests.cpp
    unittests/validation/DrawIndirectValidationTests.cpp
    unittests/validation/DrawVertexAndIndexBufferOOBValidationTests.cpp
    unittests/validation/DynamicStateCommandValidationTests.cpp
    unittests/validation/ErrorScopeValidationTests.cpp
    unittests/validation/ExternalTextureTests.cpp
    unittests/validation/GetBindGroupLayoutValidationTests.cpp
    unittests/validation/IndexBufferValidationTests.cpp
    unittests/validation/InternalUsageValidationTests.cpp
    unittests/validation/LabelTests.cpp
    unittests/validation/MinimumBufferSizeValidationTests.cpp
    unittests/validation/MultipleDeviceTests.cpp
    unittests/validation/ObjectCachingTests.cpp
    unittests/validation/OverridableConstantsValidationTests.cpp
    unittests/validation/PipelineAndPassCompatibilityTests.cpp
    unittests/validation/PixelLocalStorageTests.cpp
    unittests/validation/QueryValidationTests.cpp
    unittests/validation/QueueOnSubmittedWorkDoneValidationTests.cpp
    unittests/validation/QueueSubmitValidationTests.cpp
    unittests/validation/QueueWriteBufferValidationTests.cpp
    unittests/validation/QueueWriteTextureValidationTests.cpp
    unittests/validation/RenderBundleValidationTests.cpp
    unittests/validation/RenderPassDescriptorValidationTests.cpp
    unittests/validation/RenderPipelineValidationTests.cpp
    unittests/validation/ResourceUsageTrackingTests.cpp
    unittests/validation/SamplerValidationTests.cpp
    unittests/validation/ShaderModuleValidationTests.cpp
    unittests/validation/StorageTextureValidationTests.cpp
    unittests/validation/SurfaceConfigurationValidationTests.cpp
    unittests/validation/TextureSubresourceTests.cpp
    unittests/validation/TextureValidationTests.cpp
    unittests/validation/TextureViewValidationTests.cpp
    unittests/validation/ToggleValidationTests.cpp
    unittests/validation/UnsafeAPIValidationTests.cpp
    unittests/validation/ValidationTest.cpp
    unittests/validation/ValidationTest.h
    unittests/validation/VertexBufferValidationTests.cpp
    unittests/validation/VertexStateValidationTests.cpp
    unittests/validation/WGSLFeatureValidationTests.cpp
    unittests/validation/WritableBufferBindingAliasingValidationTests.cpp
    unittests/validation/WritableTextureBindingAliasingValidationTests.cpp
    unittests/validation/WriteBufferTests.cpp
    unittests/wire/WireAdapterTests.cpp
    unittests/wire/WireArgumentTests.cpp
    unittests/wire/WireBasicTests.cpp
    unittests/wire/WireBufferMappingTests.cpp
    unittests/wire/WireCreatePipelineAsyncTests.cpp
    unittests/wire/WireDeviceLifetimeTests.cpp
    unittests/wire/WireDisconnectTests.cpp
    unittests/wire/WireErrorCallbackTests.cpp
    unittests/wire/WireExtensionTests.cpp
    unittests/wire/WireFutureTest.cpp
    unittests/wire/WireFutureTest.h
    unittests/wire/WireInjectDeviceTests.cpp
    unittests/wire/WireInjectInstanceTests.cpp
    unittests/wire/WireInjectSwapChainTests.cpp
    unittests/wire/WireInjectTextureTests.cpp
    unittests/wire/WireInstanceTests.cpp
    unittests/wire/WireMemoryTransferServiceTests.cpp
    unittests/wire/WireOptionalTests.cpp
    unittests/wire/WireQueueTests.cpp
    unittests/wire/WireShaderModuleTests.cpp
    unittests/wire/WireTest.cpp
    unittests/wire/WireTest.h
    UnittestsMain.cpp
)
if (WIN32)
    target_sources(dawn_unittests PRIVATE
        unittests/WindowsUtilsTests.cpp
    )
endif (WIN32)

if (DAWN_ENABLE_D3D12)
    target_sources(dawn_unittests PRIVATE
        d3d12/CopySplitTests.cpp
    )
endif (DAWN_ENABLE_D3D12)

# TODO Port other test targets (perf and end2end)?
