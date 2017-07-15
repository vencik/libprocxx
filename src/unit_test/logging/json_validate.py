#!/usr/bin/env python

import sys
import json


print("Checking JSON format")

for line in sys.stdin:
    json_obj = line.rstrip()

    try:
        print(json_obj)
        json.loads(json_obj)

    except ValueError as x:
        print("Malformed JSON: '%s'" % (json_obj,))
        sys.exit(1)

print("JSON format checked")
