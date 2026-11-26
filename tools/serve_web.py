"""Serves the browser build (build/web/site) to phones on this network: scripts/serve-web.ps1 runs it.

    python -I -B tools/serve_web.py [--port 8091] [--root build/web/site] [--move-to https://glowtide.netlify.app/]

Every answer says no-cache, so a phone that kept an older index.js never runs it against a newer index.wasm.
--move-to: the game lives at another address now. Every visit gets a page that sends the player there with the saves
this address kept (localStorage is per address), which web/shell.html takes in.
"""
import argparse
import functools
import html
import http.server
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

MOVED = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>GLOWTIDE has moved</title>
<style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;background:#0e0e2e;
color:#fff;font:18px/1.5 system-ui,sans-serif;text-align:center;padding:0 16px}a{color:#ffd27a}</style>
</head><body><p>GLOWTIDE has moved to <a id="to" href="TO">TO</a>,<br>taking your progress with it&hellip;</p>
<script>
  // This address's saves (the glowtide:* localStorage keys) go along in the new address's fragment, which never
  // reaches a server: #import=<the keys as JSON>.
  var moved = {};
  try {
    for (var i = 0; i < localStorage.length; ++i) {
      var key = localStorage.key(i);
      if (key.indexOf('glowtide:') === 0) moved[key] = localStorage.getItem(key);
    }
  } catch (e) {}
  var to = document.getElementById('to').href;
  location.replace(Object.keys(moved).length ? to + '#import=' + encodeURIComponent(JSON.stringify(moved)) : to);
</script></body></html>
"""


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".webmanifest": "application/manifest+json",
    }
    moved = None  # the page every visit gets with --move-to

    def do_GET(self):
        if not self.moved:
            return super().do_GET()
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(self.moved)))
        self.end_headers()
        self.wfile.write(self.moved)

    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8091)
    ap.add_argument("--root", default=os.path.join(ROOT, "build", "web", "site"))
    ap.add_argument("--move-to", help="the game's new address: every visit is sent there with its saves")
    a = ap.parse_args()
    if a.move_to:
        Handler.moved = MOVED.replace("TO", html.escape(a.move_to, quote=True)).encode()
        print(f"[serve] every visit on port {a.port} goes to {a.move_to} with its saves; Ctrl+C stops it", flush=True)
    elif not os.path.isfile(os.path.join(a.root, "index.html")):
        raise SystemExit(f"no index.html in {a.root}: run scripts\\build-web.ps1 first")
    else:
        print(f"[serve] {a.root} on port {a.port}; Ctrl+C stops it", flush=True)
    server = http.server.ThreadingHTTPServer(("0.0.0.0", a.port), functools.partial(Handler, directory=a.root))
    server.serve_forever()


if __name__ == "__main__":
    main()
