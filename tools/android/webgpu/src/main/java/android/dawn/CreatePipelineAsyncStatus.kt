package android.dawn

object CreatePipelineAsyncStatus {
    const val Success = 0x00000000;
    const val InstanceDropped = 0x00000001;
    const val ValidationError = 0x00000002;
    const val InternalError = 0x00000003;
    const val DeviceLost = 0x00000004;
    const val DeviceDestroyed = 0x00000005;
    const val Unknown = 0x00000006;
}
