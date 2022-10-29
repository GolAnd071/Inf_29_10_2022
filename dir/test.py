#!/usr/bin/python3
import os

print("Content-Type: text/html")
print("Status: 200 OK")
print()

print("<b>Python CGI<b>")
print("<br><br>");
print("<b><em>Method: " + os.environ["REQUEST_METHOD"] + "</em></b>")