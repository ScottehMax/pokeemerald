"""Where battle logs come from.

Each source runs in its own thread and puts events on a queue:

    ("battle", me)       a new battle starts; me = the player to show at the bottom (or "")
    ("lines", [str], backlog)
                         protocol lines; backlog=True for history that should be
                         taken in without being shown (catching up on a battle in progress)
    ("eof", None)        the source has nothing more
"""

from __future__ import annotations

import base64
import html
import json
import os
import queue
import re
import socket
import ssl
import struct
import threading
import time
from pathlib import Path
from urllib.parse import urlparse

import lvproto as P

MAIN_SERVER = "wss://sim3.psim.us/showdown/websocket"


def lines_of(text: str, kind: str) -> list[str]:
    """Protocol lines from a log in one of the supported formats."""
    if kind == "jsonl":  # magneton battle records: {"l": line} / {"r": request} / {"team": ...}
        out = []
        for raw in text.splitlines():
            raw = raw.strip()
            if not raw:
                continue
            try:
                j = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if isinstance(j, dict) and "l" in j:
                out.append(j["l"])
        return out
    if kind == "html":  # a saved Showdown replay page
        m = re.search(r'<script[^>]*class="battle-log-data"[^>]*>(.*?)</script>', text, re.S)
        if m:
            text = html.unescape(m[1])
    return [l for l in text.splitlines() if l.startswith("|")]


def is_battle_end(line: str) -> bool:
    return line.startswith("|win|") or line == "|tie" or line.startswith("|tie|")


def file_kind(path: Path) -> str:
    suffix = path.suffix.lower()
    return {".jsonl": "jsonl", ".html": "html", ".htm": "html"}.get(suffix, "log")


def me_from_filename(path: Path) -> str:
    """magneton names its records battle-<format>-<id>-<bot>.jsonl."""
    m = re.fullmatch(r"battle-.+-\d+-(\w+)", path.stem)
    return m[1] if m else ""


class Source(threading.Thread):
    def __init__(self, events: queue.Queue, log=print):
        super().__init__(daemon=True)
        self.events = events
        self.log = log

    def put(self, kind, value=None, backlog=False):
        if kind == "lines":
            self.events.put((kind, value, backlog))
        else:
            self.events.put((kind, value))


class FileSource(Source):
    """One log file: replayed from the start, or (catch_up) taken in silently
    and then followed as it grows."""

    def __init__(self, events, path: Path, follow: bool, catch_up: bool, me: str = "", log=print):
        super().__init__(events, log)
        self.path, self.follow, self.catch_up = Path(path), follow, catch_up
        self.me = me or me_from_filename(self.path)
        self.finished = False   # saw |win| or |tie|
        self.last_growth = time.time()

    def run(self):
        self.put("battle", self.me)
        self.stream(first_backlog=self.catch_up)
        self.put("eof")

    def stream(self, first_backlog: bool, stop: threading.Event | None = None):
        kind = file_kind(self.path)
        pos = 0
        partial = ""
        first = True
        while True:
            try:
                with open(self.path, "r", encoding="utf-8", errors="replace") as f:
                    f.seek(pos)
                    chunk = f.read()
                    pos = f.tell()
            except FileNotFoundError:
                chunk = ""
            if chunk:
                self.last_growth = time.time()
                text = partial + chunk
                if kind != "html":
                    text, sep, partial = text.rpartition("\n")
                    text += sep
                lines = lines_of(text, kind)
                if any(is_battle_end(l) for l in lines):
                    self.finished = True
                if lines:
                    self.put("lines", lines, backlog=first and first_backlog)
            first = False
            if not self.follow or (stop is not None and stop.is_set()):
                return
            time.sleep(0.2)


