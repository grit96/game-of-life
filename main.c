#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ncurses.h>

#include "maths.h"


#define HEIGHT (LINES)
#define WIDTH (COLS)

#define WIN_HEIGHT (HEIGHT)
#define WIN_WIDTH (25)
#define WIN_STARTX (WIDTH - WIN_WIDTH)
#define WIN_STARTY (0)

#define COORD(y, x) ((y) * WIDTH + (x))

#define N_BUFFERS (10)

typedef struct
{
  bool state;
} Cell;

typedef struct
{
  Cell * cells;
} Cells;

typedef struct
{
  int buffer_size;
  int next_buf;
  Cells buffers[N_BUFFERS];
  Cells tmp_buf;
  Cells head;
  Cells copied;
  int copied_dy;
  int copied_dx;
} CellBuffers;

typedef struct
{
  bool stats;
  bool trace;
  bool erase;

  bool line;
  int line_sy;
  int line_sx;
  int line_ey;
  int line_ex;

  bool circle;
  int circle_y;
  int circle_x;
  int circle_r;

  bool rect;
  int rect_sy;
  int rect_sx;
  int rect_ey;
  int rect_ex;

  bool copy;
  int copy_sy;
  int copy_sx;
  int copy_ey;
  int copy_ex;
} State;

static int DOT = COLOR_PAIR(1) | ' ';
static int GUIDE = COLOR_PAIR(2) | ' ';


void
update_cell(Cells * cells, int y, int x, int state, int chr)
{
  if (y >= 0 &&
      x >= 0 &&
      y < HEIGHT &&
      x < WIDTH)
  {
    if (cells != NULL)
    {
      Cell * cell = cells->cells + COORD(y, x);
      if (state == 2)
      {
        state = cell->state;
      }
      else
      {
        cell->state = state;
      }
    }
    mvaddch(y, x, state ? chr : ' ');
  }
}

void
reset_cells(Cells * cells, int state)
{
  int y, x;
  for (y = 0; y < HEIGHT; y++)
  {
    for (x = 0; x < WIDTH; x++)
    {
      update_cell(cells, y, x, state, DOT);
    }
  }
}

void
copy_buffer_range(Cells * from, Cells * to, int from_sy, int from_sx, int from_ey, int from_ex, int to_sy, int to_sx)
{
  if (from_sy > from_ey)
  {
    int tmp = from_sy;
    from_sy = from_ey;
    from_ey = tmp;
  }
  if (from_sx > from_ex)
  {
    int tmp = from_sx;
    from_sx = from_ex;
    from_ex = tmp;
  }

  int dy, dx;
  for (dy = 0; dy <= (from_ey-from_sy); dy++)
  {
    for (dx = 0; dx <= (from_ex-from_sx); dx++)
    {
      Cell * cell_from = from->cells + COORD(from_sy+dy, from_sx+dx);
      Cell * cell_to = to->cells + COORD(to_sy+dy, to_sx+dx);
      *cell_to = *cell_from;
    }
  }
}

void
draw_buffer_range(Cells * cells, int sy, int sx, int ey, int ex)
{
  ey += SIGN(ey - sy);
  ex += SIGN(ex - sx);

  if (sy > ey)
  {
    int tmp = sy;
    sy = ey;
    ey = tmp;
  }
  if (sx > ex)
  {
    int tmp = sx;
    sx = ex;
    ex = tmp;
  }

  int y, x;
  for (y = sy; y <= ey; y++)
  {
    for (x = sx; x <= ex; x++)
    {
      Cell * cell = cells->cells + COORD(y, x);
      mvaddch(y, x, cell->state ? DOT : ' ');
    }
  }
}

void
draw_buffer(Cells * cells)
{
  draw_buffer_range(cells, 0, 0, HEIGHT, WIDTH);
}

void
update_stats(State * state, int next_buf)
{
  int i = 0;
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "STATE");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "trace %d", state->trace);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "erase %d", state->erase);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "line %d", state->line);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "circle %d", state->circle);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "rectangle %d", state->rect);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "copy %d", state->copy);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "last buffer %2d", next_buf);
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "HELP");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "trace (t)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "circle (o)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "line (l)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "rectangle (r)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "copy (d)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "paste (p)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "clear (c)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "save to buffer (s)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "load from buffer (0-9)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "next frame (enter)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "quit (q)");
  mvprintw(WIN_STARTY + i++, WIN_STARTX, "toggle sidebar (?)");
}

