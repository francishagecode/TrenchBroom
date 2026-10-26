"""A tiny local review server: the gallery plus one endpoint to record decisions."""

from __future__ import annotations

import json
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

from . import project
from .gallery import write_gallery


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=project.OUT, **kwargs)

    def log_message(self, fmt, *args):  # keep the terminal quiet
        pass

    def do_GET(self):
        if self.path in ("/", "/index.html", "/gallery.html"):
            # Rebuild on every page load so edits to design files show up on refresh.
            path = os.path.join(project.OUT, "gallery.html")
            write_gallery(path, server=True)
            self.path = "/gallery.html"
        return super().do_GET()

    def do_POST(self):
        if self.path != "/api/review":
            self.send_error(404)
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = json.loads(self.rfile.read(length) or b"{}")
            entry = project.set_review(body["design"], body["variant"], body["status"],
                                       body.get("note", ""))
            self._json(200, {"entry": entry})
        except Exception as e:
            self._json(400, {"error": f"{type(e).__name__}: {e}"})

    def _json(self, code, data):
        payload = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)


def serve(port: int = 8765) -> None:
    os.makedirs(project.OUT, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", port), Handler)
    print(f"Brush Lab review gallery: http://127.0.0.1:{port}/  (Ctrl+C to stop)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
