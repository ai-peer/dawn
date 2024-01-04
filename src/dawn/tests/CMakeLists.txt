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

# NB: The options 'is_chromeos' and 'build_with_chromium' used in BUILD.gn are
# considered false in this ~equivalent CMakeLists.

# A source group is implemented as an INTERFACE library so that gtest can find
# tests through linked source groups in dawn_end2end_tests.
# Note that this is imperfect as it propagates the compile options/linked
# libraries of each source group to the whole dawn_end2end_tests target.
function (add_source_group TARGET)
	add_library(${TARGET} INTERFACE)
endfunction()
function (source_group_link_libraries TARGET)
	target_link_libraries(${TARGET} INTERFACE ${ARGN})
endfunction()
function (source_group_include_directories TARGET)
	target_include_directories(${TARGET} INTERFACE ${ARGN})
endfunction()
macro(source_group_sources TARGET)
	set(SRC "")
	foreach (maybe_relative_path ${ARGN})
		cmake_path(ABSOLUTE_PATH maybe_relative_path OUTPUT_VARIABLE absolute_path)
		list(APPEND SRC ${absolute_path})
	endforeach()
	source_group(${TARGET} FILES ${SRC})
	target_sources(${TARGET} INTERFACE ${SRC})
endmacro()

function(dawn_test_compile_options TARGET)
	common_compile_options(${TARGET})
	set_target_properties(${TARGET} PROPERTIES FOLDER "Tests")
endfunction()
function(dawn_test_source_group_compile_options TARGET)
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
endif()

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

