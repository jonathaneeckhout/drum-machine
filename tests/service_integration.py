"""Exercise the real service/client lifecycle with SDL's dummy audio device."""
import os
from pathlib import Path
import socket
import stat
import subprocess
import sys
import tempfile
import time


binary = str(Path(sys.argv[1]).resolve())
env = {**os.environ, "SDL_AUDIODRIVER": "dummy"}
env.pop("DISPLAY", None)
env.pop("WAYLAND_DISPLAY", None)

with tempfile.TemporaryDirectory(prefix="drum-service-") as directory:
    endpoint = Path(directory) / "control.sock"
    with open(Path(directory) / "service.log", "w+") as log:
        service = subprocess.Popen(
            [binary, "--serve", "--socket", str(endpoint)],
            stdin=subprocess.DEVNULL, stdout=log, stderr=log, env=env,
        )
        try:
            deadline = time.monotonic() + 10
            while not endpoint.exists():
                if service.poll() is not None or time.monotonic() > deadline:
                    log.seek(0)
                    raise AssertionError(f"Service failed to start: {log.read()}")
                time.sleep(0.02)

            def command(text, expected=0):
                result = subprocess.run(
                    [binary, "--socket", str(endpoint), "--command", text],
                    capture_output=True, text=True, env=env, timeout=5,
                )
                assert result.returncode == expected, result.stderr + result.stdout
                return result.stdout

            assert stat.S_IMODE(endpoint.stat().st_mode) == 0o600
            command("clear")
            command("tempo 137")
            command("step 2 9")
            command("volume 2 0.4")
            command("play")
            assert "playing / 137 BPM" in command("status")
            client = subprocess.run(
                [binary, "--connect", "--socket", str(endpoint)],
                input="status\nquit\n", capture_output=True, text=True, env=env, timeout=5,
            )
            assert client.returncode == 0 and "playing / 137 BPM" in client.stdout
            assert "playing / 137 BPM" in command("status"), "quit must preserve playback"
            subprocess.run(
                [binary, "--connect", "--socket", str(endpoint)],
                input="", capture_output=True, text=True, env=env, check=True, timeout=5,
            )
            assert "playing / 137 BPM" in command("status"), "EOF must preserve playback"
            assert "Invalid" in command("step 2 9 garbage", expected=1)
            assert "........x......." in command("status"), "invalid commands must not toggle steps"

            # An abandoned/oversized request and a closed response peer cannot kill audio.
            with socket.socket(socket.AF_UNIX) as peer:
                peer.connect(str(endpoint))
                peer.sendall(b"x" * 4097 + b"\n")
                assert b"Invalid" in peer.recv(4096)
            with socket.socket(socket.AF_UNIX) as peer:
                peer.connect(str(endpoint))
                peer.sendall(b"status\n")
            assert "playing / 137 BPM" in command("status")

            duplicate = subprocess.run(
                [binary, "--serve", "--socket", str(endpoint)],
                capture_output=True, text=True, env=env, timeout=5,
            )
            assert duplicate.returncode != 0, "a duplicate service must not replace the socket"
            assert "playing / 137 BPM" in command("status")
            command("stop")
            assert "stopped / 137 BPM" in command("status")
            for args in (["--serve", "--terminal"], ["--connect", "--render-demo", "ignored.wav"]):
                assert subprocess.run([binary, *args], capture_output=True, env=env).returncode != 0
        finally:
            service.terminate()
            try:
                service.wait(timeout=5)
            except subprocess.TimeoutExpired:
                service.kill()
                service.wait()
                raise
        assert service.returncode == 0, "SIGTERM must stop the service cleanly"
        assert not endpoint.exists(), "shutdown must remove its socket"

print("Service/client integration passed.")
