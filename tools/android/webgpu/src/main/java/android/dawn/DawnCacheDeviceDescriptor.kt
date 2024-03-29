package android.dawn

class DawnCacheDeviceDescriptor(
    var isolationKey: String = "",
    var loadDataFunction: DawnLoadCacheDataFunction? = null,
    var storeDataFunction: DawnStoreCacheDataFunction? = null,
    var functionUserdata: ByteArray? = null
) : DeviceDescriptor() {
}
