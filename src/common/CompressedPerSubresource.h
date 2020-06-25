// An iterator like the std::vector iterator, but that increments only every <divisor>
template<typename T>
class DivisorIterator {
  public:
    // operator ++ == != *
    // contructor
  private:
    T* data;
    uint32_t divisor;
    uint32_t currentCount = 0;
}

template<typname T>
class DivisorCosntBeginEnd {
    //constructor from begin, end and divisor

    DivisorIterator<const T> begin() const;
    DivisorIterator<const T> end() const;
};

template<typename T>
class SubresourceStorage {
  public:
    enum Compression {
        NoCompression,
        PerArrayLayer,
        PerPlane,
        PerResource,
    };

    PerSubresource(uint8_t planeCount, uint16_t arrayLayerCount, uint8_t mipLevelCount, T defaultValue = T());

    Compression GetCompressionLevel() const;
    // Other getters

    ityp::span<size_t, T> Iterate(Compression c, dimensionStart, dimensionEnd); // Asserts c is less compressed than mCompression, decompresses if it needs to.
    DivisorConstBeginEnd<T> IterateConst(Compression c); // Asserts c is less compressed than mCompression, decompresses if it needs to.

  private:
    uint16_t arrayLayerCount;
    uint8_t planeCount;
    uint8_t mipLevelCount;
    Compression mCompression;
    std::vector<T> mData;
};

// TextureViewAs
// TODO create if not present.
PassTextureUsage& textureUsage = mTextureUsages[texture];
SubresourceStorage<wgpu::TextureUsage>* subresourceUsage = &textureUsage.subresourceUsages;

if (levelCount != subresourceUsage.GetLevelCount()) {
    for (each level {
        for each layer
            for each plane 
                subresource.at(level, layer, plane) |= ...
    }
}

if (layerCount != ...)
    for each layer
        for each plane 
            subresource.at(level, layer, plane) |= ...
