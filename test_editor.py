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
    
    # Ensure test file is clear
    test_file = "test_io.txt"
    if os.path.exists(test_file):
        os.remove(test_file)

    p = subprocess.Popen(['./text-editor', test_file], stdin=slave, stdout=slave, stderr=slave, preexec_fn=os.setsid)
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

    print("--- Testing text insertion ---")
    os.write(master, b'F')
    os.write(master, b'i')
    os.write(master, b'l')
    os.write(master, b'e')
    os.write(master, b' ')
    os.write(master, b'I')
    os.write(master, b'/')
    os.write(master, b'O')
    out = read_output()

    print("--- Testing 'ESC' (Back to Normal Mode) ---")
    os.write(master, b'\x1b') # ESC
    time.sleep(0.1)
    
    print("--- Testing :wq (Save and Quit) ---")
    os.write(master, b':')
    time.sleep(0.1)
    os.write(master, b'w')
    time.sleep(0.1)
    os.write(master, b'q')
    time.sleep(0.1)
    os.write(master, b'\r')

    try:
        end_time = time.time() + 2.0
        while time.time() < end_time:
            if p.poll() is not None:
                break
            r, _, _ = select.select([master], [], [], 0.1)
            if r:
                os.read(master, 4096)
        
        if p.poll() is not None:
            print("[PASS] Editor quit successfully")
        else:
            print("[FAIL] Editor did not quit in time")
            p.kill()
    except Exception as e:
        print(f"[FAIL] Editor did not quit in time: {e}")
        p.kill()

    # Check file contents
    if os.path.exists(test_file):
        with open(test_file, 'r') as f:
            content = f.read()
            if content == "File I/O\n":
                print("[PASS] File saved correctly")
            else:
                print(f"[FAIL] File content incorrect: {repr(content)}")
    else:
        print("[FAIL] File was not created")

if __name__ == '__main__':
    try:
        test_editor()
    except Exception as e:
        print(f"[ERROR] Test failed with exception: {e}")
