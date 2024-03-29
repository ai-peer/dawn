package android.dawn

class RenderPassTimestampWrites(
    var querySet: QuerySet? = null,
    var beginningOfPassWriteIndex: Int = Constants.QUERY_SET_INDEX_UNDEFINED,
    var endOfPassWriteIndex: Int = Constants.QUERY_SET_INDEX_UNDEFINED
) {
}
