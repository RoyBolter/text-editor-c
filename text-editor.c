// INCLUDES

#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

// DEFINES
#define CTRL_KEY(k) ((k) & 0x1f)

// STRUCTS

struct termios orig_termios;

struct config {
  int screenRows;
  int screenCols;
  struct termios orig_termios;
};

struct config E;

// TERMINAL FUNCTIONS

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

// Raw mode allows the terminal to read the bytes of the characters input as they are typed 
// rather than the default canonical mode which waits for the enter key to be pressed
void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;

  // Disable flags from certain commands like ctrl-c or ctrl-z
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= ~(CS8);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;


  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

char readKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

int getCursor(int* rows, int* cols) {
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
  readKey();
  return -1;
}

int getWindowSize(int* rows, int* cols) {
  struct winsize ws;

  if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) { // Terminal Input/Output Control Get WIN SiZe
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;    // moves the cursor right by 999 and down by 999 without going off screen, fallback windowSize method if ioctl isn't suppoerted
    readKey();
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

//OUTPUT

void drawRows() {
  int y;
  for (y=0; y<E.screenRows; y++) {
    write(STDOUT_FILENO, "~\r\n", 3);
  }
}

void refreshEditor() {
  // the 4 means we are writing 4 bytes to output, \x1b is the escape character
  // escape sequences always start with 27[, the 2J command clears the entire screen
  write(STDOUT_FILENO, "\x1b[2J", 4);

  // Here we are only sending 3 bytes, and the H command moves the cursor
  write(STDOUT_FILENO, "\x1b[H", 3);

  drawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
}

// INPUT

void processKeypress() {
  char c = readKey();

  switch (c) {
    case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

void init() {
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}

// MAIN

int main() {
  enableRawMode();
  init();

  while (1) {
    refreshEditor();
    processKeypress();
  }
  return 0;
}
