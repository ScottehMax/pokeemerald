"""The GameCube side of a JOY Bus link to mGBA.

mGBA's "File > Connect to Dolphin..." connects to a Dolphin emulator over two
TCP sockets, which this class serves instead:

- clock (port 49420): we send big-endian int32 GBA cycle counts; mGBA runs
  the GBA for that long (lockstep).
- data (port 54970): we send JOY Bus commands, mGBA answers each one:
      0x00 poll   -> 00 04 JOYSTAT        (unused: 0x14 also returns JOYSTAT)
      0xFF reset  -> 00 04 JOYSTAT
      0x14 read   -> JOY_TRANS (4 bytes, LE) + JOYSTAT
      0x15 write  + 4 bytes into JOY_RECV -> JOYSTAT
  Each command must follow a clock packet. mGBA ignores commands (and sends
  nothing back) while the GBA isn't in JOY Bus mode.

The viewer ROM's handshake is described in include/constants/link_viewer.h.
"""

from __future__ import annotations

import socket
import struct
import threading
import time

import lvproto as P

C = P.C
DATA_PORT = 54970
CLOCK_PORT = 49420
GBA_HZ = 1 << 24

CMD_POLL, CMD_READ, CMD_WRITE, CMD_RESET = 0x00, 0x14, 0x15, 0xFF
JOYSTAT_RECV = C["LV_JOYSTAT_RECV"]
JOYSTAT_READY = C["LV_JOYSTAT_READY"]
JOYSTAT_NEED_SYNC = C["LV_JOYSTAT_NEED_SYNC"]
MAX_IN_FLIGHT = 256  # stream bytes written but not yet taken by the viewer


class NoReply(Exception):
    pass


class JoyBusHost:
    def __init__(self, host: str = "127.0.0.1", data_port: int = DATA_PORT, clock_port: int = CLOCK_PORT, log=print,
                 clock_rate: float = 1.0):
        self.host, self.data_port, self.clock_port = host, data_port, clock_port
        self.clock_rate = clock_rate  # >1 lets an unthrottled emulator run faster than real time
        self.log = log
        self.lock = threading.Lock()
        self.pending = bytearray()      # whole messages waiting to be written
        self.paused = True              # no writes until the stream has been (re)started
        self.sync_needed = threading.Event()
        self.connected = False
        self.viewer_ready = False
        self.words_sent = 0
        self.stream_pos = 0             # bytes written since the last reset
        self._stop = False
        self.thread = threading.Thread(target=self._serve, name="joybus", daemon=True)

    # --- used by the bridge --------------------------------------------------

    def start(self):
        self.thread.start()

    def stop(self):
        self._stop = True

    def send(self, msg: bytes):
        with self.lock:
            self.pending += msg

    def backlog(self) -> int:
        with self.lock:
            return len(self.pending)

    def take_sync_request(self) -> bool:
        """True once after the viewer (re)started: the caller must queue a
        fresh session and call resume()."""
        if self.sync_needed.is_set():
            self.sync_needed.clear()
            with self.lock:
                self.pending.clear()
            return True
        return False

    def resume(self):
        with self.lock:
            self.paused = False

    # --- connection -------------------------------------------------------------

    def _listen(self, port: int) -> socket.socket:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((self.host, port))
        s.listen(1)
        return s

    def _serve(self):
        data_listener = self._listen(self.data_port)
        clock_listener = self._listen(self.clock_port)
        self.log(f"[joybus] waiting for mGBA (File > Connect to Dolphin...) on {self.host}:{self.data_port}/{self.clock_port}")
        while not self._stop:
            data, _ = data_listener.accept()
            clock_listener.settimeout(10)
            try:
                clock, _ = clock_listener.accept()
            except socket.timeout:
                data.close()
                continue
            finally:
                clock_listener.settimeout(None)
            for s in (data, clock):
                s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.connected = True
            self.log("[joybus] mGBA connected")
            try:
                self._run(data, clock)
            except (OSError, ConnectionError) as e:
                self.log(f"[joybus] mGBA disconnected ({e})")
            finally:
                self.connected = False
                self.viewer_ready = False
                with self.lock:
                    self.paused = True
                for s in (data, clock):
                    try:
                        s.close()
                    except OSError:
                        pass

    def _run(self, data: socket.socket, clock: socket.socket):
        self.data, self.clock = data, clock
        self.last_clock = time.perf_counter()
        self._clock(GBA_HZ // 30)  # a little lead so the GBA starts at once
        self.must_reset = True     # (re)connected: restart the stream
        while not self._stop:
            try:
                self._step()
            except NoReply:
                if self.viewer_ready:
                    self.log("[joybus] the GBA stopped answering (not in JOY Bus mode, or paused)")
                    self.must_reset = True  # a write may have been lost
                self.viewer_ready = False
                time.sleep(0.02)

    def _step(self):
        """One status read, then a reset or a write when the viewer is ready for it."""
        reply = self._command(CMD_READ, 5)
        trans, stat = struct.unpack("<I", reply[:4])[0], reply[4]

        ready = bool(stat & JOYSTAT_READY) and trans >> 16 == C["LV_MAGIC"]
        if ready != self.viewer_ready:
            self.viewer_ready = ready
            self.log("[joybus] viewer ready" if ready else "[joybus] the GBA isn't running the viewer")
        if not ready:
            time.sleep(0.01)
            return

        if self.must_reset or stat & JOYSTAT_NEED_SYNC:
            if stat & JOYSTAT_NEED_SYNC and not self.must_reset:
                self.log("[joybus] the viewer restarted: resending the battle")
            with self.lock:
                self.paused = True
                self.pending.clear()
            self._command(CMD_RESET, 3)
            self.stream_pos = 0
            self.must_reset = False
            self.sync_needed.set()
            return

        if stat & JOYSTAT_RECV:
            return  # the GBA hasn't taken the last word yet (or its buffer is full)
        if (self.stream_pos - (trans & 0xFFFF)) & 0xFFFF >= MAX_IN_FLIGHT:
            time.sleep(0.002)
            return  # plenty queued on the GBA already

        with self.lock:
            word = None
            if not self.paused and self.pending:
                # Messages are queued whole, so padding always lands between them.
                word = bytes(self.pending[:4]).ljust(4, b"\0")
                del self.pending[:4]
        if word is None:
            time.sleep(0.002)
            return
        try:
            self._command(CMD_WRITE, 1, word)
        except NoReply:
            self.must_reset = True  # the word may or may not have arrived
            raise
        self.words_sent += 1
        self.stream_pos += 4

    def _clock(self, cycles: int):
        self.clock.sendall(struct.pack(">i", max(0, min(cycles, GBA_HZ))))

    def _command(self, cmd: int, reply_len: int, payload: bytes = b"") -> bytes:
        # Stray bytes (a late answer to a command that timed out) would misalign replies.
        self.data.setblocking(False)
        try:
            while self.data.recv(64):
                pass
        except (BlockingIOError, InterruptedError):
            pass
        finally:
            self.data.setblocking(True)

        now = time.perf_counter()
        self._clock(int((now - self.last_clock) * GBA_HZ * self.clock_rate))
        self.last_clock = now
        self.data.sendall(bytes([cmd]) + payload)

        self.data.settimeout(0.25)
        reply = b""
        try:
            while len(reply) < reply_len:
                chunk = self.data.recv(reply_len - len(reply))
                if not chunk:
                    raise ConnectionError("connection closed")
                reply += chunk
        except socket.timeout:
            raise NoReply() from None
        finally:
            self.data.settimeout(None)
        if reply_len == 3 and reply[:2] != b"\x00\x04":
            raise NoReply()
        return reply