void
add_circle(Cells * cells, int y, int x, int r, int chr, int state)
{
  if (r == 0) return update_cell(cells, y, x, state, chr);

  int a2 = 4*r*r;
  int b2 = r*r;
  int dx, dy, s;

  for (dx = 0, dy = r, s = 2*b2+a2*(1-2*r); b2*dx <= a2*dy; dx++)
  {
    update_cell(cells, y + dy, x + dx, state, chr);
    update_cell(cells, y + dy, x - dx, state, chr);
    update_cell(cells, y - dy, x + dx, state, chr);
    update_cell(cells, y - dy, x - dx, state, chr);

    if (s >= 0)
    {
      s += 4*a2*(1-dy);
      dy--;
    }
    s += b2*((4*dx)+6);
  }

  for (dx = 2*r, dy = 0, s = 2*a2+b2*(1-4*r); a2*dy <= b2*dx; dy++)
  {
    update_cell(cells, y + dy, x + dx, state, chr);
    update_cell(cells, y + dy, x - dx, state, chr);
    update_cell(cells, y - dy, x + dx, state, chr);
    update_cell(cells, y - dy, x - dx, state, chr);

    if (s >= 0)
    {
      s += 4*b2*(1-dx);
      dx--;
    }
    s += a2*((4*dy)+6);
  }
}

void
add_line(Cells * cells, int sy, int sx, int ey, int ex, int chr, int state)
{
  int w = ex - sx;
  int h = ey - sy;
  w += SIGN(w);
  h += SIGN(h);

  int len = sqrt(w*w + h*h);

  int i;
  for (i = 0; i < len; i++)
  {
    int dy = (float)(h * i) / len;
    int dx = (float)(w * i) / len;
    update_cell(cells, sy + dy, sx + dx, state, chr);
  }
}

void
add_rect(Cells * cells, int sy, int sx, int ey, int ex, int chr, int state)
{
  add_line(cells, sy, sx, sy, ex, chr, state);
  add_line(cells, sy, ex, ey, ex, chr, state);
  add_line(cells, ey, ex, ey, sx, chr, state);
  add_line(cells, ey, sx, sy, sx, chr, state);
}

int
neighbours(Cells * cells, int y, int x)
{
  int dy, dx;
  int n = 0;

  for (dy = -1; dy <= 1; dy++)
  {
    for (dx = -1; dx <= 1; dx++)
    {
      if (!(dy == 0 && dx == 0) &&
          y + dy >= 0 &&
          x + dx >= 0 &&
          y + dy < HEIGHT &&
          x + dx < WIDTH)
      {
        Cell * current_cell = cells->cells + COORD(y + dy, x + dx);
        n += current_cell->state;
      }
    }
  }

  return n;
}

void
tick(CellBuffers * cell_buffers)
{
  int y, x, n;

  memcpy(cell_buffers->tmp_buf.cells, cell_buffers->head.cells, cell_buffers->buffer_size);

  for (y = 0; y < HEIGHT; y++)
  {
    for (x = 0; x < WIDTH; x++)
    {
      // set 'alive' state of each cell depending on the number of neighbours
      n = neighbours(&(cell_buffers->head), y, x);

      Cell * current_cell = cell_buffers->tmp_buf.cells + COORD(y, x);

      if (n < 2 || n > 3)
      {
        current_cell->state = false;
      }
      if (n == 3)
      {
        current_cell->state = true;
      }

      mvaddch(y, x, current_cell->state ? DOT : ' ');
    }
  }

  memcpy(cell_buffers->head.cells, cell_buffers->tmp_buf.cells, cell_buffers->buffer_size);

  refresh();
}

void
clear_guides(State * state, CellBuffers * cell_buffers)
{
  if (state->circle)
  {
    add_circle(&(cell_buffers->head), state->circle_y, state->circle_x, state->circle_r, DOT, 2);
  }
  if (state->line)
  {
    add_line(&(cell_buffers->head), state->line_sy, state->line_sx, state->line_ey, state->line_ex, DOT, 2);
  }
  if (state->rect)
  {
    add_rect(&(cell_buffers->head), state->rect_sy, state->rect_sx, state->rect_ey, state->rect_ex, DOT, 2);
  }
  if (state->copy)
  {
    add_rect(&(cell_buffers->head), state->copy_sy, state->copy_sx, state->copy_ey, state->copy_ex, DOT, 2);
  }
}

