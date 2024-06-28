package android.dawn

import android.dawn.FeatureName
import android.dawn.dawnLauncher
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class FeaturesTest {
    @Test
    fun featuresTest() {
        val requiredFeatures =
            arrayOf(FeatureName.BGRA8UnormStorage, FeatureName.TextureCompressionASTC)
        dawnLauncher(requiredFeatures) { device ->
            val deviceFeatures = device.enumerateFeatures()
            requiredFeatures.forEach {
                assert(deviceFeatures.contains(it)) { "Requested feature $it available on device" }
            }
        }
    }
}