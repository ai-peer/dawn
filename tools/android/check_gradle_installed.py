import shutil
import sys

if shutil.which("gradle"):
    print("true")
else:
    print("false")
