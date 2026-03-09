// INCLUDES

#include <ctype.h>
#include <stdlib.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

// DEFINES
#define CTRL_KEY(k) ((k) & 0x1f)
#define ESC_KEY '\x1b'

enum editorMode {
  NORMAL = 0,
  INSERT = 1
};

// STRUCTS

typedef struct erow {
  int size;
  char *chars;
} erow;

struct termios orig_termios;

struct config {
  int cx, cy;
  int screenRows;
  int screenCols;
  int numrows;
  erow *row;
  int mode;
  char *filename;
  struct termios orig_termios;
};

struct config E;

// ROW OPERATIONS

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.numrows++;
}

void editorAppendRow(char *s, size_t len) {
  editorInsertRow(E.numrows, s, len);
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
}

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
  }
  E.cy++;
  E.cx = 0;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
}

void editorDelCharLeft() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0) return;
  erow *row = &E.row[E.cy];
  editorRowDelChar(row, E.cx - 1);
  E.cx--;
}

void editorDelCharUnder() {
  if (E.cy == E.numrows) return;
  erow *row = &E.row[E.cy];
  if (E.cx >= row->size) return;
  editorRowDelChar(row, E.cx);
}

// FILE I/O

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  FILE *fp = fopen(filename, "r");
  if (!fp) return;

  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
}

void editorSave() {
  if (E.filename == NULL) return;
  FILE *fp = fopen(E.filename, "w");
  if (!fp) return;
  int i;
  for (i = 0; i < E.numrows; i++) {
    fprintf(fp, "%s\n", E.row[i].chars);
  }
  fclose(fp);
}

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

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

int getWindowSize(int* rows, int* cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursor(rows, cols);
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
    if (y == E.screenRows - 1) {
      char status[80];
      int len = snprintf(status, sizeof(status), "\x1b[7m %s \x1b[0m", E.mode == INSERT ? "-- INSERT --" : "-- NORMAL --");
      write(STDOUT_FILENO, status, len);
    } else {
      if (y < E.numrows) {
        int len = E.row[y].size;
        if (len > E.screenCols) len = E.screenCols;
        write(STDOUT_FILENO, E.row[y].chars, len);
      } else {
        write(STDOUT_FILENO, "~", 1);
      }
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void refreshEditor() {
  // the 4 means we are writing 4 bytes to output, \x1b is the escape character
  // escape sequences always start with 27[, the 2J command clears the entire screen
  write(STDOUT_FILENO, "\x1b[2J", 4);

  // Here we are only sending 3 bytes, and the H command moves the cursor
  write(STDOUT_FILENO, "\x1b[H", 3);

  drawRows();
  
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  write(STDOUT_FILENO, buf, strlen(buf));
}

// INPUT

void moveCursor(char key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

  switch (key) {
    case 'h':
      if (E.cx > 0) E.cx--;
      break;
    case 'l':
      if (row && E.cx < row->size) E.cx++;
      else if (!row && E.cx < E.screenCols - 1) E.cx++;
      break;
    case 'k':
      if (E.cy > 0) E.cy--;
      break;
    case 'j':
      if (E.cy < E.numrows) E.cy++;
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void processKeypress() {
  char c = readKey();

  if (E.mode == NORMAL) {
    switch (c) {
      case CTRL_KEY('q'):
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
      case CTRL_KEY('s'):
        editorSave();
        break;
      case 'i':
        E.mode = INSERT;
        break;
      case 'x':
        editorDelCharUnder();
        break;
      case ':':
        {
          // Very basic command line reading
          char cmd[32];
          int cmd_len = 0;
          while (1) {
            char c = readKey();
            if (c == ESC_KEY) {
              break;
            } else if (c == '\r') {
              cmd[cmd_len] = '\0';
              if (strcmp(cmd, "w") == 0) {
                editorSave();
              } else if (strcmp(cmd, "q") == 0) {
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
              } else if (strcmp(cmd, "wq") == 0) {
                editorSave();
                write(STDOUT_FILENO, "\x1b[2J", 4);
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
              }
              break;
            } else if (c == 127 || c == CTRL_KEY('h')) {
              if (cmd_len > 0) cmd_len--;
            } else if (!iscntrl(c) && cmd_len < 31) {
              cmd[cmd_len++] = c;
            }
          }
        }
        break;
      case 'h':
      case 'j':
      case 'k':
      case 'l':
        moveCursor(c);
        break;
    }
  } else if (E.mode == INSERT) {
    switch (c) {
      case ESC_KEY:
        E.mode = NORMAL;
        break;
      case 127: // BACKSPACE
      case CTRL_KEY('h'):
        editorDelCharLeft();
        break;
      case '\r':
        editorInsertNewline();
        break;
      case CTRL_KEY('s'):
        editorSave();
        break;
      case CTRL_KEY('q'): // Allow quitting in insert mode too for convenience
        write(STDOUT_FILENO, "\x1b[2J", 4);
        write(STDOUT_FILENO, "\x1b[H", 3);
        exit(0);
        break;
      default:
        if (!iscntrl(c)) {
          editorInsertChar(c);
        }
        break;
    }
  }
}

void init() {
  E.cx = 0;
  E.cy = 0;
  E.numrows = 0;
  E.row = NULL;
  E.mode = NORMAL;
  E.filename = NULL;
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}

// MAIN

int main(int argc, char *argv[]) {
  enableRawMode();
  init();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    refreshEditor();
    processKeypress();
  }
  return 0;
}
