# Drives anvil on a pseudo-terminal: open a file, type text, save, quit,
# then check the file on disk. Also exercises arrows, backspace, and the
# status bar.
import os, pty, sys, time, select

def run(args, keys, want_in_output):
    pid, fd = pty.fork()
    if pid == 0:
        os.execv(args[0], args)
    out = b""
    sent = False
    deadline = time.time() + 10
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.2)
        if not sent and b"line 1/1" in out:
            os.write(fd, keys)     # type only after the first draw: earlier
            sent = True            # input is flushed when raw mode begins
        if r:
            try:
                chunk = os.read(fd, 4096)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            break
    alive = True
    try:
        done, status = os.waitpid(pid, os.WNOHANG)
        if done:
            alive = False
    except ChildProcessError:
        alive = False
    if alive:
        import signal as sig
        os.kill(pid, sig.SIGKILL)
        os.waitpid(pid, 0)
        sys.stdout.write("FAIL: anvil did not exit; saw %d bytes of output\n" % len(out))
        sys.stdout.write(repr(out[-400:]) + "\n")
        sys.exit(1)
    os.close(fd)
    return out

path = "/tmp/anvil_pty.txt"
if os.path.exists(path):
    os.unlink(path)

# type "hello", left x2, "X", backspace, enter, "world", ctrl-s, ctrl-q
keys = b"hello\x1b[D\x1b[D" + b"X\x7f" + b"\x1b[F\rworld\x13\x11"
out = run(["build/anvil", path], keys, b"world")
body = open(path, "rb").read()
assert body == b"hello\nworld", f"file content wrong: {body!r}"
assert b"line 2/2" in out.replace(b"\r", b""), "status bar missing line info"
print("PTY SMOKE PASSED")