# Source code for mocks used for platform testing are separated from the rest of
# sources so that they aren't included in non-test builds.
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
endif()
set_target_properties(dawn_unittests
	PROPERTIES
	CXX_STANDARD 20 # Required by WGSLFeatureValidationTests
	CXX_STANDARD_REQUIRED ON
)
target_sources(dawn_unittests
	PRIVATE
	"${Dawn_SOURCE_DIR}/src/dawn/wire/client/ClientMemoryTransferService_mock.cpp"
	"${Dawn_SOURCE_DIR}/src/dawn/wire/client/ClientMemoryTransferService_mock.h"
	"${Dawn_SOURCE_DIR}/src/dawn/wire/server/ServerMemoryTransferService_mock.cpp"
	"${Dawn_SOURCE_DIR}/src/dawn/wire/server/ServerMemoryTransferService_mock.h"
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
endif()

if (DAWN_ENABLE_D3D12)
	target_sources(dawn_unittests PRIVATE
		d3d12/CopySplitTests.cpp
	)
endif()

###############################################################################
# Dawn test infrastructure targets
###############################################################################

add_library(dawn_test_infra_sources STATIC ${DAWN_PLACEHOLDER_FILE})
dawn_test_compile_options(dawn_test_infra_sources)
target_link_libraries(dawn_test_infra_sources
	PUBLIC
	gmock
	gtest
	PRIVATE
	dawncpp
	dawn_proc
	dawn_common
	dawn_native
	dawn_utils
	dawn_wire
	dawn_internal_config
	libtint
	absl_strings
)
target_sources(
	dawn_test_infra_sources
	PRIVATE
	AdapterTestConfig.cpp
	AdapterTestConfig.h
	DawnTest.cpp
	DawnTest.h
	MockCallback.h
	ParamGenerator.h
	ToggleParser.cpp
	ToggleParser.h
)

if (DAWN_SUPPORTS_GLFW_FOR_WINDOWING)
	target_link_libraries(dawn_test_infra_sources
		PUBLIC
		glfw
		dawn_glfw
	)
endif()

###############################################################################
# Dawn end2end tests targets
###############################################################################

add_source_group(dawn_end2end_tests_sources)
dawn_test_source_group_compile_options(dawn_end2end_tests_sources)
source_group_link_libraries(dawn_end2end_tests_sources
	dawn_platform_mocks_sources
	dawn_test_infra_sources
	dawncpp
	dawn_proc
	dawn_common
	dawn_native
	dawn_platform
	dawn_utils
	dawn_wire
	dawn_internal_config
	absl_strings
)
source_group_sources(dawn_end2end_tests_sources
	end2end/AdapterCreationTests.cpp
	end2end/AdapterEnumerationTests.cpp
	end2end/BasicTests.cpp
	end2end/BindGroupTests.cpp
	end2end/BufferHostMappedPointerTests.cpp
	end2end/BufferHostMappedPointerTests.h
	end2end/BufferTests.cpp
	end2end/BufferZeroInitTests.cpp
	end2end/ClipSpaceTests.cpp
	end2end/ColorStateTests.cpp
	end2end/CommandEncoderTests.cpp
	end2end/CompressedTextureFormatTests.cpp
	end2end/ComputeCopyStorageBufferTests.cpp
	end2end/ComputeDispatchTests.cpp
	end2end/ComputeFlowControlTests.cpp
	end2end/ComputeLayoutMemoryBufferTests.cpp
	end2end/ComputeSharedMemoryTests.cpp
	end2end/ComputeStorageBufferBarrierTests.cpp
	end2end/CopyExternalTextureForBrowserTests.cpp
	end2end/CopyTests.cpp
	end2end/CopyTextureForBrowserTests.cpp
	end2end/CreatePipelineAsyncTests.cpp
	end2end/CullingTests.cpp
	end2end/DebugMarkerTests.cpp
	end2end/DepthBiasTests.cpp
	end2end/DepthStencilCopyTests.cpp
	end2end/DepthStencilLoadOpTests.cpp
	end2end/DepthStencilSamplingTests.cpp
	end2end/DepthStencilStateTests.cpp
	end2end/DestroyTests.cpp
	end2end/DeviceInitializationTests.cpp
	end2end/DeviceLifetimeTests.cpp
	end2end/DeviceLostTests.cpp
	end2end/DrawIndexedIndirectTests.cpp
	end2end/DrawIndexedTests.cpp
	end2end/DrawIndirectTests.cpp
	end2end/DrawTests.cpp
	end2end/DualSourceBlendTests.cpp
	end2end/DynamicBufferOffsetTests.cpp
	end2end/EntryPointTests.cpp
	end2end/EventTests.cpp
	end2end/ExperimentalSubgroupsTests.cpp
	end2end/ExternalTextureTests.cpp
	end2end/FirstIndexOffsetTests.cpp
	end2end/FragDepthTests.cpp
	end2end/FramebufferFetchTests.cpp
	end2end/GpuMemorySynchronizationTests.cpp
	end2end/HistogramTests.cpp
	end2end/IndexFormatTests.cpp
	end2end/MaxLimitTests.cpp
	end2end/MemoryAllocationStressTests.cpp
	end2end/MemoryHeapPropertiesTests.cpp
	end2end/MultisampledRenderingTests.cpp
	end2end/MultisampledSamplingTests.cpp
	end2end/MultithreadTests.cpp
	end2end/NonzeroBufferCreationTests.cpp
	end2end/NonzeroTextureCreationTests.cpp
	end2end/OpArrayLengthTests.cpp
	end2end/Packed4x8IntegerDotProductTests.cpp
	end2end/PipelineCachingTests.cpp
	end2end/PipelineLayoutTests.cpp
	end2end/PixelLocalStorageTests.cpp
	end2end/PrimitiveStateTests.cpp
	end2end/PrimitiveTopologyTests.cpp
	end2end/QueryTests.cpp
	end2end/QueueTests.cpp
	end2end/QueueTimelineTests.cpp
	end2end/ReadOnlyDepthStencilAttachmentTests.cpp
	end2end/RenderAttachmentTests.cpp
	end2end/RenderBundleTests.cpp
	end2end/RenderPassLoadOpTests.cpp
	end2end/RenderPassTests.cpp
	end2end/RequiredBufferSizeInCopyTests.cpp
	end2end/SamplerFilterAnisotropicTests.cpp
	end2end/SamplerTests.cpp
	end2end/ScissorTests.cpp
	end2end/ShaderF16Tests.cpp
	end2end/ShaderTests.cpp
	end2end/ShaderValidationTests.cpp
	end2end/StorageTextureTests.cpp
	end2end/SubresourceRenderAttachmentTests.cpp
	end2end/Texture3DTests.cpp
	end2end/TextureCorruptionTests.cpp
	end2end/TextureFormatTests.cpp
	end2end/TextureShaderBuiltinTests.cpp
	end2end/TextureSubresourceTests.cpp
	end2end/TextureViewTests.cpp
	end2end/TextureZeroInitTests.cpp
	end2end/VertexFormatTests.cpp
	end2end/VertexOnlyRenderPipelineTests.cpp
	end2end/VertexStateTests.cpp
	end2end/ViewportOrientationTests.cpp
	end2end/ViewportTests.cpp
)

if (DAWN_ENABLE_D3D11 OR DAWN_ENABLE_D3D12)
	source_group_link_libraries(dawn_end2end_tests_sources
		d3d11
		dxgi
	)

	source_group_sources(dawn_end2end_tests_sources
		end2end/D3DResourceWrappingTests.cpp
		end2end/VideoViewsTests_win.cpp
	)
endif()

if (DAWN_ENABLE_D3D12)
	source_group_sources(dawn_end2end_tests_sources
		end2end/D3D12CachingTests.cpp
	)
endif()

if (APPLE)
	if (DAWN_ENABLE_METAL)
		source_group_sources(dawn_end2end_tests_sources
			end2end/VideoViewsTests_mac.cpp
		)
	endif()
	source_group_sources(dawn_end2end_tests_sources
		end2end/BufferHostMappedPointerTests_apple.mm
	)

	source_group_link_libraries(dawn_end2end_tests_sources
		"-framework IOSurface"
	)

	# NB: enable_arc is ignored in this CMakelists (contrary to BUILD.gn)
endif()

if (APPLE OR UNIX)
	source_group_sources(dawn_end2end_tests_sources
		end2end/BufferHostMappedPointerTests_posix.cpp
	)
endif()

if (WIN32)
	source_group_sources(dawn_end2end_tests_sources
		end2end/BufferHostMappedPointerTests_win.cpp
	)
endif()

if (DAWN_SUPPORTS_GLFW_FOR_WINDOWING)
	source_group_sources(dawn_end2end_tests_sources
		end2end/SwapChainTests.cpp
		end2end/SwapChainValidationTests.cpp
		end2end/WindowSurfaceTests.cpp
	)
endif()

if (DAWN_ENABLE_D3D11 OR DAWN_ENABLE_D3D12 OR DAWN_ENABLE_METAL)
	source_group_sources(dawn_end2end_tests_sources
		end2end/VideoViewsTests.cpp
		end2end/VideoViewsTests.h
	)
endif()

add_source_group(dawn_white_box_tests_sources)
dawn_test_source_group_compile_options(dawn_white_box_tests_sources)
source_group_link_libraries(dawn_white_box_tests_sources
	dawn_test_infra_sources
	dawncpp
	dawn_proc
	dawn_common
	dawn_native
	dawn_utils
	dawn_wire
	dawn_internal_config
	libtint
	absl_strings
)
source_group_sources(dawn_white_box_tests_sources
	white_box/SharedTextureMemoryTests.cpp
	white_box/SharedTextureMemoryTests.h

	white_box/BufferAllocatedSizeTests.cpp
	white_box/InternalResolveAttachmentSampleTypeTests.cpp
	white_box/InternalResourceUsageTests.cpp
	white_box/InternalStorageBufferBindingTests.cpp
	white_box/QueryInternalShaderTests.cpp
)

if (APPLE)
	source_group_sources(dawn_white_box_tests_sources
		white_box/SharedTextureMemoryTests_apple.mm
	)
endif()

if (DAWN_ENABLE_D3D11 OR DAWN_ENABLE_D3D12)
	source_group_sources(dawn_white_box_tests_sources
		white_box/SharedTextureMemoryTests_win.cpp
	)
endif()

if (DAWN_ENABLE_VULKAN)
	source_group_link_libraries(dawn_white_box_tests_sources
		Vulkan-Headers
	)

	if (UNIX AND NOT APPLE)
		source_group_sources(dawn_white_box_tests_sources
			white_box/VulkanImageWrappingTests.cpp
			white_box/VulkanImageWrappingTests.h
			white_box/VulkanImageWrappingTests_DmaBuf.cpp
			white_box/VulkanImageWrappingTests_DmaBuf.h
			white_box/VulkanImageWrappingTests_OpaqueFD.cpp
			white_box/VulkanImageWrappingTests_OpaqueFD.h

			white_box/SharedTextureMemoryTests_dmabuf.cpp
			white_box/SharedTextureMemoryTests_opaquefd.cpp
		)
	endif()

	if (ANDROID)
		source_group_sources(dawn_white_box_tests_sources
			white_box/SharedTextureMemoryTests_android.cpp
		)
	endif()

	if (DAWN_ENABLE_ERROR_INJECTION)
		source_group_sources(dawn_white_box_tests_sources
			white_box/VulkanErrorInjectorTests.cpp
		)
	endif()
endif()

if (DAWN_ENABLE_D3D11)
	source_group_sources(dawn_white_box_tests_sources
		white_box/D3D11BufferTests.cpp
	)
endif()

if (DAWN_ENABLE_D3D12)
	source_group_sources(dawn_white_box_tests_sources
		white_box/D3D12DescriptorHeapTests.cpp
		white_box/D3D12ResidencyTests.cpp
		white_box/D3D12ResourceHeapTests.cpp
		white_box/GPUTimestampCalibrationTests.cpp
		white_box/GPUTimestampCalibrationTests.h
		white_box/GPUTimestampCalibrationTests_D3D12.cpp
	)
endif()

if (DAWN_ENABLE_METAL)
	source_group_sources(dawn_white_box_tests_sources
		white_box/GPUTimestampCalibrationTests.cpp
		white_box/GPUTimestampCalibrationTests.h
		white_box/GPUTimestampCalibrationTests_Metal.mm
		white_box/MetalAutoreleasePoolTests.mm
	)

	# NB: enable_arc is ignored in this CMakelists (contrary to BUILD.gn)
endif()

if (DAWN_ENABLE_OPENGLES)
	source_group_sources(dawn_white_box_tests_sources
		white_box/EGLImageWrappingTests.cpp
		white_box/GLTextureWrappingTests.cpp
	)
	source_group_include_directories(dawn_white_box_tests_sources
		"${Dawn_SOURCE_DIR}/third_party/khronos"
	)
endif()

add_executable(dawn_end2end_tests)
dawn_test_compile_options(dawn_end2end_tests)
target_link_libraries(dawn_end2end_tests
	PRIVATE
	dawn_end2end_tests_sources
	dawn_test_infra_sources
	dawn_white_box_tests_sources
	dawn_internal_config
)
target_sources(dawn_end2end_tests PRIVATE
	End2EndTestsMain.cpp
)
if (UNIX AND NOT APPLE)
	target_link_libraries(dawn_end2end_tests PRIVATE
		gbm
	)
endif()

###############################################################################
# Dawn perf tests
###############################################################################

add_executable(dawn_perf_tests)
dawn_test_compile_options(dawn_perf_tests)
target_link_libraries(dawn_perf_tests
	PRIVATE
	dawn_test_infra_sources
	dawncpp
	dawn_proc
	dawn_common
	dawn_platform
	dawn_utils
	dawn_wire
	dawn_internal_config
)
target_sources(dawn_perf_tests PRIVATE
	perf_tests/BufferUploadPerf.cpp
	perf_tests/DawnPerfTest.cpp
	perf_tests/DawnPerfTest.h
	perf_tests/DawnPerfTestPlatform.cpp
	perf_tests/DawnPerfTestPlatform.h
	perf_tests/DrawCallPerf.cpp
	perf_tests/ShaderRobustnessPerf.cpp
	perf_tests/SubresourceTrackingPerf.cpp
	perf_tests/VulkanZeroInitializeWorkgroupMemoryPerf.cpp

	PerfTestsMain.cpp
)
