package android.dawn

object BufferMapAsyncStatus {
    const val Success = 0x00000000;
    const val InstanceDropped = 0x00000001;
    const val ValidationError = 0x00000002;
    const val Unknown = 0x00000003;
    const val DeviceLost = 0x00000004;
    const val DestroyedBeforeCallback = 0x00000005;
    const val UnmappedBeforeCallback = 0x00000006;
    const val MappingAlreadyPending = 0x00000007;
    const val OffsetOutOfRange = 0x00000008;
    const val SizeOutOfRange = 0x00000009;
}
