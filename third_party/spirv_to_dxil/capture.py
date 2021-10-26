import os
import subprocess
import sys
import errno

# Wrapper for mesa build scripts that make use of meson's `capture` property

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: capture.py <output_path> <script_path> [args ..]")
        sys.exit(1)

    output_path = sys.argv[1]
    script_path = sys.argv[2]

    # `exist_ok` is only available in python 3.2
    try:
        os.makedirs(os.path.dirname(output_path))
    except OSError as e:
        if e.errno != errno.EEXIST:
            raise

    f = open(output_path, 'w')
    process = subprocess.Popen(["python3", script_path] + sys.argv[3:],
                               stdout=f)
    process.wait()
    f.close()
