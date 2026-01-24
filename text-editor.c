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
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);

  raw.c_lflag &= ~(ECHO | ICANON);

  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();
  
  // Read and print the input byte in decimal form with the character afterwards
  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n",c,c);
    }
  }
  return 0;
}
