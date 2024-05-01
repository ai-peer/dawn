import subprocess
import os

cwd = os.getcwd()
source_sets = """android.sourceSets {
    main.kotlin.srcDirs += \'""" + cwd + """/gen/java/'
}"""
f = open("../../tools/android/webgpu/sourceSets.gradle", "w")
f.write(source_sets)
f.close()

print(
    subprocess.run(["gradle", "wrapper"],
                   cwd="../../tools/android",
                   capture_output=True))
print(
    subprocess.run(["./gradlew", "build"],
                   cwd="../../tools/android",
                   capture_output=True))
