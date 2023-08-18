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

#ifndef SRC_DAWN_NATIVE_TEXTURE_H_
#define SRC_DAWN_NATIVE_TEXTURE_H_

#include <vector>

#include "dawn/common/WeakRef.h"
#include "dawn/common/ityp_array.h"
#include "dawn/common/ityp_bitset.h"
#include "dawn/native/ConstContent.h"
#include "dawn/native/Error.h"
#include "dawn/native/Format.h"
#include "dawn/native/Forward.h"
#include "dawn/native/ObjectBase.h"
#include "dawn/native/SharedTextureMemory.h"
#include "dawn/native/Subresource.h"

#include "dawn/native/dawn_platform.h"

namespace dawn::native {

enum class AllowMultiPlanarTextureFormat {
    No,
    Yes,
};

MaybeError ValidateTextureDescriptor(
    const DeviceBase* device,
    const TextureDescriptor* descriptor,
    AllowMultiPlanarTextureFormat allowMultiPlanar = AllowMultiPlanarTextureFormat::No,
    std::optional<wgpu::TextureUsage> allowedSharedTextureMemoryUsage = std::nullopt);
MaybeError ValidateTextureViewDescriptor(const DeviceBase* device,
                                         const TextureBase* texture,
                                         const TextureViewDescriptor* descriptor);
ResultOrError<TextureViewDescriptor> GetTextureViewDescriptorWithDefaults(
    const TextureBase* texture,
    const TextureViewDescriptor* descriptor);

bool IsValidSampleCount(uint32_t sampleCount);

static constexpr wgpu::TextureUsage kReadOnlyTextureUsages =
    wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding | kReadOnlyRenderAttachment |
    kReadOnlyStorageTexture;

// Valid texture usages for a resolve texture that are loaded from at the beginning of a render
// pass.
static constexpr wgpu::TextureUsage kResolveTextureLoadAndStoreUsages =
    kResolveAttachmentLoadingUsage | wgpu::TextureUsage::RenderAttachment;

namespace detail {

struct TextureBaseContents {
    wgpu::TextureDimension mDimension;
    const Format& mFormat;
    FormatSet mViewFormats;
    Extent3D mSize;
    uint32_t mMipLevelCount;
    uint32_t mSampleCount;
    wgpu::TextureUsage mUsage = wgpu::TextureUsage::None;
    wgpu::TextureUsage mInternalUsage = wgpu::TextureUsage::None;
    wgpu::TextureFormat mFormatEnumForReflection;

    TextureBaseContents(DeviceBase* device,
                        const TextureDescriptor* descriptor,
                        ObjectBase::ErrorTag tag);
    TextureBaseContents(DeviceBase* device, const TextureDescriptor* descriptor);

    void AddInternalUsage(wgpu::TextureUsage usage);

#define TEXTURE_BASE_CONTENT_FUNCS(X)                                                              \
    X(wgpu::TextureDimension, GetDimension, ())                                                    \
    X(const Format&, GetFormat, ())                                                                \
    X(const FormatSet&, GetViewFormats, ())                                                        \
    X(const Extent3D&, GetSize, ())                                                                \
    X(uint32_t, GetWidth, ())                                                                      \
    X(uint32_t, GetHeight, ())                                                                     \
    X(uint32_t, GetDepth, ())                                                                      \
    X(uint32_t, GetArrayLayers, ())                                                                \
    X(uint32_t, GetNumMipLevels, ())                                                               \
    X(SubresourceRange, GetAllSubresources, ())                                                    \
    X(uint32_t, GetSampleCount, ())                                                                \
    X(bool, IsMultisampledTexture, ())                                                             \
    /* Returns the usage the texture was created with via the API.                              */ \
    X(wgpu::TextureUsage, GetUsage, ())                                                            \
    /* Returns union of base and extension usages.                                              */ \
    X(wgpu::TextureUsage, GetInternalUsage, ())                                                    \
    /* For a texture with non-block-compressed texture format, its physical size is always      */ \
    /* equal to its virtual size. For a texture with block compressed texture format, the       */ \
    /* physical size is the one with paddings if necessary, which is always a multiple of the   */ \
    /* block size and used in texture copying. The virtual size is the one without paddings,    */ \
    /* which is not required to be a multiple of the block size and used in texture sampling.   */ \
    X(Extent3D, GetMipLevelSingleSubresourcePhysicalSize, (uint32_t level))                        \
    X(Extent3D, GetMipLevelSingleSubresourceVirtualSize, (uint32_t level))                         \
    /* For 2d-array textures, this keeps the array layers in contrast to                        */ \
    /* GetMipLevelSingleSubresourceVirtualSize.                                                 */ \
    X(Extent3D, GetMipLevelSubresourceVirtualSize, (uint32_t level))

