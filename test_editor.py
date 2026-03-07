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
    
    def read_output(timeout=0.3):
        output = b""
        end_time = time.time() + timeout
        while time.time() < end_time:
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                data = os.read(master, 4096)
                if not data:
                    break
                output += data
            else:
                break
        return output.decode('utf-8', errors='replace')

    print("--- Starting Editor ---")
    out = read_output(0.5)
    if '\x1b[1;1H' in out:
        print("[PASS] Initial cursor position is correct (1;1)")
    else:
        print("[FAIL] Initial cursor position missing. Output:", repr(out))

    print("--- Testing 'l' (Move Right) ---")
    os.write(master, b'l')
    out = read_output()
    if '\x1b[1;2H' in out:
        print("[PASS] Cursor moved right to (1;2)")
    else:
        print("[FAIL] Cursor did not move right. Output:", repr(out))

    print("--- Testing 'j' (Move Down) ---")
    os.write(master, b'j')
    out = read_output()
    if '\x1b[2;2H' in out:
        print("[PASS] Cursor moved down to (2;2)")
    else:
        print("[FAIL] Cursor did not move down. Output:", repr(out))

    print("--- Testing 'i' (Insert Mode) ---")
    os.write(master, b'i')
    out = read_output()
    print("--- Testing 'l' in Insert Mode (Should NOT move) ---")
    os.write(master, b'l')
    out = read_output()
    if '\x1b[2;3H' not in out:
        print("[PASS] Cursor did not move (stayed at 2;2) because we are in Insert Mode")
    else:
        print("[FAIL] Cursor moved in Insert Mode!")

    print("--- Testing 'ESC' (Back to Normal Mode) and 'h' (Move Left) ---")
    os.write(master, b'\x1b') # ESC
    time.sleep(0.1)
    os.write(master, b'h')
    out = read_output()
    if '\x1b[2;1H' in out:
        print("[PASS] Returned to Normal mode and moved left to (2;1)")
    else:
        print("[FAIL] Did not return to Normal mode / move left. Output:", repr(out))

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
