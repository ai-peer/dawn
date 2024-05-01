import subprocess

print(
    subprocess.run(
        ["./gradlew", ":webgpu:testDebugUnitTest"],
        cwd="../../tools/android",
        capture_output=True))
