"""
TOS Upgrade Check API
Endpoint: GET /tos/upgrade/check
Params:  model, version, version_code
"""

from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import json

# Latest firmware info — update this when releasing new versions
LATEST_VERSION = "1"
LATEST_VERSION_CODE = 260601000
LATEST_BUILD = "1.26.6.r2"
LATEST_PATCH = "2026-06-01"
LATEST_URL = "https://dl.otodone.com/tos/firmware/tos_latest.bin"
LATEST_SIZE = 276964
LATEST_SHA256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path != "/tos/upgrade/check":
            self.send_error(404)
            return

        params = parse_qs(parsed.query)
        model = params.get("model", [""])[0]
        version = params.get("version", ["0"])[0]
        version_code = params.get("version_code", ["0"])[0]

        try:
            current_code = int(version_code)
        except ValueError:
            current_code = 0

        has_update = current_code < LATEST_VERSION_CODE

        resp = {
            "code": 0,
            "data": {
                "has_update": has_update,
                "current": {
                    "model": model,
                    "version": version,
                    "version_code": current_code,
                },
                "latest": {
                    "version": LATEST_VERSION,
                    "version_code": LATEST_VERSION_CODE,
                    "build": LATEST_BUILD,
                    "patch": LATEST_PATCH,
                    "url": LATEST_URL,
                    "size": LATEST_SIZE,
                    "sha256": LATEST_SHA256,
                },
            },
        }

        body = json.dumps(resp, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):
        print("[REQ] %s" % (fmt % args))

if __name__ == "__main__":
    import sys
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8084  # 修改这里：默认端口从 80 改为 8084
    print(f"TOS Upgrade API running on :{port}")
    HTTPServer(("0.0.0.0", port), Handler).serve_forever()