package android.dawn

object Functions {
    init {
        System.loadLibrary("webgpu_c_bundled");
    }

    external fun createInstance(descriptor: InstanceDescriptor?): Instance
}
