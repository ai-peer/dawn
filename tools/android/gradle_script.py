import subprocess


print(subprocess.run(
    ["python3", "generator/dawn_json_generator.py", "--template-dir generator/templates", "--output-dir tools/android/webgpu/src/main", "--dawn-json src/dawn/dawn.json", "--targets kotlin", "--jinja2-path third_party/jinja2"],
    cwd="../../",
    capture_output=True))
print(subprocess.run(
    ["gradle", "wrapper"],
    cwd="../../tools/android",
    capture_output=True))
print(subprocess.run(
    ["./gradlew", ":webgpu:testDebugUnitTest"],
    cwd="../../tools/android",
    capture_output=True))
