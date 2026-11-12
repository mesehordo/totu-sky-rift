"""Helyi fogadószerver a böngészőben generált képekhez (csak 127.0.0.1-en figyel).

    python tools/art_receiver.py        -> http://127.0.0.1:47888/upload?name=<azonosító>

A böngészőben futó oldal POST-tal küldi a PNG-t; a fájl az assets/art/raw/<azonosító>.png-be kerül.
"""
import os
import re
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAW = os.path.join(ROOT, "assets", "art", "raw")


class H(BaseHTTPRequestHandler):
    def _cors(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Access-Control-Allow-Private-Network", "true")

    def do_OPTIONS(self):
        self.send_response(204)
        self._cors()
        self.end_headers()

    def do_POST(self):
        q = parse_qs(urlparse(self.path).query)
        name = q.get("name", [""])[0]
        if not re.fullmatch(r"[a-z0-9_]{2,60}", name):
            self.send_response(400)
            self._cors()
            self.end_headers()
            return
        n = int(self.headers.get("Content-Length", 0))
        data = self.rfile.read(n)
        if not data.startswith(b"\x89PNG") and not data.startswith(b"\xff\xd8") and not data.startswith(b"RIFF"):
            self.send_response(415)
            self._cors()
            self.end_headers()
            return
        os.makedirs(RAW, exist_ok=True)
        with open(os.path.join(RAW, name + ".png"), "wb") as f:
            f.write(data)
        print("mentve:", name, len(data), "bájt", flush=True)
        self.send_response(200)
        self._cors()
        self.end_headers()
        self.wfile.write(b"ok")

    def log_message(self, *a):
        pass


if __name__ == "__main__":
    print("fogadó: http://127.0.0.1:47888", flush=True)
    ThreadingHTTPServer(("127.0.0.1", 47888), H).serve_forever()