class DirectorySource(Source):
    """Follows the newest log in a directory, moving on to each new one: point
    it at magneton's runs/showdown to watch the bot's battles as they happen."""

    def __init__(self, events, directory: Path, pattern: str, replay_last: bool, log=print):
        super().__init__(events, log)
        self.dir, self.pattern, self.replay_last = Path(directory), pattern, replay_last

    def newest(self) -> Path | None:
        files = [p for p in self.dir.glob(self.pattern) if p.is_file()]
        return max(files, key=lambda p: p.stat().st_mtime) if files else None

    def run(self):
        current = self.newest()
        catch_up = True
        if current is not None and not self.replay_last:
            if any(is_battle_end(l) for l in lines_of(current.read_text(encoding="utf-8", errors="replace"), file_kind(current))):
                self.log(f"[source] {current.name} is over; waiting for a new battle in {self.dir}")
                seen = current
                while (current := self.newest()) in (None, seen):
                    time.sleep(0.5)
                catch_up = False
        elif self.replay_last:
            catch_up = False
        while True:
            if current is None:
                time.sleep(0.5)
                current = self.newest()
                catch_up = False
                continue
            self.log(f"[source] following {current.name}")
            fs = FileSource(self.events, current, follow=True, catch_up=catch_up, log=self.log)
            stop = threading.Event()
            self.put("battle", fs.me)
            t = threading.Thread(target=fs.stream, args=(catch_up, stop), daemon=True)
            t.start()
            # Move on once there's a newer file and this battle is over (or has stalled).
            while True:
                time.sleep(0.5)
                newer = self.newest()
                if newer is not None and newer != current and newer.stat().st_mtime > current.stat().st_mtime:
                    if fs.finished or time.time() - fs.last_growth > 120:
                        break
            stop.set()
            t.join()
            current, catch_up = newer, False


# --- Showdown --------------------------------------------------------------------

def tls_context() -> ssl.SSLContext:
    """Mozilla's CA bundle when certifi is installed: on Windows, Python's
    OpenSSL can pick an expired certificate from the system store on the way
    to the root and refuse a valid chain (it does for sim3.psim.us)."""
    try:
        import certifi
    except ImportError:
        return ssl.create_default_context()
    return ssl.create_default_context(cafile=certifi.where())


