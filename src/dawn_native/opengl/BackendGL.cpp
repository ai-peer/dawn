// Copyright 2019 The Dawn Authors
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

#include "dawn_native/opengl/BackendGL.h"

#include "common/Constants.h"
#include "dawn_native/Instance.h"
#include "dawn_native/OpenGLBackend.h"
#include "dawn_native/opengl/DeviceGL.h"

#include <cstring>
#include <iostream>

namespace dawn_native { namespace opengl {

    namespace {

        struct Vendor {
            const char* vendorName;
            uint32_t vendorId;
        };

        const Vendor kVendors[] = {{"ATI", kVendorID_AMD},
                                   {"ARM", kVendorID_ARM},
                                   {"Imagination", kVendorID_ImgTec},
                                   {"Intel", kVendorID_Intel},
                                   {"NVIDIA", kVendorID_Nvidia},
                                   {"Qualcomm", kVendorID_Qualcomm}};

        uint32_t GetVendorIdFromVendors(const char* vendor) {
            uint32_t vendorId = 0;
            for (const auto& it : kVendors) {
                // Matching vendor name with vendor string
                if (strstr(vendor, it.vendorName) != nullptr) {
                    vendorId = it.vendorId;
                    break;
                }
            }
            return vendorId;
        }

        void KHRONOS_APIENTRY OnGLDebugMessage(GLenum source,
                                               GLenum type,
                                               GLuint id,
                                               GLenum severity,
                                               GLsizei length,
                                               const GLchar* message,
                                               const void* userParam) {
            const char* sourceText;
            switch (source) {
                case GL_DEBUG_SOURCE_API:
                    sourceText = "OpenGL";
                    break;
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                    sourceText = "Window System";
                    break;
                case GL_DEBUG_SOURCE_SHADER_COMPILER:
                    sourceText = "Shader Compiler";
                    break;
                case GL_DEBUG_SOURCE_THIRD_PARTY:
                    sourceText = "Third Party";
                    break;
                case GL_DEBUG_SOURCE_APPLICATION:
                    sourceText = "Application";
                    break;
                case GL_DEBUG_SOURCE_OTHER:
                    sourceText = "Other";
                    break;
                default:
                    sourceText = "UNKNOWN";
                    break;
            }

            const char* severityText;
            switch (severity) {
                case GL_DEBUG_SEVERITY_HIGH:
                    severityText = "High";
                    break;
                case GL_DEBUG_SEVERITY_MEDIUM:
                    severityText = "Medium";
                    break;
                case GL_DEBUG_SEVERITY_LOW:
                    severityText = "Low";
                    break;
                case GL_DEBUG_SEVERITY_NOTIFICATION:
                    severityText = "Notification";
                    break;
                default:
                    severityText = "UNKNOWN";
                    break;
            }

            if (type == GL_DEBUG_TYPE_ERROR) {
                std::cout << "OpenGL error:" << std::endl;
                std::cout << "    Source: " << sourceText << std::endl;
                std::cout << "    ID: " << id << std::endl;
                std::cout << "    Severity: " << severityText << std::endl;
                std::cout << "    Message: " << message << std::endl;

                // Abort on an error when in Debug mode.
                UNREACHABLE();
            }
        }

    }  // anonymous namespace

    // The OpenGL backend's Adapter.

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance) : AdapterBase(instance, BackendType::OpenGL) {
        }

        MaybeError Initialize(const AdapterDiscoveryOptions* options) {
            // Use getProc to populate the dispatch table
            DAWN_TRY(mFunctions.Initialize(options->getProc));

            // Use the debug output functionality to get notified about GL errors
            // TODO(cwallez@chromium.org): add support for the KHR_debug and ARB_debug_output
            // extensions
            bool hasDebugOutput = mFunctions.IsAtLeastGL(4, 3) || mFunctions.IsAtLeastGLES(3, 2);

            if (GetInstance()->IsBackendValidationEnabled() && hasDebugOutput) {
                mFunctions.Enable(GL_DEBUG_OUTPUT);
                mFunctions.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

                // Any GL error; dangerous undefined behavior; any shader compiler and linker errors
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH,
                                               0, nullptr, GL_TRUE);

                // Severe performance warnings; GLSL or other shader compiler and linker warnings;
                // use of currently deprecated behavior
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM,
                                               0, nullptr, GL_TRUE);

