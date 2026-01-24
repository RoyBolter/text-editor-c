#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

struct termios orig_termios;


// Raw mode allows the terminal to read the bytes of the characters input as they are typed 
// rather than the default canonical mode which waits for the enter key to be pressed
void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;

  // Disable flags from certain commands like ctrl-c or ctrl-z
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;


  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  while (1) {
    // Read and print the input byte in decimal form with the character afterwards
    char c = '\0';
    read(STDIN_FILENO, &c, 1);

    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n",c,c);
    }
    if (c == 'q') break;
  }
  return 0;
}
