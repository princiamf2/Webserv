#!/usr/bin/env python3

import os

print("CGI OK")
print("METHOD =", os.environ.get("REQUEST_METHOD", ""))
print("QUERY =", os.environ.get("QUERY_STRING", ""))