void
draw_guides(State * state, int y, int x)
{
  if (state->circle)
  {
    state->circle_r = sqrt(pow(y - state->circle_y, 2) + pow((x - state->circle_x) / 2, 2));
    add_circle(NULL, state->circle_y, state->circle_x, state->circle_r, GUIDE, 1);
  }
  if (state->line)
  {
    state->line_ey = y;
    state->line_ex = x;
    add_line(NULL, state->line_sy, state->line_sx, state->line_ey, state->line_ex, GUIDE, 1);
  }
  if (state->rect)
  {
    state->rect_ey = y;
    state->rect_ex = x;
    add_rect(NULL, state->rect_sy, state->rect_sx, state->rect_ey, state->rect_ex, GUIDE, 1);
  }
  if (state->copy)
  {
    state->copy_ey = y;
    state->copy_ex = x;
    add_rect(NULL, state->copy_sy, state->copy_sx, state->copy_ey, state->copy_ex, GUIDE, 1);
  }
}

void
copy(CellBuffers * cell_buffers, int sy, int sx, int ey, int ex)
{
  copy_buffer_range(&(cell_buffers->head), &(cell_buffers->copied), sy, sx, ey, ex, 0, 0);
  cell_buffers->copied_dy = ey - sy;
  cell_buffers->copied_dx = ex - sx;
}

void
paste(CellBuffers * cell_buffers, int y, int x)
{
  copy_buffer_range(&(cell_buffers->copied), &(cell_buffers->head), 0, 0, cell_buffers->copied_dy, cell_buffers->copied_dx, y, x);
  draw_buffer_range(&(cell_buffers->head), y, x, y + cell_buffers->copied_dy, x + cell_buffers->copied_dx);
}

bool
keyboard(State * state, CellBuffers * cell_buffers, int c)
{
  int y, x;
  getyx(stdscr, y, x);

  switch (c) {
    case KEY_LEFT:
    {
      if (x > 0) x--;
    } break;
    case KEY_RIGHT:
    {
      if (x < WIDTH-1) x++;
    } break;
    case KEY_UP:
    {
      if (y > 0) y--;
    } break;
    case KEY_DOWN:
    {
      if (y < HEIGHT-1) y++;
    } break;
    case ' ':
    {
      // toggle currently selected cell 'alive' state
      Cell * cursor_cell = cell_buffers->head.cells + COORD(y, x);
      update_cell(&(cell_buffers->head), y, x, !(cursor_cell->state), DOT);
    } break;
    case 'e':
    {
      state->erase = !state->erase;
    } break;
    case 't':
    {
      state->trace = !state->trace;
    } break;
    case 's':
    {
      cell_buffers->next_buf = (cell_buffers->next_buf + 1) % N_BUFFERS;
      memcpy((cell_buffers->buffers + cell_buffers->next_buf)->cells, cell_buffers->head.cells, cell_buffers->buffer_size);
    } break;
    case 'l':
    {
      if (state->line)
      {
        add_line(&(cell_buffers->head), state->line_sy, state->line_sx, state->line_ey, state->line_ex, DOT, !state->erase);
        state->line = false;
      }
      else
      {
        state->line_sy = y;
        state->line_sx = x;
        state->line_ey = y;
        state->line_ex = x;
        state->line = true;
      }
    } break;
    case 'o':
    {
      if (state->circle)
      {
        add_circle(&(cell_buffers->head), state->circle_y, state->circle_x, state->circle_r, DOT, !state->erase);
        state->circle = false;
      }
      else
      {
        state->circle_y = y;
        state->circle_x = x;
        state->circle_r = 0;
        state->circle = true;
      }
    } break;
    case 'r':
    {
      if (state->rect)
      {
        add_rect(&(cell_buffers->head), state->rect_sy, state->rect_sx, state->rect_ey, state->rect_ex, DOT, !state->erase);
        state->rect = false;
      }
      else
      {
        state->rect_sy = y;
        state->rect_sx = x;
        state->rect_ey = y;
        state->rect_ex = x;
        state->rect = true;
      }
    } break;
    case 'c':
    {
      reset_cells(&(cell_buffers->head), 0);
    } break;
    case 'd':
    {
      if (state->copy)
      {
        clear_guides(state, cell_buffers);
        copy(cell_buffers, state->copy_sy, state->copy_sx, state->copy_ey, state->copy_ex);
        state->copy = false;
      }
      else
      {
        state->copy_sy = y;
        state->copy_sx = x;
        state->copy_ey = y;
        state->copy_ex = x;
        state->copy = true;
      }
    } break;
    case 'p':
    {
      paste(cell_buffers, y, x);
    } break;
    case 10:
    {
      tick(cell_buffers);
    } break;
    case 'q':
    {
      if (state->circle || state->line || state->rect || state->trace || state->copy)
      {
        clear_guides(state, cell_buffers);
        state->circle = state->line = state->rect = state->trace = state->copy = false;
      }
      else
      {
        return false;
      }
    } break;
    case '?':
    {
      state->stats = !state->stats;
      draw_buffer_range(&(cell_buffers->head), WIN_STARTY, WIN_STARTX, WIN_STARTY + HEIGHT, WIN_STARTX + WIDTH);
    } break;
    default:
    {
      if ('0' <= c && c <= '9')
      {
        memcpy(cell_buffers->head.cells, (cell_buffers->buffers + (c - '0'))->cells, cell_buffers->buffer_size);
        draw_buffer(&(cell_buffers->head));
      }
    }
  }

  clear_guides(state, cell_buffers);
  draw_guides(state, y, x);

  if (state->trace) update_cell(&(cell_buffers->head), y, x, !state->erase, DOT);
  if (state->stats) update_stats(state, cell_buffers->next_buf);

  move(y, x);
  refresh();

  return true;
}

