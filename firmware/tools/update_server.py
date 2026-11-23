#!/usr/bin/env python3
"""Minimal firmware server for the press counters:  serves a folder (manifest.json + .bin files) over HTTP.

    python update_server.py ./fw --port 8080 --token SECRET

The board sends the token as "Authorization: Bearer <token>" (setting 'Access token' on its web page).  Without --token the folder is open
to everybody who can reach the port.  Plain HTTP only - run it inside your factory network, not on the internet.  The images are NOT signed.
"""
import argparse
import functools
import hmac
import http.server
import os


class Handler(http.server.SimpleHTTPRequestHandler):
    token = ""

    def do_GET(self):
        if self.token:
            got = self.headers.get("Authorization", "")
            if not hmac.compare_digest(got, "Bearer " + self.token):
                self.send_error(401, "token required")
                return
        super().do_GET()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--token", default="")
    a = ap.parse_args()
    Handler.token = a.token
    h = functools.partial(Handler, directory=os.path.abspath(a.folder))
    srv = http.server.ThreadingHTTPServer(("0.0.0.0", a.port), h)
    print("serving", os.path.abspath(a.folder), "on port", a.port, "(token required)" if a.token else "(NO token)")
    srv.serve_forever()


if __name__ == "__main__":
    main()