    DAWN_CONTENT_FUNC_HEADERS(TEXTURE_BASE_CONTENT_FUNCS)
};

}  // namespace detail

class TextureBase : public ApiObjectBase {
  public:
    enum class ClearValue { Zero, NonZero };

    static TextureBase* MakeError(DeviceBase* device, const TextureDescriptor* descriptor);

    ObjectType GetType() const override;

    // Inline proxy functions that just call into the contents.
    DAWN_CONTENT_PROXY_FUNCS(TEXTURE_BASE_CONTENT_FUNCS)
#undef TEXTURE_BASE_CONTENT_FUNCS

    uint32_t GetSubresourceCount() const;

    bool IsDestroyed() const;
    void SetHasAccess(bool hasAccess);
    uint32_t GetSubresourceIndex(uint32_t mipLevel, uint32_t arraySlice, Aspect aspect) const;
    bool IsSubresourceContentInitialized(const SubresourceRange& range) const;
    void SetIsSubresourceContentInitialized(bool isInitialized, const SubresourceRange& range);

    MaybeError ValidateCanUseInSubmitNow() const;

    // Returns true if the size covers the whole subresource.
    bool CoverFullSubresource(uint32_t mipLevel, const Extent3D& size) const;

    Extent3D ClampToMipLevelVirtualSize(uint32_t level,
                                        const Origin3D& origin,
                                        const Extent3D& extent) const;

    ResultOrError<Ref<TextureViewBase>> CreateView(
        const TextureViewDescriptor* descriptor = nullptr);
    ApiObjectList* GetViewTrackingList();

    bool IsImplicitMSAARenderTextureViewSupported() const;

    Ref<SharedTextureMemoryBase> TryGetSharedTextureMemory();

    // Dawn API
    TextureViewBase* APICreateView(const TextureViewDescriptor* descriptor = nullptr);
    void APIDestroy();
    uint32_t APIGetWidth() const;
    uint32_t APIGetHeight() const;
    uint32_t APIGetDepthOrArrayLayers() const;
    uint32_t APIGetMipLevelCount() const;
    uint32_t APIGetSampleCount() const;
    wgpu::TextureDimension APIGetDimension() const;
    wgpu::TextureFormat APIGetFormat() const;
    wgpu::TextureUsage APIGetUsage() const;

  protected:
    TextureBase(DeviceBase* device, const TextureDescriptor* descriptor);
    TextureBase(DeviceBase* device,
                const TextureDescriptor* descriptor,
                const detail::TextureBaseContents contents);
    ~TextureBase() override;

    void DestroyImpl() override;

    // The shared texture memory the texture was created from. May be null.
    WeakRef<SharedTextureMemoryBase> mSharedTextureMemory;

  private:
    struct TextureState {
        TextureState();

        // Indicates whether the texture may access by the GPU in a queue submit.
        bool hasAccess : 1;
        // Indicates whether the texture has been destroyed.
        bool destroyed : 1;
    };

    TextureBase(DeviceBase* device, const TextureDescriptor* descriptor, ObjectBase::ErrorTag tag);

    const detail::TextureBaseContents mContents;

    TextureState mState;

    // Textures track texture views created from them so that they can be destroyed when the texture
    // is destroyed.
    ApiObjectList mTextureViews;

    // TODO(crbug.com/dawn/845): Use a more optimized data structure to save space
    std::vector<bool> mIsSubresourceContentInitializedAtIndex;
};

class TextureViewBase : public ApiObjectBase {
  public:
    TextureViewBase(TextureBase* texture, const TextureViewDescriptor* descriptor);
    ~TextureViewBase() override;

    static TextureViewBase* MakeError(DeviceBase* device, const char* label = nullptr);

    ObjectType GetType() const override;
    void FormatLabel(absl::FormatSink* s) const override;

    const TextureBase* GetTexture() const;
    TextureBase* GetTexture();

    Aspect GetAspects() const;
    const Format& GetFormat() const;
    wgpu::TextureViewDimension GetDimension() const;
    uint32_t GetBaseMipLevel() const;
    uint32_t GetLevelCount() const;
    uint32_t GetBaseArrayLayer() const;
    uint32_t GetLayerCount() const;
    const SubresourceRange& GetSubresourceRange() const;

  protected:
    void DestroyImpl() override;

  private:
    TextureViewBase(DeviceBase* device, ObjectBase::ErrorTag tag, const char* label);

    ApiObjectList* GetObjectTrackingList() override;

    Ref<TextureBase> mTexture;

    const Format& mFormat;
    wgpu::TextureViewDimension mDimension;
    SubresourceRange mRange;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TEXTURE_H_
