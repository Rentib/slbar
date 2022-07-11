/* See LICENSE file for copyright and license details. */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>

#define LEN(a)    (sizeof(a) / sizeof(a)[0])
#define CMDLEN    (64)
#define BARLEN    (LEN(modules) * CMDLEN + 1)

typedef unsigned int uint;
typedef struct {
  char* cmd; /* command to be executed */
  uint  upd; /* update interval (0 means no update interval) */
  uint  sig; /* update signal (0 means no signal) */
} Module;

/* functions */
static int gcd(int a, int b); /* greatest common divisor */
static int lcm(int a, int b); /* least common multiple */
static void die(const char *fmt, ...); /* prints error to stderr and exits */
static void display(); /* displays status bar */
static void loop(); /* main loop of the program */
static void setup(); /* sets up handling signals */
static void sighandler(int sig); /* signal handler */
static void termhandler(); /* SIGTERM handler */
static void update(uint time); /* updates status bar */
static void updatecmd(const char *cmd, char *out, int add_sep); /* updates given module */

/* globals */
static Display *dpy;
static Window root;
static int screen;

#include "config.h"

static int break_loop = 0;
static uint current = 0;
static char barstr[2][BARLEN] = { 0 };
static char cmds[LEN(modules)][CMDLEN] = { 0 };

int
gcd(int a, int b)
{
  return b ? gcd(b, a % b) : a;
}

int
lcm(int a, int b)
{
  return a / gcd(a, b) * b;
}

void
die(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fputc('\n', stderr);

  exit(1);
}

void
display()
{
  uint old = current, new = current ^ 1;
  uint i;

  barstr[new][0] = '\0';

  for (i = 0; i < LEN(modules); i++)
    strcat(barstr[new], cmds[i]);

  if (!(strcmp(barstr[new], barstr[old])))
    return;

  XStoreName(dpy, root, barstr[new]);
  XFlush(dpy);
  current = new;
}

void
loop()
{
  uint i, upd_lcm = 1;

  for (i = 0; i < LEN(modules); i++) {
    upd_lcm = lcm(upd_lcm, modules[i].upd ? modules[i].upd : 1);
    updatecmd(modules[i].cmd, cmds[i], i < LEN(modules) - 1);
  }

  for (i = 0; !break_loop; i %= upd_lcm) {
    update(i++);
    display();
    sleep(1.0);
  }
}

void
setup()
{
  const Module *m;
  uint i;
  screen = DefaultScreen(dpy);
  root = RootWindow(dpy, screen);
  signal(SIGTERM, termhandler);
  signal(SIGINT,  termhandler);
  for (m = modules, i = LEN(modules); i--; m++)
    if (m->sig)
      signal(m->sig + SIGRTMIN, sighandler);
}

void
sighandler(int sig)
{
  const Module *m;
  uint i;
  for (m = modules, i = 0; i < LEN(modules); i++, m++)
    if ((int)m->sig + SIGRTMIN == sig)
      updatecmd(m->cmd, cmds[i], i < LEN(modules) - 1);
  display();
}

void
termhandler()
{
  break_loop = 1;
}

void
update(uint time)
{
  const Module *m;
  uint i;
  for (m = modules, i = 0; i < LEN(modules); i++, m++)
    if (m->upd && time % m->upd == 0)
      updatecmd(m->cmd, cmds[i], i < LEN(modules) - 1);
}

void
updatecmd(const char *cmd, char *out, int add_sep)
{
  FILE *fp;
  size_t len;
  if (!(fp = popen(cmd, "r")))
    return;
  fgets(out, CMDLEN - sep_len * add_sep - 1, fp);
  if (!(len = strlen(out))) {
    pclose(fp);
    return;
  }
  out[len - (out[len - 1] == '\n')] = '\0';
  if (add_sep)
    strcat(out, sep);
  pclose(fp);
}

int
main (void)
{
  if (!(dpy = XOpenDisplay(NULL)))
    die("slbar: cannot open display");

  setup();
  loop();

  XCloseDisplay(dpy);

  return 0;
}
