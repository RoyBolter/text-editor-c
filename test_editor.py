import pty
import os
import time
import subprocess
import select
import fcntl
import termios
import struct

def test_editor():
    master, slave = pty.openpty()
    winsize = struct.pack("HHHH", 24, 80, 0, 0)
    fcntl.ioctl(slave, termios.TIOCSWINSZ, winsize)
    
    p = subprocess.Popen(['./text-editor'], stdin=slave, stdout=slave, stderr=slave, preexec_fn=os.setsid)
    os.close(slave)
    
    def read_output(timeout=0.5):
        output = b""
        end_time = time.time() + timeout
        while time.time() < end_time:
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                data = os.read(master, 4096)
                if not data:
                    break
                output += data
        return output.decode('utf-8', errors='replace')

    print("--- Starting Editor ---")
    out = read_output(1.0)
    if '-- NORMAL --' in out and '\x1b[1;1H' in out:
        print("[PASS] Initial status is NORMAL and cursor position is correct (1;1)")
    else:
        print("[FAIL] Initial state missing. Output:", repr(out))

    print("--- Testing 'i' (Insert Mode) ---")
    os.write(master, b'i')
    out = read_output()
    if '-- INSERT --' in out:
        print("[PASS] Switched to INSERT mode successfully")
    else:
        print("[FAIL] Did not switch to INSERT mode. Output:", repr(out))

    print("--- Testing text insertion with Newline ---")
    os.write(master, b'H')
    os.write(master, b'i')
    os.write(master, b'\r')
    os.write(master, b'W')
    os.write(master, b'o')
    os.write(master, b'r')
    os.write(master, b'l')
    os.write(master, b'd')
    out = read_output()
    if 'Hi\r\nWorld' in out or ('Hi' in out and 'World' in out):
        print("[PASS] Inserted text with newline successfully")
    else:
        print("[FAIL] Failed to insert text with newline. Output:", repr(out))

    print("--- Testing 'ESC' (Back to Normal Mode) ---")
    os.write(master, b'\x1b') # ESC
    time.sleep(0.1)
    
    print("--- Testing Cursor Snapping ---")
    # Cursor is currently at end of "World" (row 2, col 5). Move up to "Hi" (row 1, max col 2).
    os.write(master, b'k')
    out = read_output()
    # It snapped to cx=2. Let's move left to select 'i' and delete it.
    os.write(master, b'h') # move to cx=1
    os.write(master, b'x') # deletes 'i'
    out = read_output()
    if 'H\r\nWorld' in out or ('H' in out and 'World' in out and 'i' not in out):
        print("[PASS] Cursor snapped and deleted character successfully")
    else:
        print("[FAIL] Cursor snapping / delete failed. Output:", repr(out))

    print("--- Quitting (Ctrl-Q) ---")
    os.write(master, b'\x11') # Ctrl-Q
    try:
        p.wait(timeout=2)
        print("[PASS] Editor quit successfully")
    except subprocess.TimeoutExpired:
        print("[FAIL] Editor did not quit in time")
        p.kill()

if __name__ == '__main__':
    try:
        test_editor()
    except Exception as e:
        print(f"[ERROR] Test failed with exception: {e}")
