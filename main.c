#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ncurses.h>

#include "shapes.c"
#include "gol.h"


static int * cells;
static int * buffer;

int main();
void init();
void deinit();
void initcurses();
void tick();
int neighbours(int y, int x);
void add_circle(int y, int x, int radius);


int
main()
{
  init();

  add_circle(1 * LINES / 4, 1 * COLS / 4, (2 * LINES > COLS ? COLS / 4 : LINES) / 4);
  add_circle(3 * LINES / 4, 1 * COLS / 4, (2 * LINES > COLS ? COLS / 4 : LINES) / 4);
  add_circle(3 * LINES / 4, 3 * COLS / 4, (2 * LINES > COLS ? COLS / 4 : LINES) / 4);
  add_circle(1 * LINES / 4, 3 * COLS / 4, (2 * LINES > COLS ? COLS / 4 : LINES) / 4);
  add_circle(2 * LINES / 4, 2 * COLS / 4, (2 * LINES > COLS ? COLS / 4 : LINES) / 4);

  do
  {
    tick();
  }
  while (getch() == 10);

  deinit();

  return 0;
}

void
init()
{
  initcurses();

  cells = malloc(SIZE);
  buffer = malloc(SIZE);
}

void
initcurses()
{
  initscr();
  curs_set(0);
  start_color();
  use_default_colors();

  raw();
  noecho();
  keypad(stdscr, TRUE);
}

void
deinit()
{
  endwin();

  free(cells);
  free(buffer);
}

void
tick()
{
  int y, x, n;

  memcpy(buffer, cells, SIZE);

  for (y = 0; y < LINES; y++)
  {
    for (x = 0; x < COLS; x++)
    {
      n = neighbours(y, x);

      if (n < 2 || n > 3)
      {
        BUFFER(y, x) = 0;
      }
      if (n == 3)
      {
        BUFFER(y, x) = 1;
      }

      mvaddch(y, x, ALIVE(y, x) ? 'X' : ' ');
    }
  }

  memcpy(cells, buffer, SIZE);

  refresh();
}

void
add_circle(int y, int x, int radius)
{
  int i;

  Point * points = malloc(sizeof(Point) * 360);
  get_points((Circle) {radius, (Point) {y, x}}, points);

  for (i = 0; i < 360; i++)
  {
    CELL(points[i].y, points[i].x) = 1;
  }

  free(points);
}

int
neighbours(int y, int x)
{
  int dy, dx, n;

  for (dy = -1; dy <= 1; dy++)
  {
    for (dx = -1; dx <= 1; dx++)
    {
      if (!(dy == 0 && dx == 0) &&
          y + dy >= 0 &&
          x + dx >= 0 &&
          y + dy < LINES &&
          x + dx < COLS)
      {
        n += ALIVE(y + dy, x + dx);
      }
    }
  }

  return n;
}
