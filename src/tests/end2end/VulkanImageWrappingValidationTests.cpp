#include "dawn_native/VulkanBackend.h"
#include "tests/DawnTest.h"
#include "utils/DawnHelpers.h"

class VulkanImageWrappingValidationTests : public DawnTest {
  public:
    void SetUp() override {
        DawnTest::SetUp();

        defaultDescriptor.dimension = dawn::TextureDimension::e2D;
        defaultDescriptor.format = dawn::TextureFormat::RGBA8Unorm;
        defaultDescriptor.size = {10, 10, 1};
        defaultDescriptor.sampleCount = 1;
        defaultDescriptor.arrayLayerCount = 1;
        defaultDescriptor.mipLevelCount = 1;
        defaultDescriptor.usage = dawn::TextureUsageBit::OutputAttachment;
    }

    dawn::Texture WrapVulkanImage(const dawn::TextureDescriptor* descriptor) {
        std::vector<int> waitFds;
        DawnTexture texture = dawn_native::vulkan::WrapVulkanImage(
            device.Get(), reinterpret_cast<const DawnTextureDescriptor*>(descriptor), -1, 0, 0,
            waitFds);
        return dawn::Texture::Acquire(texture);
    }

  protected:
    dawn::TextureDescriptor defaultDescriptor;
};

// Test an error occurs if the texture descriptor is invalid
TEST_P(VulkanImageWrappingValidationTests, InvalidTextureDescriptor) {
    defaultDescriptor.nextInChain = this;

    ASSERT_DEVICE_ERROR(dawn::Texture texture = WrapVulkanImage(&defaultDescriptor));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor dimension isn't 2D
// TODO(cwallez@chromium.org): Reenable when 1D or 3D textures are implemented
TEST_P(VulkanImageWrappingValidationTests, DISABLED_InvalidTextureDimension) {
    defaultDescriptor.dimension = dawn::TextureDimension::e2D;

    ASSERT_DEVICE_ERROR(dawn::Texture texture = WrapVulkanImage(&defaultDescriptor));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor mip level count isn't 1
TEST_P(VulkanImageWrappingValidationTests, InvalidMipLevelCount) {
    defaultDescriptor.mipLevelCount = 2;

    ASSERT_DEVICE_ERROR(dawn::Texture texture = WrapVulkanImage(&defaultDescriptor));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor array layer count isn't 1
TEST_P(VulkanImageWrappingValidationTests, InvalidArrayLayerCount) {
    defaultDescriptor.arrayLayerCount = 2;

    ASSERT_DEVICE_ERROR(dawn::Texture texture = WrapVulkanImage(&defaultDescriptor));
    ASSERT_EQ(texture.Get(), nullptr);
}

// Test an error occurs if the descriptor sample count isn't 1
TEST_P(VulkanImageWrappingValidationTests, InvalidSampleCount) {
    defaultDescriptor.sampleCount = 4;

    ASSERT_DEVICE_ERROR(dawn::Texture texture = WrapVulkanImage(&defaultDescriptor));
    ASSERT_EQ(texture.Get(), nullptr);
}

DAWN_INSTANTIATE_TEST(VulkanImageWrappingValidationTests, VulkanBackend);
