package android.dawn

class Instance(val handle: Long) {

    external fun createSurface(descriptor: SurfaceDescriptor?): Surface
    external fun hasWGSLLanguageFeature(feature: Int): Boolean
    external fun processEvents(): Unit
    external fun requestAdapter(
        options: RequestAdapterOptions?,
        callback: RequestAdapterCallback
    ): Unit
}