class WebSocket:
    """A minimal RFC 6455 client (text frames only), enough for Showdown."""

    def __init__(self, url: str, timeout: float = 15):
        u = urlparse(url)
        port = u.port or (443 if u.scheme == "wss" else 80)
        sock = socket.create_connection((u.hostname, port), timeout=timeout)
        if u.scheme == "wss":
            sock = tls_context().wrap_socket(sock, server_hostname=u.hostname)
        self.sock = sock
        key = base64.b64encode(os.urandom(16)).decode()
        path = (u.path or "/") + (f"?{u.query}" if u.query else "")
        host = u.hostname if u.port is None else f"{u.hostname}:{u.port}"
        req = (f"GET {path} HTTP/1.1\r\nHost: {host}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\nUser-Agent: pokeemerald-link-viewer\r\n"
               f"Origin: https://play.pokemonshowdown.com\r\n\r\n")
        sock.sendall(req.encode())
        head = b""
        while b"\r\n\r\n" not in head:
            chunk = sock.recv(4096)
            if not chunk:
                raise ConnectionError("connection closed during the handshake")
            head += chunk
        head, _, self.buf = head.partition(b"\r\n\r\n")
        status = head.split(b"\r\n")[0].decode(errors="replace")
        if " 101 " not in status + " ":
            raise ConnectionError(f"websocket handshake refused: {status}")
        sock.settimeout(None)

    def _read(self, n: int) -> bytes:
        while len(self.buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("connection closed")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def _send_frame(self, opcode: int, payload: bytes):
        head = bytes([0x80 | opcode])
        n = len(payload)
        if n < 126:
            head += bytes([0x80 | n])
        elif n < 65536:
            head += bytes([0x80 | 126]) + struct.pack(">H", n)
        else:
            head += bytes([0x80 | 127]) + struct.pack(">Q", n)
        mask = os.urandom(4)
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def send(self, text: str):
        self._send_frame(0x1, text.encode())

    def recv(self) -> str:
        message = b""
        while True:
            b0, b1 = self._read(2)
            opcode, n = b0 & 0x0F, b1 & 0x7F
            if n == 126:
                n = struct.unpack(">H", self._read(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", self._read(8))[0]
            mask = self._read(4) if b1 & 0x80 else None
            payload = self._read(n)
            if mask:
                payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
            if opcode == 0x9:      # ping
                self._send_frame(0xA, payload)
                continue
            if opcode == 0x8:      # close
                raise ConnectionError("server closed the connection")
            if opcode in (0x0, 0x1, 0x2):
                message += payload
                if b0 & 0x80:
                    return message.decode("utf-8", errors="replace")

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


class ShowdownSource(Source):
    """Spectates battles on a Showdown server as a guest: one room, or every
    battle a user plays (polled with /cmd userdetails)."""

    def __init__(self, events, url: str, room: str = "", user: str = "", catch_up: bool = True, log=print):
        super().__init__(events, log)
        self.url, self.room, self.user = url or MAIN_SERVER, room, user
        self.catch_up = catch_up
        self.current = ""       # the battle room we're in
        self.done: set[str] = set()
        self.delivered = 0      # battle lines of the current room passed on so far
        self.joining = ""       # a room we asked to join
        self.join_time = 0.0

    def run(self):
        while True:
            try:
                self.session()
            except (OSError, ConnectionError) as e:
                self.log(f"[showdown] {e}; reconnecting in 5 s")
                self.current = ""
            time.sleep(5)

    def session(self):
        ws = WebSocket(self.url)
        self.log(f"[showdown] connected to {self.url}")
        last_query = 0.0
        ws.sock.settimeout(1.0)
        self.logged_in = False  # joining before the server names us a guest gets us dropped from the room
        while True:
            if self.joining and time.time() - self.join_time > 15:
                self.joining = ""  # no answer: look again
            if self.user and self.logged_in and not self.current and not self.joining and time.time() - last_query > 5:
                ws.send(f"|/cmd userdetails {self.user}")
                last_query = time.time()
            try:
                msg = ws.recv()
            except socket.timeout:
                continue
            self.on_message(ws, msg)

    def on_message(self, ws: WebSocket, msg: str):
        room = ""
        if msg.startswith(">"):
            first, _, msg = msg.partition("\n")
            room = first[1:].strip()
        lines = msg.split("\n")
        if not room:
            for line in lines:
                if line.startswith("|challstr|") and not self.logged_in:
                    self.logged_in = True
                    if self.room:
                        ws.send(f"|/join {self.room}")
                elif line.startswith("|queryresponse|userdetails|"):
                    self.on_userdetails(ws, line.split("|", 3)[3])
                elif line.startswith("|popup|"):
                    self.log(f"[showdown] {line[7:]}")
            return
        if not room.startswith("battle-"):
            return
        if any(l.startswith("|noinit|") or l.startswith("|deinit") for l in lines):
            if room == self.current:
                self.current = ""
            if room == self.joining:
                self.joining = ""
            if self.room == room:
                self.log(f"[showdown] left {room}: {' '.join(l for l in lines if l.startswith(('|noinit', '|deinit')))}")
            return
        battle_lines = [l for l in lines if l.startswith("|")]
        if any(l.startswith("|init|battle") for l in battle_lines):
            if room == self.current:
                # The whole log again (the server can resend it): pass on only what's new.
                new = battle_lines[self.delivered:]
                self.delivered = len(battle_lines)
                if new:
                    self.put("lines", new)
            else:
                self.current = room
                self.joining = ""
                self.delivered = len(battle_lines)
                self.log(f"[showdown] watching {room}")
                self.put("battle", self.user)
                self.put("lines", battle_lines, backlog=self.catch_up and any(l.startswith("|turn|") for l in battle_lines))
        elif room == self.current:
            self.delivered += len(battle_lines)
            self.put("lines", battle_lines)
        if room == self.current and any(is_battle_end(l) for l in battle_lines):
            self.done.add(room)
            self.current = ""
            ws.send(f"|/leave {room}")

    def on_userdetails(self, ws: WebSocket, js: str):
        try:
            details = json.loads(js)
        except json.JSONDecodeError:
            return
        if not details or not isinstance(details.get("rooms"), dict):
            return
        battles = []
        for name in details["rooms"]:
            m = re.search(r"battle-[a-z0-9]+-(\d+)(?:-[a-z0-9]+pw)?", name)
            if m and m[0] not in self.done:
                battles.append((int(m[1]), m[0]))
        if battles and not self.current and not self.joining:
            room = max(battles)[1]
            self.joining, self.join_time = room, time.time()
            ws.send(f"|/join {room}")
