#!/usr/bin/env python3
"""Fake press counter for developing the web page without hardware:   python mock_gateway.py   ->  http://localhost:8099/

Serves firmware/press_counter/data/index.html and answers the /api/* calls with plausible data (nothing is stored, commands are only logged)."""
import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PAGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "press_counter", "data", "index.html")
T0 = time.time()
S = {"relay": 1, "count": 1234}


def state():
    S["count"] += 1 if int(time.time()) % 4 == 0 else 0
    up = int(time.time() - T0)
    return {
        "device": "press-7", "version": "6.2", "uptime": up + 4000, "heap": 182480, "time": int(time.time()), "tag": "personnel:1017",
        "net": {"wifi": True, "ap": False, "apSsid": "PressCounter-A1B2C3", "ssid": "factory", "ip": "192.168.1.57", "rssi": -61, "mac": "AA:BB:CC:11:22:33",
                "ntp": True, "mqtt": True, "mqttConfigured": True},
        "m": {"relay": S["relay"], "count": S["count"], "lifetime": 1203456, "lastDurMs": 412, "inPress": False, "lastAgoS": 12, "tempC": 41.5, "err": 6,
              "errText": "NO SERVER", "errMask": 1 << 6, "operator": "1017", "die": "D-4471", "sessionSec": 5400, "calls": 2, "queued": 0, "dropped": 0,
              "published": 9100, "ledAlert": up % 2 == 0, "btnSup": False, "btnNet": False, "nfc": True, "writePending": False, "banner": "", "outs": S.get("outs", 0b0101), "v12": 24.12, "aux": 0.0},
        "update": {"phase": 0, "pct": 0, "msg": "new firmware available", "latest": "6.2", "isNew": True, "configured": True, "busy": False},
    }


SETTINGS = {"net": {"mqttHost": "192.168.1.10", "mqttPort": 1883, "mqttTopic": "press", "mqttClient": "", "mqttUser": "", "mqttPassSet": False,
                    "updateUrl": "http://192.168.1.10:8080/fw", "updateTokenSet": True, "autoUpdate": 0, "checkMinutes": 360, "ntpHost": "pool.ntp.org",
                    "adminPassSet": False, "otaPassSet": True, "wifiSsid": "factory"},
            "machine": {"name": "press-7", "minpulse": "20", "idle": "30", "stuck": "60", "tempoff": "0", "overtemp": "800", "remote": "0", "sound": "2",
                        "bright": "80", "display": "1", "out1": "0", "out2": "1", "out3": "2", "out4": "0", "v12cal": "1000", "auxcal": "1000"}}


class H(BaseHTTPRequestHandler):
    def _send(self, code, body, ctype="application/json"):
        b = body if isinstance(body, bytes) else (body if isinstance(body, str) else json.dumps(body)).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path == "/":
            self._send(200, open(PAGE, "rb").read(), "text/html")
        elif self.path == "/api/state":
            self._send(200, state())
        elif self.path == "/api/settings":
            self._send(200, SETTINGS)
        elif self.path == "/api/log":
            self._send(200, "12 WiFi connected, ip 192.168.1.57\n13 time synchronised\n15 counter link up\n", "text/plain")
        elif self.path == "/api/scan":
            self._send(200, [{"ssid": "factory", "rssi": -60, "open": False}, {"ssid": "guest", "rssi": -75, "open": True}])
        else:
            self._send(404, "not found", "text/plain")

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(n).decode()
        print("POST", self.path, body[:200])
        if self.path == "/api/cmd" and "cmd=relay" in body:
            S["relay"] = 1 if "arg=1" in body else 0
        self._send(200, "ok", "text/plain")


if __name__ == "__main__":
    print("mock press counter on http://localhost:8099/")
    ThreadingHTTPServer(("127.0.0.1", 8099), H).serve_forever()
