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

#ifndef DAWNNATIVE_BINDGROUP_H_
#define DAWNNATIVE_BINDGROUP_H_

#include "common/Constants.h"
#include "common/PlacementAllocated.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/Error.h"
#include "dawn_native/Forward.h"
#include "dawn_native/ObjectBase.h"

#include "dawn_native/dawn_platform.h"

#include <array>

namespace dawn_native {

    class DeviceBase;

    MaybeError ValidateBindGroupDescriptor(DeviceBase* device,
                                           const BindGroupDescriptor* descriptor);

    struct BufferBinding {
        BufferBase* buffer;
        uint64_t offset;
        uint64_t size;
    };

    class BindGroupBase : public ObjectBase {
      public:
        ~BindGroupBase() override;

        static BindGroupBase* MakeError(DeviceBase* device);

        BindGroupLayoutBase* GetLayout();
        BufferBinding GetBindingAsBufferBinding(size_t binding);
        SamplerBase* GetBindingAsSampler(size_t binding);
        TextureViewBase* GetBindingAsTextureView(size_t binding);

        // Compute the amount of space / alignment required to store bindings for a bind group of
        // this layout.
        static size_t GetBindingDataSize(const BindGroupLayoutBase* layout);
        static constexpr uint32_t GetBindingDataAlignment() {
            // template <uint32_t BindingCount, uint32_t BufferCount>
            // struct BindingData {
            //     Ref<ObjectBase> bindings[BindingCount];
            //     uint32_t offsets[BufferCount];
            //     uint32_t sizes[BufferCount];
            // };
            // This struct describes how binding data is packed. Require the alignment to match the
            // alignment of the first (and largest) member.
            return alignof(Ref<ObjectBase>);
        }

      protected:
        // To save memory, the size of a bind group is dynamically determined and the bind group is
        // placement-allocated into memory big enough to hold the bind group with its
        // dynamically-sized bindings after it. The pointer of the memory of the beginning of the
        // binding data should be passed as |bindingDataStart|.
        BindGroupBase(DeviceBase* device,
                      const BindGroupDescriptor* descriptor,
                      void* bindingDataStart);

      private:
        BindGroupBase(DeviceBase* device, ObjectBase::ErrorTag tag);

        Ref<BindGroupLayoutBase> mLayout;
        Ref<ObjectBase>* const mBindings;
        uint32_t* const mOffsets;
        uint32_t* const mSizes;
    };

    // Helper class so |BindGroupBaseOwnBindingData| can allocate memory for its binding data,
    // before calling the BindGroupBase base class constructor.
    class OwnBindingDataHolder {
      protected:
        explicit OwnBindingDataHolder(size_t size);
        ~OwnBindingDataHolder();

        void* mBindingData;
    };

    // We don't have the complexity of placement-allocation of bind group data in
    // the Null backend. This class, keeps the binding data in a separate allocation for simplicity.
    class BindGroupBaseOwnBindingData : private OwnBindingDataHolder, public BindGroupBase {
      public:
        BindGroupBaseOwnBindingData(DeviceBase* device, const BindGroupDescriptor* descriptor);
        ~BindGroupBaseOwnBindingData() override = default;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_BINDGROUP_H_
