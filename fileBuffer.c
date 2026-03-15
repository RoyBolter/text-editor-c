#include <stdio.h>
#include <stdlib.h>

struct buffer {
  char *data;
  int size;
};

void saveFile(char *filename, struct buffer *b) {
  FILE *file = fopen(filename, "w");
  if (file == NULL)
    return;

  fwrite(b->data, 1, b->size, file);

  fclose(file);
}
