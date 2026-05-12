import os
import sys

print("Content-Type: text/plain")
print()

for k in sorted(os.environ):
    if k in ["SCRIPT_NAME", "SCRIPT_FILENAME", "PATH_INFO", "QUERY_STRING", "REQUEST_URI", "REQUEST_METHOD", "CONTENT_LENGTH", "CONTENT_TYPE"] or k.startswith("HTTP_"):
        print(f"{k}={os.environ[k]}")

print("BODY=" + sys.stdin.read())