package android.dawn

import org.junit.Test

import org.junit.Assert.assertEquals

class ConstantsTest {
    @Test
    fun checkConstants() {
        assertEquals(Constants.UINT32_MAX, Integer.parseUnsignedInt("4294967295"));
        assertEquals(Constants.UINT64_MAX, java.lang.Long.parseUnsignedLong("18446744073709551615"));
        assertEquals(Constants.SIZE_MAX, Constants.UINT64_MAX);
        assertEquals(Constants.ARRAY_LAYER_COUNT_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.COPY_STRIDE_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.DEPTH_SLICE_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.LIMIT_U32_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.LIMIT_U64_UNDEFINED, Constants.UINT64_MAX);
        assertEquals(Constants.MIP_LEVEL_COUNT_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.QUERY_SET_INDEX_UNDEFINED, Constants.UINT32_MAX);
        assertEquals(Constants.WHOLE_MAP_SIZE, Constants.SIZE_MAX);
        assertEquals(Constants.WHOLE_SIZE, Constants.UINT64_MAX);
    }
}