                // Performance warnings from redundant state changes; trivial undefined behavior
                // This is disabled because we do an incredible amount of redundant state changes.
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0,
                                               nullptr, GL_FALSE);

                // Any message which is not an error or performance concern
                mFunctions.DebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                               GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                               GL_FALSE);
                mFunctions.DebugMessageCallback(&OnGLDebugMessage, nullptr);
            }

            // Set state that never changes between devices.
            mFunctions.Enable(GL_DEPTH_TEST);
            mFunctions.Enable(GL_SCISSOR_TEST);
            mFunctions.Enable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
            mFunctions.Enable(GL_MULTISAMPLE);
            mFunctions.Enable(GL_FRAMEBUFFER_SRGB);

            mPCIInfo.name = reinterpret_cast<const char*>(mFunctions.GetString(GL_RENDERER));

            // Workaroud to find vendor id from vendor name
            const char* vendor = reinterpret_cast<const char*>(mFunctions.GetString(GL_VENDOR));
            mPCIInfo.vendorId = GetVendorIdFromVendors(vendor);

            InitializeSupportedExtensions();

            return {};
        }

        ~Adapter() override = default;

      private:
        OpenGLFunctions mFunctions;

        ResultOrError<DeviceBase*> CreateDeviceImpl(const DeviceDescriptor* descriptor) override {
            // There is no limit on the number of devices created from this adapter because they can
            // all share the same backing OpenGL context.
            return {new Device(this, descriptor, mFunctions)};
        }

        void InitializeSupportedExtensions() {
            int32_t numExtensions;
            mFunctions.GetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
            // BC1, BC2 and BC3 formats are not supported in OpenGL core features.
            bool supportS3TC = false;
            bool supportTextureSRGB = false;
            bool supportS3TCSRGB = false;
            bool supportRGTC = mFunctions.IsAtLeastGL(3, 0);
            bool supportBPTC = mFunctions.IsAtLeastGL(4, 2);

            for (int32_t i = 0; i < numExtensions; ++i) {
                const char* extension =
                    reinterpret_cast<const char*>(mFunctions.GetStringi(GL_EXTENSIONS, i));
                if ((strcmp(extension, "GL_EXT_texture_compression_s3tc") == 0)) {
                    supportS3TC = true;
                } else if ((strcmp(extension, "GL_EXT_texture_sRGB") == 0)) {
                    // COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT and
                    // COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT requires both GL_EXT_texture_sRGB and
                    // GL_EXT_texture_compression_s3tc on desktop OpenGL drivers.
                    // (https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_sRGB.txt)
                    supportTextureSRGB = true;
                } else if ((strcmp(extension, "GL_EXT_texture_compression_s3tc_srgb") == 0)) {
                    // GL_EXT_texture_compression_s3tc_srgb is an extension in OpenGL ES.
                    supportS3TCSRGB = true;
                } else if ((strcmp(extension, "GL_ARB_texture_compression_rgtc") == 0) ||
                           (strcmp(extension, "GL_EXT_texture_compression_rgtc") == 0)) {
                    supportRGTC = true;
                } else if ((strcmp(extension, "GL_ARB_texture_compression_bptc") == 0) ||
                           (strcmp(extension, "GL_EXT_texture_compression_bptc") == 0)) {
                    supportBPTC = true;
                }
            }

            if (supportS3TC && (supportTextureSRGB || supportS3TCSRGB) && supportRGTC &&
                supportBPTC) {
                mSupportedExtensions.EnableExtension(dawn_native::Extension::TextureCompressionBC);
            }
        }
    };

    // Implementation of the OpenGL backend's BackendConnection

    Backend::Backend(InstanceBase* instance) : BackendConnection(instance, BackendType::OpenGL) {
    }

    std::vector<std::unique_ptr<AdapterBase>> Backend::DiscoverDefaultAdapters() {
        // The OpenGL backend needs at least "getProcAddress" to discover an adapter.
        return {};
    }

    ResultOrError<std::vector<std::unique_ptr<AdapterBase>>> Backend::DiscoverAdapters(
        const AdapterDiscoveryOptionsBase* optionsBase) {
        // TODO(cwallez@chromium.org): For now only create a single OpenGL adapter because don't
        // know how to handle MakeCurrent.
        if (mCreatedAdapter) {
            return DAWN_VALIDATION_ERROR("The OpenGL backend can only create a single adapter");
        }

        ASSERT(optionsBase->backendType == BackendType::OpenGL);
        const AdapterDiscoveryOptions* options =
            static_cast<const AdapterDiscoveryOptions*>(optionsBase);

        if (options->getProc == nullptr) {
            return DAWN_VALIDATION_ERROR("AdapterDiscoveryOptions::getProc must be set");
        }

        std::unique_ptr<Adapter> adapter = std::make_unique<Adapter>(GetInstance());
        DAWN_TRY(adapter->Initialize(options));

        mCreatedAdapter = true;
        std::vector<std::unique_ptr<AdapterBase>> adapters;
        adapters.push_back(std::unique_ptr<AdapterBase>(adapter.release()));
        return std::move(adapters);
    }

    BackendConnection* Connect(InstanceBase* instance) {
        return new Backend(instance);
    }

}}  // namespace dawn_native::opengl
