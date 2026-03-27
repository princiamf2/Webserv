#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()

print("Content-Type: text/plain")
print()
print("CGI OK")
print("METHOD =", os.environ.get("REQUEST_METHOD", ""))
print("QUERY =", os.environ.get("QUERY_STRING", ""))
print("BODY =", body)