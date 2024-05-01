import subprocess

print(
    subprocess.run(
        ["cp", "-r", "gen/java", "../../tools/android/webgpu/src/main/"],
        capture_output=True))
print(
    subprocess.run(
        ["gradle", "wrapper"],
        cwd="../../tools/android",
        capture_output=True))
print(
    subprocess.run(
        ["./gradlew", ":webgpu:testDebugUnitTest"],
        cwd="../../tools/android",
        capture_output=True))
