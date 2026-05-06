#!/usr/bin/env python3
import os, sys
print("Content-Type: text/plain")
print()
print("CGI OK SERVER2")
print("METHOD =", os.environ.get("REQUEST_METHOD"))
print("QUERY =", os.environ.get("QUERY_STRING"))
print("BODY =", sys.stdin.read())