void
init_curses()
{
  initscr();
  start_color();
  use_default_colors();

  init_pair(1, COLOR_BLACK, COLOR_BLUE);
  init_pair(2, COLOR_BLACK, COLOR_YELLOW);

  raw();
  noecho();
  keypad(stdscr, TRUE);
}

void
new_cell_buffer(Cells * cells, int size)
{
  cells->cells = malloc(size);
}

void
init_game(State * state, CellBuffers * cell_buffers)
{
  state->stats = true;
  state->trace = false;
  state->erase = false;
  state->line = false;
  state->circle = false;
  state->rect = false;
  state->copy = false;

  cell_buffers->next_buf = -1;
  cell_buffers->buffer_size = WIDTH * HEIGHT * sizeof(int);

  int buf_index;
  for (buf_index = 0;
       buf_index < N_BUFFERS;
       ++buf_index)
  {
    new_cell_buffer((cell_buffers->buffers + buf_index), cell_buffers->buffer_size);
  }

  new_cell_buffer(&(cell_buffers->head), cell_buffers->buffer_size);
  new_cell_buffer(&(cell_buffers->tmp_buf), cell_buffers->buffer_size);
  new_cell_buffer(&(cell_buffers->copied), cell_buffers->buffer_size);

  add_circle(&(cell_buffers->head), 1 * HEIGHT / 4, 1 * WIDTH / 4, (2 * HEIGHT > WIDTH ? WIDTH / 4 : HEIGHT) / 4, DOT, 1);
  add_circle(&(cell_buffers->head), 3 * HEIGHT / 4, 1 * WIDTH / 4, (2 * HEIGHT > WIDTH ? WIDTH / 4 : HEIGHT) / 4, DOT, 1);
  add_circle(&(cell_buffers->head), 3 * HEIGHT / 4, 3 * WIDTH / 4, (2 * HEIGHT > WIDTH ? WIDTH / 4 : HEIGHT) / 4, DOT, 1);
  add_circle(&(cell_buffers->head), 1 * HEIGHT / 4, 3 * WIDTH / 4, (2 * HEIGHT > WIDTH ? WIDTH / 4 : HEIGHT) / 4, DOT, 1);
  add_circle(&(cell_buffers->head), 2 * HEIGHT / 4, 2 * WIDTH / 4, (2 * HEIGHT > WIDTH ? WIDTH / 4 : HEIGHT) / 4, DOT, 1);

  update_stats(state, cell_buffers->next_buf);

  move(0, 0);
}

void
deinit(CellBuffers * cell_buffers)
{
  endwin();

  int buf_index;
  for (buf_index = 0;
       buf_index < N_BUFFERS;
       ++buf_index)
  {
    free((cell_buffers->buffers + buf_index)->cells);
  }
  free(cell_buffers->tmp_buf.cells);
}

int
main()
{
  CellBuffers cell_buffers;
  State state;

  init_curses();
  init_game(&state, &cell_buffers);

  while (keyboard(&state, &cell_buffers, getch()));

  deinit(&cell_buffers);

  return 0;
}
