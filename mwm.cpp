// MiseEnPlace(or My) WM (MWM)
// A clone of DWM with my changes over time

// {{{ License/Details

/*
MIT/X Consortium License

© 2006-2019 Anselm R Garbe <anselm@garbe.ca>
© 2006-2009 Jukka Salmi <jukka at salmi dot ch>
© 2006-2007 Sander van Dijk <a dot h dot vandijk at gmail dot com>
© 2007-2011 Peter Hartlich <sgkkr at hartlich dot com>
© 2007-2009 Szabolcs Nagy <nszabolcs at gmail dot com>
© 2007-2009 Christof Musik <christof at sendfax dot de>
© 2007-2009 Premysl Hruby <dfenze at gmail dot com>
© 2007-2008 Enno Gottox Boland <gottox at s01 dot de>
© 2008 Martin Hurton <martin dot hurton at gmail dot com>
© 2008 Neale Pickett <neale dot woozle dot org>
© 2009 Mate Nagy <mnagy at port70 dot net>
© 2010-2016 Hiltjo Posthuma <hiltjo@codemadness.org>
© 2010-2012 Connor Lane Smith <cls@lubutu.com>
© 2011 Christoph Lohmann <20h@r-36.net>
© 2015-2016 Quentin Rameau <quinq@fifth.space>
© 2015-2016 Eric Pruitt <eric.pruitt@gmail.com>
© 2016-2017 Markus Teich <markus.teich@stusta.mhn.de>
© 2020-2022 Chris Down <chris@chrisdown.name>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
 */

/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */

// }}} License

// {{{ includes

// Assume XINERAMA is always wanted
#ifndef XINERAMA
#define XINERAMA // for dual monitor support
#endif           /* XINERAMA */

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <array>
#include <ctype.h>
#include <dbus/dbus.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#ifdef __cplusplus
}
#endif
#include <locale.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/types.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

// }}} includes

// {{{ Logging
// TODO: setup logging

// }}} Logging

// {{{ dwm.c

#define ASSERT_LE(x, y)                                                        \
  if (!(x <= y)) {                                                             \
    fprintf(stderr, "Assertion failed: %s <= %s (%d <= %d)\n", #x, #y, x, y);  \
    exit(EXIT_FAILURE);                                                        \
  }

// {{{ drw

/* See LICENSE file for copyright and license details. */

typedef struct {
  Cursor cursor;
} Cur;

typedef struct Fnt {
  Display *dpy;
  unsigned int h;
  XftFont *xfont;
  FcPattern *pattern;
  struct Fnt *next;
} Fnt;

enum { ColFg, ColBg, ColBorder }; /* Clr scheme index */
typedef XftColor Clr;

typedef struct {
  unsigned int w, h;
  Display *dpy;
  int screen;
  Window root;
  Drawable drawable;
  GC gc;
  Clr *scheme;
  Fnt *fonts;
} Drw;

/* Drawable abstraction */
Drw *drw_create(Display *dpy, int screen, Window win, unsigned int w,
                unsigned int h);
void drw_resize(Drw *drw, unsigned int w, unsigned int h);
void drw_free(Drw *drw);

/* Fnt abstraction */
Fnt *drw_fontset_create(Drw *drw, const char *fonts[], size_t fontcount);
void drw_fontset_free(Fnt *set);
unsigned int drw_fontset_getwidth(Drw *drw, const char *text);
unsigned int drw_fontset_getwidth_clamp(Drw *drw, const char *text,
                                        unsigned int n);
void drw_font_getexts(Fnt *font, const char *text, unsigned int len,
                      unsigned int *w, unsigned int *h);

/* Colorscheme abstraction */
void drw_clr_create(Drw *drw, Clr *dest, const char *clrname);
Clr *drw_scm_create(Drw *drw, const char *clrnames[], size_t clrcount);

/* Cursor abstraction */
Cur *drw_cur_create(Drw *drw, int shape);
void drw_cur_free(Drw *drw, Cur *cursor);

/* Drawing context manipulation */
void drw_setfontset(Drw *drw, Fnt *set);
void drw_setscheme(Drw *drw, Clr *scm);

/* Drawing functions */
void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h,
              int filled, int invert);
int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h,
             unsigned int lpad, const char *text, int invert);

/* Map functions */
void drw_map(Drw *drw, Window win, int x, int y, unsigned int w,
             unsigned int h);

// }}} drw

// {{{ util

/* See LICENSE file for copyright and license details. */

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
template <typename T> T *ecalloc_type(size_t nmemb = 1);

// }}} util

// {{{ util.c

/* See LICENSE file for copyright and license details. */

void die(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    fputc(' ', stderr);
    perror(NULL);
  } else {
    fputc('\n', stderr);
  }
  exit(1);
}

void *ecalloc(size_t nmemb, size_t size) {
  void *p;

  if (!(p = calloc(nmemb, size)))
    die("calloc:");
  return p;
}

template <typename T> T *ecalloc_type(size_t nmemb) {
  return static_cast<T *>(ecalloc(nmemb, sizeof(T)));
}

// }}} util.c

// {{{ drw.c

#define UTF_INVALID 0xFFFD
#define UTF_SIZ 4

static const unsigned char utfbyte[UTF_SIZ + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};
static const unsigned char utfmask[UTF_SIZ + 1] = {0xC0, 0x80, 0xE0, 0xF0,
                                                   0xF8};
static const long utfmin[UTF_SIZ + 1] = {0, 0, 0x80, 0x800, 0x10000};
static const long utfmax[UTF_SIZ + 1] = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF,
                                         0x10FFFF};

static long utf8decodebyte(const char c, size_t *i) {
  for (*i = 0; *i < (UTF_SIZ + 1); ++(*i)) {
    if (((unsigned char)c & utfmask[*i]) == utfbyte[*i]) {
      return (unsigned char)c & ~utfmask[*i];
    }
  }
  return 0;
}

static size_t utf8validate(long *u, size_t i) {
  if (!BETWEEN(*u, utfmin[i], utfmax[i]) || BETWEEN(*u, 0xD800, 0xDFFF)) {
    *u = UTF_INVALID;
  }
  for (i = 1; *u > utfmax[i]; ++i) {
    ;
  }
  return i;
}

static size_t utf8decode(const char *c, long *u, size_t clen) {
  size_t i, j, len, type;
  long udecoded;
  *u = UTF_INVALID;
  if (!clen) {
    return 0;
  }
  udecoded = utf8decodebyte(c[0], &len);
  if (!BETWEEN(len, 1, UTF_SIZ)) {
    return 1;
  }
  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | utf8decodebyte(c[i], &type);
    if (type) {
      return j;
    }
  }
  if (j < len) {
    return 0;
  }
  *u = udecoded;
  utf8validate(u, len);
  return len;
}

Drw *drw_create(Display *dpy, int screen, Window root, unsigned int w,
                unsigned int h) {
  Drw *drw = ecalloc_type<Drw>();
  drw->dpy = dpy;
  drw->screen = screen;
  drw->root = root;
  drw->w = w;
  drw->h = h;
  drw->drawable = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
  drw->gc = XCreateGC(dpy, root, 0, NULL);
  XSetLineAttributes(dpy, drw->gc, 1, LineSolid, CapButt, JoinMiter);
  return drw;
}

void drw_resize(Drw *drw, unsigned int w, unsigned int h) {
  if (!drw) {
    return;
  }
  drw->w = w;
  drw->h = h;
  if (drw->drawable) {
    XFreePixmap(drw->dpy, drw->drawable);
  }
  drw->drawable = XCreatePixmap(drw->dpy, drw->root, w, h,
                                DefaultDepth(drw->dpy, drw->screen));
}

void drw_free(Drw *drw) {
  XFreePixmap(drw->dpy, drw->drawable);
  XFreeGC(drw->dpy, drw->gc);
  drw_fontset_free(drw->fonts);
  free(drw);
}

/* This function is an implementation detail. Library users should use
 * drw_fontset_create instead.
 */
static Fnt *xfont_create(Drw *drw, const char *fontname,
                         FcPattern *fontpattern) {
  Fnt *font;
  XftFont *xfont = NULL;
  FcPattern *pattern = NULL;

  if (fontname) {
    /* Using the pattern found at font->xfont->pattern does not yield the
     * same substitution results as using the pattern returned by
     * FcNameParse; using the latter results in the desired fallback
     * behaviour whereas the former just results in missing-character
     * rectangles being drawn, at least with some fonts. */
    if (!(xfont = XftFontOpenName(drw->dpy, drw->screen, fontname))) {
      fprintf(stderr, "error, cannot load font from name: '%s'\n", fontname);
      return NULL;
    }
    if (!(pattern = FcNameParse((FcChar8 *)fontname))) {
      fprintf(stderr, "error, cannot parse font name to pattern: '%s'\n",
              fontname);
      XftFontClose(drw->dpy, xfont);
      return NULL;
    }
  } else if (fontpattern) {
    if (!(xfont = XftFontOpenPattern(drw->dpy, fontpattern))) {
      fprintf(stderr, "error, cannot load font from pattern.\n");
      return NULL;
    }
  } else {
    die("no font specified.");
  }

  font = ecalloc_type<Fnt>();
  font->xfont = xfont;
  font->pattern = pattern;
  font->h = xfont->ascent + xfont->descent;
  font->dpy = drw->dpy;

  return font;
}

static void xfont_free(Fnt *font) {
  if (!font) {
    return;
  }
  if (font->pattern) {
    FcPatternDestroy(font->pattern);
  }
  XftFontClose(font->dpy, font->xfont);
  free(font);
}

Fnt *drw_fontset_create(Drw *drw, const char *fonts[], size_t fontcount) {
  Fnt *cur, *ret = NULL;
  size_t i;
  if (!drw || !fonts) {
    return NULL;
  }
  for (i = 1; i <= fontcount; i++) {
    if ((cur = xfont_create(drw, fonts[fontcount - i], NULL))) {
      cur->next = ret;
      ret = cur;
    }
  }
  return (drw->fonts = ret);
}

void drw_fontset_free(Fnt *font) {
  if (font) {
    drw_fontset_free(font->next);
    xfont_free(font);
  }
}

void drw_clr_create(Drw *drw, Clr *dest, const char *clrname) {
  if (!drw || !dest || !clrname) {
    return;
  }
  if (!XftColorAllocName(drw->dpy, DefaultVisual(drw->dpy, drw->screen),
                         DefaultColormap(drw->dpy, drw->screen), clrname,
                         dest)) {
    die("error, cannot allocate color '%s'", clrname);
  }
}

/* Wrapper to create color schemes. The caller has to call free(3) on the
 * returned color scheme when done using it. */
Clr *drw_scm_create(Drw *drw, const char *clrnames[], size_t clrcount) {
  size_t i;
  Clr *ret;
  /* need at least two colors for a scheme */
  if (!drw || !clrnames || clrcount < 2 ||
      !(ret = ecalloc_type<XftColor>(clrcount))) {
    return NULL;
  }
  for (i = 0; i < clrcount; i++) {
    drw_clr_create(drw, &ret[i], clrnames[i]);
  }
  return ret;
}

void drw_setfontset(Drw *drw, Fnt *set) {
  if (drw) {
    drw->fonts = set;
  }
}

void drw_setscheme(Drw *drw, Clr *scm) {
  if (drw) {
    drw->scheme = scm;
  }
}

void drw_rect(Drw *drw, int x, int y, unsigned int w, unsigned int h,
              int filled, int invert) {
  if (!drw || !drw->scheme) {
    return;
  }
  XSetForeground(drw->dpy, drw->gc,
                 invert ? drw->scheme[ColBg].pixel : drw->scheme[ColFg].pixel);
  if (filled) {
    XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
  } else {
    XDrawRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w - 1, h - 1);
  }
}

int drw_text(Drw *drw, int x, int y, unsigned int w, unsigned int h,
             unsigned int lpad, const char *text, int invert) {
  int i, ty, ellipsis_x = 0;
  unsigned int tmpw, ew, ellipsis_w = 0, ellipsis_len;
  XftDraw *d = NULL;
  Fnt *usedfont, *curfont, *nextfont;
  int utf8strlen, utf8charlen, render = x || y || w || h;
  long utf8codepoint = 0;
  const char *utf8str;
  FcCharSet *fccharset;
  FcPattern *fcpattern;
  FcPattern *match;
  XftResult result;
  int charexists = 0, overflow = 0;
  /* keep track of a couple codepoints for which we have no match. */
  enum { nomatches_len = 64 };
  static struct {
    long codepoint[nomatches_len];
    unsigned int idx;
  } nomatches;
  static unsigned int ellipsis_width = 0;
  if (!drw || (render && (!drw->scheme || !w)) || !text || !drw->fonts) {
    return 0;
  }
  if (!render) {
    w = invert ? invert : ~invert;
  } else {
    XSetForeground(drw->dpy, drw->gc,
                   drw->scheme[invert ? ColFg : ColBg].pixel);
    XFillRectangle(drw->dpy, drw->drawable, drw->gc, x, y, w, h);
    d = XftDrawCreate(drw->dpy, drw->drawable,
                      DefaultVisual(drw->dpy, drw->screen),
                      DefaultColormap(drw->dpy, drw->screen));
    x += lpad;
    w -= lpad;
  }

  usedfont = drw->fonts;
  if (!ellipsis_width && render) {
    ellipsis_width = drw_fontset_getwidth(drw, "...");
  }
  while (1) {
    ew = ellipsis_len = utf8strlen = 0;
    utf8str = text;
    nextfont = NULL;
    while (*text) {
      utf8charlen = utf8decode(text, &utf8codepoint, UTF_SIZ);
      for (curfont = drw->fonts; curfont; curfont = curfont->next) {
        charexists = charexists ||
                     XftCharExists(drw->dpy, curfont->xfont, utf8codepoint);
        if (charexists) {
          drw_font_getexts(curfont, text, utf8charlen, &tmpw, NULL);
          if (ew + ellipsis_width <= w) {
            /* keep track where the ellipsis still fits */
            ellipsis_x = x + ew;
            ellipsis_w = w - ew;
            ellipsis_len = utf8strlen;
          }
          if (ew + tmpw > w) {
            overflow = 1;
            /* called from drw_fontset_getwidth_clamp():
             * it wants the width AFTER the overflow
             */
            if (!render) {
              x += tmpw;
            } else {
              utf8strlen = ellipsis_len;
            }
          } else if (curfont == usedfont) {
            utf8strlen += utf8charlen;
            text += utf8charlen;
            ew += tmpw;
          } else {
            nextfont = curfont;
          }
          break;
        }
      }
      if (overflow || !charexists || nextfont) {
        break;
      } else {
        charexists = 0;
      }
    }

    if (utf8strlen) {
      if (render) {
        ty = y + (h - usedfont->h) / 2 + usedfont->xfont->ascent;
        XftDrawStringUtf8(d, &drw->scheme[invert ? ColBg : ColFg],
                          usedfont->xfont, x, ty, (XftChar8 *)utf8str,
                          utf8strlen);
      }
      x += ew;
      w -= ew;
    }
    if (render && overflow) {
      drw_text(drw, ellipsis_x, y, ellipsis_w, h, 0, "...", invert);
    }
    if (!*text || overflow) {
      break;
    } else if (nextfont) {
      charexists = 0;
      usedfont = nextfont;
    } else {
      /* Regardless of whether or not a fallback font is found, the
       * character must be drawn. */
      charexists = 1;
      for (i = 0; i < nomatches_len; ++i) {
        /* avoid calling XftFontMatch if we know we won't find a match */
        if (utf8codepoint == nomatches.codepoint[i]) {
          goto no_match;
        }
      }
      fccharset = FcCharSetCreate();
      FcCharSetAddChar(fccharset, utf8codepoint);
      if (!drw->fonts->pattern) {
        /* Refer to the comment in xfont_create for more information. */
        die("the first font in the cache must be loaded from a font string.");
      }
      fcpattern = FcPatternDuplicate(drw->fonts->pattern);
      FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
      FcPatternAddBool(fcpattern, FC_SCALABLE, FcTrue);
      FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
      FcDefaultSubstitute(fcpattern);
      match = XftFontMatch(drw->dpy, drw->screen, fcpattern, &result);
      FcCharSetDestroy(fccharset);
      FcPatternDestroy(fcpattern);
      if (match) {
        usedfont = xfont_create(drw, NULL, match);
        if (usedfont &&
            XftCharExists(drw->dpy, usedfont->xfont, utf8codepoint)) {
          for (curfont = drw->fonts; curfont->next; curfont = curfont->next) {
            ; /* NOP */
          }
          curfont->next = usedfont;
        } else {
          xfont_free(usedfont);
          nomatches.codepoint[++nomatches.idx % nomatches_len] = utf8codepoint;
        no_match:
          usedfont = drw->fonts;
        }
      }
    }
  }
  if (d) {
    XftDrawDestroy(d);
  }
  return x + (render ? w : 0);
}

void drw_map(Drw *drw, Window win, int x, int y, unsigned int w,
             unsigned int h) {
  if (!drw) {
    return;
  }
  XCopyArea(drw->dpy, drw->drawable, win, drw->gc, x, y, w, h, x, y);
  XSync(drw->dpy, False);
}

unsigned int drw_fontset_getwidth(Drw *drw, const char *text) {
  if (!drw || !drw->fonts || !text) {
    return 0;
  }
  return drw_text(drw, 0, 0, 0, 0, 0, text, 0);
}

unsigned int drw_fontset_getwidth_clamp(Drw *drw, const char *text,
                                        unsigned int n) {
  unsigned int tmp = 0;
  if (drw && drw->fonts && text && n) {
    tmp = drw_text(drw, 0, 0, 0, 0, 0, text, n);
  }
  return MIN(n, tmp);
}

void drw_font_getexts(Fnt *font, const char *text, unsigned int len,
                      unsigned int *w, unsigned int *h) {
  XGlyphInfo ext;
  if (!font || !text) {
    return;
  }
  XftTextExtentsUtf8(font->dpy, font->xfont, (XftChar8 *)text, len, &ext);
  if (w) {
    *w = ext.xOff;
  }
  if (h) {
    *h = font->h;
  }
}

Cur *drw_cur_create(Drw *drw, int shape) {
  Cur *cur;
  if (!drw || !(cur = ecalloc_type<Cur>())) {
    return NULL;
  }
  cur->cursor = XCreateFontCursor(drw->dpy, shape);
  return cur;
}

void drw_cur_free(Drw *drw, Cur *cursor) {
  if (!cursor) {
    return;
  }
  XFreeCursor(drw->dpy, cursor->cursor);
  free(cursor);
}

// }}} drw.c

// {{{ macros
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask)                                                        \
  (mask & ~(numlockmask | LockMask) &                                          \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask |      \
    Mod5Mask))
#define INTERSECT(x, y, w, h, m)                                               \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) *             \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLE(C) ((C->tags & C->mon->tagset[C->mon->seltags]))
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define TEXTW(X) (drw_fontset_getwidth(drw, (X)) + lrpad)
#define SYSTEM_TRAY_REQUEST_DOCK 0
#define XEMBED_EMBEDDED_NOTIFY 0
#define XEMBED_MAPPED (1 << 0)
// }}} macros

// {{{ enums
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel };                  /* color schemes */
enum {
  NetSupported,
  NetSystemTray,
  NetSystemTrayOP,
  NetSystemTrayOrientation,
  NetSystemTrayOrientationHorz,
  NetWMName,
  NetWMState,
  NetWMCheck,
  NetWMFullscreen,
  NetActiveWindow,
  NetWMWindowType,
  NetWMWindowTypeDialog,
  NetClientList,
  NetLast
}; /* EWMH atoms */
enum {
  WMProtocols,
  WMDelete,
  WMState,
  WMTakeFocus,
  WMLast
}; /* default atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum {
  ClkTagBar,
  ClkLtSymbol,
  ClkStatusText,
  ClkWinTitle,
  ClkWidgetBar,
  ClkClientWin,
  ClkRootWin,
  ClkLast
}; /* clicks */

// }}} enums

// {{{ Types
typedef union {
  int i;
  unsigned int ui;
  uint64_t ull;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
typedef struct Systray Systray;
typedef struct Workspace Workspace;
typedef uint64_t WorkspaceMask;

struct Workspace {
  WorkspaceMask mask;
  char *name;
};

struct Client {
  char name[256];
  float mina, maxa;
  float cfact;
  int x, y, w, h; // x,y coords and width/height
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  int bw, oldbw; // border width
  WorkspaceMask tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;  // Next client in the list
  Client *snext; // Selection linked list (for focus history)
  Monitor *mon;  // Monitor the client is
  Window win;    // X Window Id
};

struct Systray {
  Window win;
  Client *icons;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;             /* bar geometry */
  int bby;            /* widget bar geometry */
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  WorkspaceMask tagset[2];
  int showbar;
  int topbar;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  Window widgetbarwin;
  const Layout *lt[2];
};

typedef struct {
  const char *class_name;
  const char *instance;
  const char *title;
  WorkspaceMask tags;
  int isfloating;
  int monitor;
} Rule;

typedef struct {
  char fg[32];
  char bg[32];
  char border[32];
} ColorSetConfig;

typedef struct {
  ColorSetConfig normal;
  ColorSetConfig selected;
} BarThemeConfig;

enum WidgetId {
  WidgetBattery,
  WidgetBacklight,
  WidgetVolume,
  WidgetTheme,
  WidgetNotif,
  WidgetTodo,
  WidgetAgents,
  WidgetCpu,
  WidgetMem,
  WidgetDisk,
  WidgetNet,
  WidgetClock,
  WidgetCount
};

enum SliderWidget { SliderBacklight, SliderVolume };

enum ThemeMode { ThemeModeDark, ThemeModeLight, ThemeModeSystem, ThemeModeCount };

typedef struct {
  int x;
  int w;
} WidgetLayout;

#define MAX_LUA_WIDGETS 16

typedef struct {
  char name[32];
  char text[256];
  int highlight;
  int update_ref;
  int click_ref;
  int x;
  int w;
} LuaWidget;

#define MAX_NOTIFICATIONS 50

typedef struct Notification Notification;
struct Notification {
  unsigned int id;
  char app_name[64];
  char summary[256];
  char body[512];
  time_t received;
  time_t expire_at; /* 0 = never */
  Notification *next;
};

typedef struct {
  unsigned int id;
  int close_x, close_y, close_w, close_h;
} NotifRowLayout;

#define MAX_TODOS 50

typedef struct Todo Todo;
struct Todo {
  unsigned int id;
  char text[256];
  int done;
  time_t created;
  Todo *next;
};

typedef struct {
  unsigned int id;
  int check_x, check_y, check_w, check_h;
  int close_x, close_y, close_w, close_h;
} TodoRowLayout;

#define MAX_AGENTS 64

typedef struct AgentStatus AgentStatus;
struct AgentStatus {
  char kind[16];   /* "claude" / "codex" */
  char status[16]; /* "running" / "needs_input" */
  char label[128];
  char cwd[384];
  time_t updated;
  int from_file; /* backed by a status file on disk; dismissible */
  char file_path[PATH_MAX];
  AgentStatus *next;
};

typedef struct {
  unsigned int id;
  int close_x, close_y, close_w, close_h;
} AgentRowLayout;

enum SidebarAnim { SidebarAnimIdle, SidebarAnimOpening, SidebarAnimClosing };

// }}} Types

// {{{ Function declarations
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h,
                          int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void buttonrelease(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawwidgetbar(Monitor *m);
static void draw_slider_popup(void);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusdir(const Arg *arg);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static Atom getatomprop(Client *c, Atom prop);
static WorkspaceMask get_full_workspace_mask(void);
static void get_theme_config_path(char *buf, size_t size);
static void get_workspace_socket_path(char *buf, size_t size);
static unsigned int getsystraywidth(void);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static int load_config_from_lua(void);
static int load_theme_from_lua(void);
static int lua_mwm_create_workspace(lua_State *L);
static int lua_mwm_exec(lua_State *L);
static int lua_mwm_focus_workspace(lua_State *L);
static int lua_mwm_list_workspaces(lua_State *L);
static int lua_mwm_rename_workspace(lua_State *L);
static int lua_mwm_theme(lua_State *L);
static int lua_mwm_widget(lua_State *L);
static int lua_mwm_workspaces(lua_State *L);
static void reset_lua_widgets(void);
static void widget_update_lua_widgets(void);
static void widget_lua_click(size_t index, int button);

/* {{{ notification sidebar declarations */
static unsigned int notification_add(const char *app_name, const char *summary,
                                     const char *body, unsigned int replaces_id,
                                     int expire_timeout_ms);
static void notification_remove(unsigned int id, unsigned int reason);
static void notification_clear_all(void);
static void notification_expire_check(void);
static void notif_dbus_init(void);
static void notif_dbus_try_acquire(void);
static void notif_dbus_shutdown(void);
static void notif_dbus_process(void);
static void notif_dbus_handle_message(DBusMessage *msg);
static void notif_dbus_handle_notify(DBusMessage *msg);
static void notif_dbus_handle_close_notification(DBusMessage *msg);
static void notif_dbus_handle_get_capabilities(DBusMessage *msg);
static void notif_dbus_handle_get_server_information(DBusMessage *msg);
static void notif_dbus_handle_introspect(DBusMessage *msg);
static void notif_dbus_emit_notification_closed(unsigned int id, unsigned int reason);
static void widget_format_notifications(char *buf, size_t size);
static size_t notif_wrap_body(const char *text, int max_width, char out[][256],
                              size_t max_lines);
static void notif_format_relative_time(time_t t, char *buf, size_t size);
static int notif_sidebar_content_height(void);
static void notif_sidebar_position(Monitor *m);
static void draw_notif_sidebar(void);
static void notif_sidebar_open(Monitor *m);
static void notif_sidebar_close(void);
static void notif_sidebar_toggle(Monitor *m);
static void notif_sidebar_advance_animation(void);
static int notif_sidebar_handle_button(XButtonPressedEvent *ev);
/* }}} notification sidebar declarations */

/* {{{ todo sidebar declarations */
static unsigned int todo_add(const char *text);
static void todo_remove(unsigned int id);
static void todo_toggle(unsigned int id);
static void todo_clear_completed(void);
static void todo_seed_fake(void);
static void widget_format_todos(char *buf, size_t size);
static int todo_sidebar_content_height(void);
static void todo_sidebar_position(Monitor *m);
static void draw_todo_sidebar(void);
static void todo_sidebar_open(Monitor *m);
static void todo_sidebar_close(void);
static void todo_sidebar_toggle(Monitor *m);
static void todo_sidebar_advance_animation(void);
static int todo_sidebar_handle_button(XButtonPressedEvent *ev);
/* }}} todo sidebar declarations */

/* {{{ agent sidebar declarations */
static void get_agent_status_dir(char *buf, size_t size);
static void agents_refresh(void);
static void agents_free_list(void);
static void widget_format_agents(char *buf, size_t size);
static int agent_sidebar_content_height(void);
static void agent_sidebar_position(Monitor *m);
static void draw_agent_sidebar(void);
static void agent_sidebar_open(Monitor *m);
static void agent_sidebar_close(void);
static void agent_sidebar_toggle(Monitor *m);
static void agent_sidebar_advance_animation(void);
static int agent_sidebar_handle_button(XButtonPressedEvent *ev);
/* }}} agent sidebar declarations */

static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *c);
static void promptcommand(const Arg *arg);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static int read_long_from_file(const char *path, long *value);
static void removesystrayicon(Client *i);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebydir(const Arg *arg);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void reloadconfig(const Arg *arg);
static void restartwm(const Arg *arg);
static void restack(Monitor *m);
static void run(void);
static void scan(void);
static int sendevent(Client *c, Atom proto);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setcolorscheme(void);
static void set_default_bar_theme(void);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static unsigned int tiledcount(Monitor *m);
static void tile(Monitor *m);
static void togglebar(const Arg *arg);
static void togglefloating(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void toggletagnth(const Arg *arg);
static void toggleviewnth(const Arg *arg);
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatewidgets(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void widget_close_slider_popup(void);
static void widget_init_paths(void);
static void widget_format_backlight(char *buf, size_t size);
static void widget_format_battery(char *buf, size_t size);
static void widget_format_clock(char *buf, size_t size);
static void widget_format_cpu(char *buf, size_t size);
static void widget_format_disk(char *buf, size_t size);
static void widget_format_mem(char *buf, size_t size);
static void widget_format_net(char *buf, size_t size);
static void widget_format_volume(char *buf, size_t size);
static int widget_handle_button(Monitor *m, XButtonPressedEvent *ev);
static int widget_hit(enum WidgetId id, int x);
static void widget_open_slider_popup(Monitor *m, enum SliderWidget kind);
static void widget_position_slider_popup(Monitor *m);
static int slider_popup_handle_button(XButtonPressedEvent *ev);
static int slider_popup_handle_motion(XMotionEvent *ev);
static int widget_slider_current_percent(enum SliderWidget kind);
static void widget_slider_label(enum SliderWidget kind, char *buf, size_t size);
static void widget_slider_set_absolute(enum SliderWidget kind, int percent);
static void widget_set_backlight_percent_absolute(int percent);
static int widget_set_backlight_percent(int delta_percent);
static void widget_set_volume_percent_absolute(int percent);
static int widget_set_volume_percent(int delta_percent);
static void widget_toggle_volume_mute(void);
static void widget_update_cpu(void);
static void widget_update_disk(void);
static void widget_update_mem(void);
static void widget_update_net(void);
static void widget_update_volume(void);
static void widget_update_theme(void);
static void widget_format_theme(char *buf, size_t size);
static void widget_cycle_theme_mode(int direction);
static const BarThemeConfig *active_bar_theme(void);
static int theme_resolve_prefers_dark(void);
static int system_prefers_dark(void);
static void widget_apply_theme(void);
static void widget_refresh_client_borders(void);
static void get_theme_state_path(char *buf, size_t size);
static void load_theme_mode_state(void);
static void save_theme_mode_state(void);
static void view(const Arg *arg);
static void viewnth(const Arg *arg);
static void tagnth(const Arg *arg);
static Client *wintoclient(Window w);
static Client *wintosystrayicon(Window w);
static Workspace *workspace_by_index(size_t index);
static size_t workspace_count_visible(void);
static size_t workspace_ensure(const char *name);
static size_t workspace_find(const char *name);
static int workspace_has_clients(size_t index);
static size_t workspace_index_from_mask(WorkspaceMask mask);
static int workspace_is_visible_on_any_monitor(size_t index);
static WorkspaceMask workspace_mask_from_index(size_t index);
static WorkspaceMask workspace_reindex_mask_after_remove(WorkspaceMask mask,
                                                         size_t removed_index);
static int workspace_remove_index(size_t index);
static const char *workspace_name_from_mask(WorkspaceMask mask);
static int workspace_rename(size_t index, const char *name);
static void workspace_set_defaults(void);
static int workspace_set_list_from_lua(lua_State *L, int index);
static void workspace_sync_after_registry_change(void);
static void workspace_view_mask(Monitor *m, WorkspaceMask mask);
static int wm_command_handle_client(int fd);
static int wm_command_server_init(void);
static void wm_command_server_shutdown(void);
static int wm_eval_lua_buffer(const char *chunk, size_t len, int fd);
static void client_save_workspace_tags(const Client *c);
static int client_restore_workspace_tags(Window w, WorkspaceMask *tags);
static std::array<void (*)(XEvent *), LASTEvent> make_handler_table(void);

static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

// }}} Function declarations

// {{{ Variables
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh; /* X display screen geometry width, height */
static int bh;     /* bar height */
static int lrpad;  /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static const std::array<void (*)(XEvent *), LASTEvent> handler =
    make_handler_table();
static Atom wmatom[WMLast], netatom[NetLast];
static Atom xatom[XLast];
static int running = 1;
static int restart_requested = 0;
static Cur *cursor[CurLast];
static Clr **scheme;
static lua_State *command_lua;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Systray *systray;
static BarThemeConfig bar_theme_dark;
static BarThemeConfig bar_theme_light;
static enum ThemeMode theme_mode = ThemeModeSystem;
static int theme_resolved_dark = 1;
static LuaWidget lua_widgets[MAX_LUA_WIDGETS];
static size_t lua_widgets_len = 0;

/* {{{ notification sidebar globals */
static Notification *notifications = NULL;
static unsigned int notification_next_id = 1;
static size_t notification_count = 0;
static size_t notification_unread = 0;

static DBusConnection *notif_dbus_conn = NULL;
static int notif_dbus_owned = 0;

static Window notif_sidebar_win = None;
static Monitor *notif_sidebar_mon = NULL;
static int notif_sidebar_visible = 0;
static enum SidebarAnim notif_anim_state = SidebarAnimIdle;
static struct timeval notif_anim_start;
static int notif_anim_from_x = 0;
static int notif_anim_to_x = 0;
static int notif_sidebar_cur_x = 0;
static int notif_scroll_offset = 0;
static const int notif_sidebar_w = 360;
static const int notif_anim_duration_ms = 220;
static const int notif_row_pad = 12;

static NotifRowLayout notif_row_layout[MAX_NOTIFICATIONS];
static size_t notif_row_layout_len = 0;
static int notif_clear_x = 0, notif_clear_y = 0, notif_clear_w = 0, notif_clear_h = 0;
/* }}} notification sidebar globals */

/* {{{ todo sidebar globals */
static Todo *todos = NULL;
static unsigned int todo_next_id = 1;
static size_t todo_count = 0;
static size_t todo_incomplete = 0;

static Window todo_sidebar_win = None;
static Monitor *todo_sidebar_mon = NULL;
static int todo_sidebar_visible = 0;
static enum SidebarAnim todo_anim_state = SidebarAnimIdle;
static struct timeval todo_anim_start;
static int todo_anim_from_x = 0;
static int todo_anim_to_x = 0;
static int todo_sidebar_cur_x = 0;
static int todo_scroll_offset = 0;
static const int todo_sidebar_w = 360;
static const int todo_anim_duration_ms = 220;
static const int todo_row_pad = 12;

static TodoRowLayout todo_row_layout[MAX_TODOS];
static size_t todo_row_layout_len = 0;
static int todo_clear_x = 0, todo_clear_y = 0, todo_clear_w = 0, todo_clear_h = 0;
/* }}} todo sidebar globals */

/* {{{ agent sidebar globals */
static AgentStatus *agents = NULL;
static size_t agent_count = 0;
static size_t agent_needs_input = 0;
static time_t agents_last_refresh = 0;
static char agent_status_dir[PATH_MAX];

static Window agent_sidebar_win = None;
static Monitor *agent_sidebar_mon = NULL;
static int agent_sidebar_visible = 0;
static enum SidebarAnim agent_anim_state = SidebarAnimIdle;
static struct timeval agent_anim_start;
static int agent_anim_from_x = 0;
static int agent_anim_to_x = 0;
static int agent_sidebar_cur_x = 0;
static int agent_scroll_offset = 0;
static const int agent_sidebar_w = 360;
static const int agent_anim_duration_ms = 220;
static const int agent_row_pad = 12;

static AgentRowLayout agent_row_layout[MAX_AGENTS];
static size_t agent_row_layout_len = 0;
static int agent_clear_x = 0, agent_clear_y = 0, agent_clear_w = 0, agent_clear_h = 0;
/* }}} agent sidebar globals */

static Workspace *workspaces;
static size_t workspaces_len;
static Window root, wmcheckwin;
static int command_prompt_active = 0;
static char command_prompt[1024];
static size_t command_prompt_len;
static char command_prompt_feedback[1024];
static int wm_command_fd = -1;
static int wm_command_reply_fd = -1;
static char *wm_capture_buffer = NULL;
static size_t wm_capture_buffer_size = 0;
static size_t wm_capture_buffer_len = 0;
static char wm_socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
static char **saved_argv;
static char backlight_dir[PATH_MAX];
static char backlight_brightness_path[PATH_MAX];
static char backlight_max_path[PATH_MAX];
static char battery_capacity_path[PATH_MAX];
static char battery_status_path[PATH_MAX];
static int backlight_percent = -1;
static int battery_percent = -1;
static int battery_charging = 0;
static int volume_percent = -1;
static int volume_muted = 0;
static long cpu_prev_total = -1;
static long cpu_prev_idle = -1;
static int cpu_percent = -1;
static int mem_percent = -1;
static int disk_percent = -1;
static char net_iface[64];
static int net_up = 0;
static time_t widgets_last_refresh = 0;
static WidgetLayout widget_layout[WidgetCount];
static const enum WidgetId widget_draw_order[] = {
    WidgetBattery, WidgetBacklight, WidgetVolume,  WidgetTheme, WidgetNotif,
    WidgetTodo,    WidgetAgents,    WidgetCpu,     WidgetMem,   WidgetDisk,
    WidgetNet,     WidgetClock,
};
static Window slider_popup_win = None;
static Monitor *slider_popup_mon = NULL;
static int slider_popup_visible = 0;
static int slider_popup_dragging = 0;
static enum SliderWidget slider_popup_kind = SliderBacklight;
static const int slider_popup_w = 240;
static const int slider_popup_h = 64;
static const int slider_popup_pad = 12;
static const int slider_popup_slider_h = 8;
static const int slider_popup_slider_y = 34;
static const int slider_popup_knob_w = 12;
static const int slider_popup_knob_h = 20;

static std::array<void (*)(XEvent *), LASTEvent> make_handler_table(void) {
  std::array<void (*)(XEvent *), LASTEvent> handlers = {};
  handlers[ButtonPress] = buttonpress;
  handlers[ButtonRelease] = buttonrelease;
  handlers[ClientMessage] = clientmessage;
  handlers[ConfigureRequest] = configurerequest;
  handlers[ConfigureNotify] = configurenotify;
  handlers[DestroyNotify] = destroynotify;
  handlers[EnterNotify] = enternotify;
  handlers[Expose] = expose;
  handlers[FocusIn] = focusin;
  handlers[KeyPress] = keypress;
  handlers[MappingNotify] = mappingnotify;
  handlers[MapRequest] = maprequest;
  handlers[MotionNotify] = motionnotify;
  handlers[PropertyNotify] = propertynotify;
  handlers[UnmapNotify] = unmapnotify;
  return handlers;
}

static int read_long_from_file(const char *path, long *value) {
  FILE *fp;
  long parsed;

  if (!path || path[0] == '\0' || !value) {
    return 0;
  }
  fp = fopen(path, "r");
  if (!fp) {
    return 0;
  }
  if (fscanf(fp, "%ld", &parsed) != 1) {
    fclose(fp);
    return 0;
  }
  fclose(fp);
  *value = parsed;
  return 1;
}

static void widget_init_paths(void) {
  DIR *dir;
  struct dirent *entry;

  if (backlight_brightness_path[0] == '\0') {
    dir = opendir("/sys/class/backlight");
    if (dir) {
      while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') {
          continue;
        }
        snprintf(backlight_dir, sizeof(backlight_dir), "/sys/class/backlight/%s",
                 entry->d_name);
        snprintf(backlight_brightness_path, sizeof(backlight_brightness_path),
                 "%s/brightness", backlight_dir);
        snprintf(backlight_max_path, sizeof(backlight_max_path), "%s/max_brightness",
                 backlight_dir);
        break;
      }
      closedir(dir);
    }
  }

  if (battery_capacity_path[0] == '\0') {
    dir = opendir("/sys/class/power_supply");
    if (dir) {
      while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || strncmp(entry->d_name, "BAT", 3) != 0) {
          continue;
        }
        snprintf(battery_capacity_path, sizeof(battery_capacity_path),
                 "/sys/class/power_supply/%s/capacity", entry->d_name);
        snprintf(battery_status_path, sizeof(battery_status_path),
                 "/sys/class/power_supply/%s/status", entry->d_name);
        break;
      }
      closedir(dir);
    }
  }
}

static void updatewidgets(void) {
  FILE *fp;
  char status[64];
  long value;
  long max_value;

  widget_init_paths();

  backlight_percent = -1;
  if (read_long_from_file(backlight_brightness_path, &value) &&
      read_long_from_file(backlight_max_path, &max_value) && max_value > 0) {
    backlight_percent = (int)((value * 100 + (max_value / 2)) / max_value);
  }

  battery_percent = -1;
  battery_charging = 0;
  if (read_long_from_file(battery_capacity_path, &value)) {
    battery_percent = (int)value;
  }
  if (battery_status_path[0] != '\0') {
    fp = fopen(battery_status_path, "r");
    if (fp) {
      if (fgets(status, sizeof(status), fp)) {
        battery_charging =
            strncmp(status, "Charging", 8) == 0 || strncmp(status, "Full", 4) == 0;
      }
      fclose(fp);
    }
  }

  widget_update_cpu();
  widget_update_mem();
  widget_update_disk();
  widget_update_net();
  widget_update_volume();
  widget_update_theme();
  widget_update_lua_widgets();
}

static void widget_update_cpu(void) {
  FILE *fp;
  char line[256];
  long user = 0, nice_val = 0, sys_val = 0, idle = 0, iowait = 0, irq = 0,
       softirq = 0, steal = 0;
  long total, idle_all, delta_total, delta_idle;

  cpu_percent = -1;
  fp = fopen("/proc/stat", "r");
  if (!fp) {
    return;
  }
  if (!fgets(line, sizeof(line), fp)) {
    fclose(fp);
    return;
  }
  fclose(fp);

  if (sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld %ld", &user, &nice_val,
             &sys_val, &idle, &iowait, &irq, &softirq, &steal) < 4) {
    return;
  }

  idle_all = idle + iowait;
  total = user + nice_val + sys_val + idle + iowait + irq + softirq + steal;

  if (cpu_prev_total >= 0) {
    delta_total = total - cpu_prev_total;
    delta_idle = idle_all - cpu_prev_idle;
    if (delta_total > 0) {
      cpu_percent = (int)(100 * (delta_total - delta_idle) / delta_total);
      cpu_percent = MAX(0, MIN(100, cpu_percent));
    }
  }
  cpu_prev_total = total;
  cpu_prev_idle = idle_all;
}

static void widget_update_mem(void) {
  FILE *fp;
  char line[256];
  long mem_total = -1;
  long mem_avail = -1;

  mem_percent = -1;
  fp = fopen("/proc/meminfo", "r");
  if (!fp) {
    return;
  }
  while (fgets(line, sizeof(line), fp)) {
    if (mem_total < 0 && sscanf(line, "MemTotal: %ld", &mem_total) == 1) {
      continue;
    }
    if (mem_avail < 0 && sscanf(line, "MemAvailable: %ld", &mem_avail) == 1) {
      continue;
    }
    if (mem_total >= 0 && mem_avail >= 0) {
      break;
    }
  }
  fclose(fp);

  if (mem_total > 0 && mem_avail >= 0) {
    mem_percent = (int)(100 * (mem_total - mem_avail) / mem_total);
  }
}

static void widget_update_disk(void) {
  struct statvfs vfs;
  unsigned long long used;
  unsigned long long avail_for_calc;

  disk_percent = -1;
  if (statvfs("/", &vfs) != 0 || vfs.f_blocks == 0) {
    return;
  }
  used = (unsigned long long)(vfs.f_blocks - vfs.f_bfree);
  avail_for_calc = used + vfs.f_bavail;
  if (avail_for_calc == 0) {
    return;
  }
  disk_percent = (int)(used * 100 / avail_for_calc);
}

static void widget_update_net(void) {
  FILE *fp;
  char line[256];
  char iface[64] = "";
  char state[32] = "";
  char path[128];

  net_iface[0] = '\0';
  net_up = 0;

  fp = fopen("/proc/net/route", "r");
  if (!fp) {
    return;
  }
  if (fgets(line, sizeof(line), fp)) {
    /* discard header line */
  }
  while (fgets(line, sizeof(line), fp)) {
    char ifname[64];
    unsigned long dest;
    if (sscanf(line, "%63s %lx", ifname, &dest) == 2 && dest == 0) {
      snprintf(iface, sizeof(iface), "%s", ifname);
      break;
    }
  }
  fclose(fp);

  if (iface[0] == '\0') {
    return;
  }

  snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", iface);
  fp = fopen(path, "r");
  if (fp) {
    if (fgets(state, sizeof(state), fp)) {
      state[strcspn(state, "\n")] = '\0';
    }
    fclose(fp);
  }

  snprintf(net_iface, sizeof(net_iface), "%s", iface);
  net_up = (strcmp(state, "up") == 0);
}

static void widget_update_volume(void) {
  FILE *fp;
  char buf[1024];
  size_t len;
  char *pct_end;
  char *digits_start;
  int pct;

  volume_percent = -1;
  volume_muted = 0;

  fp = popen("amixer get Master 2>/dev/null", "r");
  if (!fp) {
    return;
  }
  len = fread(buf, 1, sizeof(buf) - 1, fp);
  pclose(fp);
  buf[len] = '\0';

  pct_end = strstr(buf, "%]");
  if (!pct_end) {
    return;
  }
  digits_start = pct_end;
  while (digits_start > buf && *(digits_start - 1) != '[') {
    digits_start--;
  }
  if (sscanf(digits_start, "%d", &pct) == 1) {
    volume_percent = MAX(0, MIN(100, pct));
    volume_muted = strstr(buf, "[off]") != NULL;
  }
}

/* {{{ widget icons (Nerd Font Private Use Area glyphs; see `fonts[]`) */
static const char icon_battery_full[] = "";
static const char icon_battery_75[] = "";
static const char icon_battery_50[] = "";
static const char icon_battery_25[] = "";
static const char icon_battery_empty[] = "";
static const char icon_battery_charging[] = "";
static const char icon_backlight[] = "\U000F0599";
static const char icon_volume_muted[] = "";
static const char icon_volume_low[] = "";
static const char icon_volume_high[] = "";
static const char icon_theme_dark[] = "";
static const char icon_theme_light[] = "\U000F0599";
static const char icon_theme_auto[] = "";
static const char icon_bell[] = "";
static const char icon_bell_outline[] = "";
static const char icon_cpu[] = "\U000F061A";
static const char icon_mem[] = "\U000F035B";
static const char icon_disk[] = "";
static const char icon_net[] = "";
static const char icon_clock[] = "";
static const char icon_close[] = "";
static const char icon_todo[] = "\U0000F0AE";
static const char icon_checkbox_unchecked[] = "\U0000F096";
static const char icon_checkbox_checked[] = "\U0000F046";
static const char icon_agents[] = "\U0000F120";
/* }}} widget icons */

static void widget_format_backlight(char *buf, size_t size) {
  if (backlight_percent < 0) {
    snprintf(buf, size, "%s n/a", icon_backlight);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon_backlight, backlight_percent);
}

static void widget_format_battery(char *buf, size_t size) {
  const char *icon;

  if (battery_percent < 0) {
    snprintf(buf, size, "%s n/a", icon_battery_full);
    return;
  }
  if (battery_charging) {
    icon = icon_battery_charging;
  } else if (battery_percent >= 90) {
    icon = icon_battery_full;
  } else if (battery_percent >= 65) {
    icon = icon_battery_75;
  } else if (battery_percent >= 40) {
    icon = icon_battery_50;
  } else if (battery_percent >= 15) {
    icon = icon_battery_25;
  } else {
    icon = icon_battery_empty;
  }
  snprintf(buf, size, "%s %d%%", icon, battery_percent);
}

static void widget_format_volume(char *buf, size_t size) {
  const char *icon;

  if (volume_percent < 0) {
    snprintf(buf, size, "%s n/a", icon_volume_high);
    return;
  }
  if (volume_muted || volume_percent == 0) {
    icon = icon_volume_muted;
  } else if (volume_percent < 50) {
    icon = icon_volume_low;
  } else {
    icon = icon_volume_high;
  }
  if (volume_muted) {
    snprintf(buf, size, "%s %d%% muted", icon, volume_percent);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon, volume_percent);
}

static void widget_format_cpu(char *buf, size_t size) {
  if (cpu_percent < 0) {
    snprintf(buf, size, "%s ...", icon_cpu);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon_cpu, cpu_percent);
}

static void widget_format_mem(char *buf, size_t size) {
  if (mem_percent < 0) {
    snprintf(buf, size, "%s n/a", icon_mem);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon_mem, mem_percent);
}

static void widget_format_disk(char *buf, size_t size) {
  if (disk_percent < 0) {
    snprintf(buf, size, "%s n/a", icon_disk);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon_disk, disk_percent);
}

static void widget_format_net(char *buf, size_t size) {
  if (net_iface[0] == '\0') {
    snprintf(buf, size, "%s down", icon_net);
    return;
  }
  snprintf(buf, size, "%s %s %s", icon_net, net_iface, net_up ? "up" : "down");
}

static void widget_format_clock(char *buf, size_t size) {
  time_t now = time(NULL);
  struct tm tm_buf;
  char timebuf[48];

  localtime_r(&now, &tm_buf);
  strftime(timebuf, sizeof(timebuf), "%a %Y-%m-%d %H:%M", &tm_buf);
  snprintf(buf, size, "%s %s", icon_clock, timebuf);
}

static void widget_set_backlight_percent_absolute(int percent) {
  FILE *fp;
  long max_brightness;
  long next_brightness;

  widget_init_paths();
  if (!read_long_from_file(backlight_max_path, &max_brightness) ||
      max_brightness <= 0 || backlight_brightness_path[0] == '\0') {
    return;
  }

  if (percent < 1) {
    percent = 1;
  }
  if (percent > 100) {
    percent = 100;
  }

  next_brightness = (max_brightness * percent + 50) / 100;
  if (next_brightness < 1) {
    next_brightness = 1;
  }
  if (next_brightness > max_brightness) {
    next_brightness = max_brightness;
  }

  fp = fopen(backlight_brightness_path, "w");
  if (!fp) {
    return;
  }
  fprintf(fp, "%ld\n", next_brightness);
  fclose(fp);
  updatewidgets();
  drawbars();
  draw_slider_popup();
}

static int widget_set_backlight_percent(int delta_percent) {
  FILE *fp;
  long brightness;
  long max_brightness;
  long next_brightness;

  widget_init_paths();
  if (!read_long_from_file(backlight_brightness_path, &brightness) ||
      !read_long_from_file(backlight_max_path, &max_brightness) ||
      max_brightness <= 0 || backlight_brightness_path[0] == '\0') {
    return 0;
  }

  next_brightness = brightness + (max_brightness * delta_percent) / 100;
  if (next_brightness < 1) {
    next_brightness = 1;
  }
  if (next_brightness > max_brightness) {
    next_brightness = max_brightness;
  }

  fp = fopen(backlight_brightness_path, "w");
  if (!fp) {
    return 0;
  }
  fprintf(fp, "%ld\n", next_brightness);
  fclose(fp);
  updatewidgets();
  drawbars();
  draw_slider_popup();
  return 1;
}

static void widget_set_volume_percent_absolute(int percent) {
  char cmd[128];

  percent = MAX(0, MIN(100, percent));
  snprintf(cmd, sizeof(cmd), "amixer -q set Master %d%% unmute", percent);
  /* setup() ignores SIGCHLD with SA_NOCLDWAIT so spawned clients never
   * zombie; that also makes system()'s internal wait() fail with ECHILD
   * even when the command ran fine, so its return value can't be trusted
   * here. */
  system(cmd);
  updatewidgets();
  drawbars();
  draw_slider_popup();
}

static int widget_set_volume_percent(int delta_percent) {
  if (volume_percent < 0) {
    return 0;
  }
  widget_set_volume_percent_absolute(volume_percent + delta_percent);
  return 1;
}

static void widget_toggle_volume_mute(void) {
  system("amixer -q set Master toggle");
  updatewidgets();
  drawbars();
  if (slider_popup_visible && slider_popup_kind == SliderVolume) {
    draw_slider_popup();
  }
}

static int widget_slider_current_percent(enum SliderWidget kind) {
  return kind == SliderVolume ? volume_percent : backlight_percent;
}

static void widget_slider_set_absolute(enum SliderWidget kind, int percent) {
  if (kind == SliderVolume) {
    widget_set_volume_percent_absolute(percent);
  } else {
    widget_set_backlight_percent_absolute(percent);
  }
}

static void widget_slider_label(enum SliderWidget kind, char *buf, size_t size) {
  if (kind == SliderVolume) {
    widget_format_volume(buf, size);
  } else {
    widget_format_backlight(buf, size);
  }
}

static void widget_position_slider_popup(Monitor *m) {
  enum WidgetId id = (slider_popup_kind == SliderVolume) ? WidgetVolume : WidgetBacklight;
  int anchor_x;
  int anchor_w;
  int popup_x;
  int popup_y;

  if (!m || slider_popup_win == None) {
    return;
  }

  anchor_x = widget_layout[id].x;
  anchor_w = widget_layout[id].w;

  popup_x = m->wx + anchor_x + (anchor_w / 2) - (slider_popup_w / 2);
  if (popup_x < m->wx) {
    popup_x = m->wx;
  }
  if (popup_x + slider_popup_w > m->wx + m->ww) {
    popup_x = m->wx + m->ww - slider_popup_w;
  }

  popup_y = m->bby - slider_popup_h - 8;
  if (popup_y < m->my) {
    popup_y = m->my;
  }

  XMoveResizeWindow(dpy, slider_popup_win, popup_x, popup_y, slider_popup_w,
                    slider_popup_h);
}

static void widget_open_slider_popup(Monitor *m, enum SliderWidget kind) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }

  if (slider_popup_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                    PointerMotionMask;
    slider_popup_win = XCreateWindow(
        dpy, root, m->wx, m->bby - slider_popup_h, slider_popup_w,
        slider_popup_h, 1, DefaultDepth(dpy, screen), CopyFromParent,
        DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, slider_popup_win, cursor[CurNormal]->cursor);
  }

  slider_popup_kind = kind;
  slider_popup_mon = m;
  slider_popup_visible = 1;
  slider_popup_dragging = 0;
  widget_position_slider_popup(m);
  XMapRaised(dpy, slider_popup_win);
  draw_slider_popup();
}

static void widget_close_slider_popup(void) {
  slider_popup_dragging = 0;
  slider_popup_visible = 0;
  slider_popup_mon = NULL;
  if (slider_popup_win != None) {
    XUnmapWindow(dpy, slider_popup_win);
  }
}

static void draw_slider_popup(void) {
  char label[64];
  int slider_x;
  int slider_w;
  int fill_w;
  int knob_x;
  int knob_y;
  int percent;

  if (!slider_popup_visible || slider_popup_win == None) {
    return;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, slider_popup_w, slider_popup_h, 1, 1);

  widget_slider_label(slider_popup_kind, label, sizeof(label));
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, slider_popup_pad, 8, slider_popup_w - 2 * slider_popup_pad,
           drw->fonts->h + 4, 0, label, 0);

  slider_x = slider_popup_pad;
  slider_w = slider_popup_w - 2 * slider_popup_pad;
  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, slider_x, slider_popup_slider_y, slider_w,
           slider_popup_slider_h, 1, 0);

  percent = widget_slider_current_percent(slider_popup_kind);
  if (percent >= 0) {
    fill_w = (slider_w * percent) / 100;
    if (fill_w < (slider_popup_knob_w / 2)) {
      fill_w = slider_popup_knob_w / 2;
    }
    if (fill_w > slider_w) {
      fill_w = slider_w;
    }
    drw_setscheme(drw, scheme[SchemeSel]);
    drw_rect(drw, slider_x, slider_popup_slider_y, fill_w,
             slider_popup_slider_h, 1, 1);

    knob_x = slider_x + fill_w - (slider_popup_knob_w / 2);
    if (knob_x < slider_x) {
      knob_x = slider_x;
    }
    if (knob_x > slider_x + slider_w - slider_popup_knob_w) {
      knob_x = slider_x + slider_w - slider_popup_knob_w;
    }
    knob_y = slider_popup_slider_y + (slider_popup_slider_h / 2) -
             (slider_popup_knob_h / 2);
    /* Knob must stay legible regardless of fill percentage, so it can't
     * reuse ColFg like the track/fill above -- those are near-identical
     * shades in some themes and the knob would vanish into a full bar. */
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_rect(drw, knob_x, knob_y, slider_popup_knob_w, slider_popup_knob_h,
             1, 1);
    drw_setscheme(drw, scheme[SchemeSel]);
    drw_rect(drw, knob_x, knob_y, slider_popup_knob_w, slider_popup_knob_h,
             0, 1);
  }

  drw_map(drw, slider_popup_win, 0, 0, slider_popup_w, slider_popup_h);
}

static int slider_popup_handle_motion(XMotionEvent *ev) {
  int slider_x;
  int slider_w;
  int relative_x;
  int percent;

  if (!slider_popup_visible || !slider_popup_dragging ||
      ev->window != slider_popup_win) {
    return 0;
  }

  slider_x = slider_popup_pad;
  slider_w = slider_popup_w - 2 * slider_popup_pad;
  relative_x = ev->x - slider_x;
  if (relative_x < 0) {
    relative_x = 0;
  }
  if (relative_x > slider_w) {
    relative_x = slider_w;
  }
  percent = (relative_x * 100) / slider_w;
  widget_slider_set_absolute(slider_popup_kind, percent);
  return 1;
}

static int slider_popup_handle_button(XButtonPressedEvent *ev) {
  int slider_x;
  int slider_w;
  int slider_y;
  int relative_x;
  int percent;

  if (!slider_popup_visible) {
    return 0;
  }

  if (ev->window != slider_popup_win) {
    widget_close_slider_popup();
    return 0;
  }

  if (ev->button != Button1) {
    return 1;
  }

  slider_x = slider_popup_pad;
  slider_w = slider_popup_w - 2 * slider_popup_pad;
  slider_y = slider_popup_slider_y -
             ((slider_popup_knob_h - slider_popup_slider_h) / 2);
  if (ev->y < slider_y || ev->y > slider_y + slider_popup_knob_h) {
    return 1;
  }
  relative_x = ev->x - slider_x;
  if (relative_x < 0) {
    relative_x = 0;
  }
  if (relative_x > slider_w) {
    relative_x = slider_w;
  }
  percent = (relative_x * 100) / slider_w;
  slider_popup_dragging = 1;
  widget_slider_set_absolute(slider_popup_kind, percent);
  return 1;
}

static int widget_hit(enum WidgetId id, int x) {
  return x >= widget_layout[id].x && x < widget_layout[id].x + widget_layout[id].w;
}

static int widget_handle_button(Monitor *m, XButtonPressedEvent *ev) {
  if (!m || ev->window != m->widgetbarwin) {
    return 0;
  }

  if (widget_hit(WidgetBacklight, ev->x)) {
    if (ev->button == Button1) {
      if (slider_popup_visible && slider_popup_kind == SliderBacklight &&
          slider_popup_mon == m) {
        widget_close_slider_popup();
      } else {
        widget_open_slider_popup(m, SliderBacklight);
      }
      return 1;
    }
    if (ev->button == Button4) {
      return widget_set_backlight_percent(+5);
    }
    if (ev->button == Button3 || ev->button == Button5) {
      return widget_set_backlight_percent(-5);
    }
    return 1;
  }

  if (widget_hit(WidgetVolume, ev->x)) {
    if (ev->button == Button1) {
      if (slider_popup_visible && slider_popup_kind == SliderVolume &&
          slider_popup_mon == m) {
        widget_close_slider_popup();
      } else {
        widget_open_slider_popup(m, SliderVolume);
      }
      return 1;
    }
    if (ev->button == Button2) {
      widget_toggle_volume_mute();
      return 1;
    }
    if (ev->button == Button4) {
      return widget_set_volume_percent(+5);
    }
    if (ev->button == Button3 || ev->button == Button5) {
      return widget_set_volume_percent(-5);
    }
    return 1;
  }

  if (widget_hit(WidgetTheme, ev->x)) {
    if (ev->button == Button1 || ev->button == Button4) {
      widget_cycle_theme_mode(+1);
      return 1;
    }
    if (ev->button == Button3 || ev->button == Button5) {
      widget_cycle_theme_mode(-1);
      return 1;
    }
    return 1;
  }

  if (widget_hit(WidgetNotif, ev->x)) {
    if (ev->button == Button1) {
      notif_sidebar_toggle(m);
    }
    return 1;
  }

  if (widget_hit(WidgetTodo, ev->x)) {
    if (ev->button == Button1) {
      todo_sidebar_toggle(m);
    }
    return 1;
  }

  if (widget_hit(WidgetAgents, ev->x)) {
    if (ev->button == Button1) {
      agent_sidebar_toggle(m);
    }
    return 1;
  }

  {
    size_t i;
    for (i = 0; i < lua_widgets_len; i++) {
      if (ev->x >= lua_widgets[i].x &&
          ev->x < lua_widgets[i].x + lua_widgets[i].w) {
        widget_lua_click(i, ev->button);
        return 1;
      }
    }
  }

  return 1;
}

static void client_save_workspace_tags(const Client *c) {
  Atom property;
  unsigned long data[2];

  if (!c) {
    return;
  }
  property = XInternAtom(dpy, "_MWM_WORKSPACE_TAGS", False);
  data[0] = (unsigned long)(c->tags & 0xFFFFFFFFULL);
  data[1] = (unsigned long)((c->tags >> 32) & 0xFFFFFFFFULL);
  XChangeProperty(dpy, c->win, property, XA_CARDINAL, 32, PropModeReplace,
                  reinterpret_cast<unsigned char *>(data), 2);
}

static int client_restore_workspace_tags(Window w, WorkspaceMask *tags) {
  Atom property;
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char *data = NULL;
  WorkspaceMask restored = 0;
  int ok = 0;

  if (!tags) {
    return 0;
  }

  property = XInternAtom(dpy, "_MWM_WORKSPACE_TAGS", False);
  if (XGetWindowProperty(dpy, w, property, 0, 2, False, XA_CARDINAL,
                         &actual_type, &actual_format, &nitems, &bytes_after,
                         &data) != Success) {
    return 0;
  }

  if (actual_type == XA_CARDINAL && actual_format == 32 && data) {
    unsigned long *values = reinterpret_cast<unsigned long *>(data);
    if (nitems >= 1) {
      restored = (WorkspaceMask)values[0];
      if (nitems >= 2) {
        restored |= ((WorkspaceMask)values[1]) << 32;
      }
      restored &= get_full_workspace_mask();
      if (restored) {
        *tags = restored;
        ok = 1;
      }
    }
  }

  if (data) {
    XFree(data);
  }
  return ok;
}

// }}} Variables

// {{{ config

/* configuration, allows nested code to access above variables */

/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx = 1; /* border pixel of windows */
static const unsigned int snap = 32;    /* snap pixel */
static const int showbar = 1;           /* 0 means no bar */
static const int showsystray = 1;       /* 0 means no system tray */
static const unsigned int systrayspacing = 6;
static const int topbar = 1;            /* 0 means bottom bar */
/* Second entry is a fallback used only for glyphs missing from the primary
 * font -- widget icons live in the Nerd Font Private Use Area ranges, so a
 * Nerd Font must be installed for them to render instead of tofu boxes. */
static const char *fonts[] = {"monospace:size=10", "UbuntuMono Nerd Font Mono:size=10"};
static const char dmenufont[] = "monospace:size=10";
static const char default_col_gray1[] = "#222222";
static const char default_col_gray2[] = "#444444";
static const char default_col_gray3[] = "#bbbbbb";
static const char default_col_gray4[] = "#eeeeee";
static const char default_col_cyan[] = "#005577";
static const char default_col_light_bg[] = "#f4f5f7";
static const char default_col_light_fg[] = "#24292f";
static const char default_col_light_border[] = "#d0d5dd";
static const char default_col_light_accent[] = "#2d6cdf";
static const char default_col_light_accent_fg[] = "#ffffff";

static const Rule rules[] = {
    /*  xprop(1):
     *  WM_CLASS(STRING) = instance, class
     *  WM_NAME(STRING) = title
     */
    /* class      instance    title       tags mask     isfloating   monitor */
    {"Gimp", NULL, NULL, 0, 1, -1},
    {"Firefox", NULL, NULL, 1ULL << 8, 0, -1},
};

/* layout(s) */
static const float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster = 1;    /* number of clients in master area */
static const int resizehints =
    1; /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen =
    1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
    /* symbol     arrange function */
    {"[]=", tile}, /* first entry is default */
    {"><>", NULL}, /* no layout function means floating behavior */
    {"[M]", monocle},
};

/* key definitions */
#define MODKEY Mod4Mask
#define WORKSPACEKEYS(KEY, INDEX)                                              \
  {MODKEY, KEY, viewnth, {.i = INDEX}},                                        \
      {MODKEY | ControlMask, KEY, toggleviewnth, {.i = INDEX}},                \
      {MODKEY | ShiftMask, KEY, tagnth, {.i = INDEX}},                         \
      {MODKEY | ControlMask | ShiftMask, KEY, toggletagnth, {.i = INDEX}},

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd)                                                             \
  {                                                                            \
    .v = (const char *[]) { "/bin/sh", "-c", cmd, NULL }                       \
  }

enum { DIR_LEFT, DIR_DOWN, DIR_UP, DIR_RIGHT };

/* commands */
static char dmenumon[2] =
    "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = {
    "dmenu_run", "-m",      dmenumon, "-fn",    dmenufont, "-nb",
    default_col_gray1, "-nf", default_col_gray3, "-sb", default_col_cyan,
    "-sf",       default_col_gray4, NULL};
static const char *termcmd[] = {"kitty", NULL};

static const char *roficmd[] = {"rofi", "-show", "drun"};

static const Key keys[] = {
    /* modifier                     key        function        argument */
    {MODKEY, XK_p, spawn, {.v = roficmd}},
    {MODKEY, XK_Return, spawn, {.v = termcmd}},
    {MODKEY, XK_b, togglebar, {0}},
    {MODKEY, XK_r, reloadconfig, {0}},
    {MODKEY, XK_h, focusdir, {.i = DIR_LEFT}},
    {MODKEY, XK_j, focusdir, {.i = DIR_DOWN}},
    {MODKEY, XK_k, focusdir, {.i = DIR_UP}},
    {MODKEY, XK_l, focusdir, {.i = DIR_RIGHT}},
    {MODKEY | ShiftMask, XK_r, restartwm, {0}},
    {MODKEY | ShiftMask, XK_h, resizebydir, {.i = DIR_LEFT}},
    {MODKEY | ShiftMask, XK_j, resizebydir, {.i = DIR_DOWN}},
    {MODKEY | ShiftMask, XK_k, resizebydir, {.i = DIR_UP}},
    {MODKEY | ShiftMask, XK_l, resizebydir, {.i = DIR_RIGHT}},
    {MODKEY | ControlMask, XK_j, focusstack, {.i = +1}},
    {MODKEY | ControlMask, XK_k, focusstack, {.i = -1}},
    {MODKEY, XK_i, incnmaster, {.i = +1}},
    {MODKEY, XK_d, incnmaster, {.i = -1}},
    {MODKEY | ControlMask, XK_h, setmfact, {.f = -0.05}},
    {MODKEY | ControlMask, XK_l, setmfact, {.f = +0.05}},
    // {MODKEY, XK_Return, zoom, {0}},
    {MODKEY, XK_Tab, view, {0}},
    {MODKEY | ShiftMask, XK_c, killclient, {0}},
    {MODKEY | ShiftMask, XK_colon, promptcommand, {0}},
    {MODKEY, XK_t, setlayout, {.v = &layouts[0]}},
    {MODKEY, XK_f, setlayout, {.v = &layouts[1]}},
    {MODKEY, XK_m, setlayout, {.v = &layouts[2]}},
    {MODKEY, XK_space, setlayout, {0}},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}},
    {MODKEY, XK_0, view, {.ull = ~0ULL}},
    {MODKEY | ShiftMask, XK_0, tag, {.ull = ~0ULL}},
    {MODKEY, XK_comma, focusmon, {.i = -1}},
    {MODKEY, XK_period, focusmon, {.i = +1}},
    {MODKEY | ShiftMask, XK_comma, tagmon, {.i = -1}},
    {MODKEY | ShiftMask, XK_period, tagmon, {.i = +1}},
    WORKSPACEKEYS(XK_1, 0) WORKSPACEKEYS(XK_2, 1) WORKSPACEKEYS(XK_3, 2)
        WORKSPACEKEYS(XK_4, 3) WORKSPACEKEYS(XK_5, 4)
            WORKSPACEKEYS(XK_6, 5) WORKSPACEKEYS(XK_7, 6)
                WORKSPACEKEYS(XK_8, 7) WORKSPACEKEYS(XK_9, 8){
                    MODKEY | ShiftMask, XK_q, quit, {0}},
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
 * ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
    /* click                event mask      button          function argument */
    {ClkLtSymbol, 0, Button1, setlayout, {0}},
    {ClkLtSymbol, 0, Button3, setlayout, {.v = &layouts[2]}},
    {ClkWinTitle, 0, Button2, zoom, {0}},
    {ClkStatusText, 0, Button2, spawn, {.v = termcmd}},
    {ClkClientWin, MODKEY, Button1, movemouse, {0}},
    {ClkClientWin, MODKEY, Button2, togglefloating, {0}},
    {ClkClientWin, MODKEY, Button3, resizemouse, {0}},
    {ClkTagBar, 0, Button1, view, {0}},
    {ClkTagBar, 0, Button3, toggleview, {0}},
    {ClkTagBar, MODKEY, Button1, tag, {0}},
    {ClkTagBar, MODKEY, Button3, toggletag, {0}},
};

// }}} config

static char *xstrdup_local(const char *src) {
  size_t len;
  char *dst;
  if (!src) {
    src = "";
  }
  len = strlen(src);
  dst = ecalloc_type<char>(len + 1);
  memcpy(dst, src, len);
  return dst;
}

static void write_all(int fd, const char *buf, size_t len) {
  while (len > 0) {
    ssize_t written = write(fd, buf, len);
    if (written <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return;
    }
    buf += (size_t)written;
    len -= (size_t)written;
  }
}

static void wm_output_append(const char *buf, size_t len) {
  if (wm_command_reply_fd >= 0) {
    write_all(wm_command_reply_fd, buf, len);
  }
  if (wm_capture_buffer && wm_capture_buffer_size > 0) {
    size_t available = wm_capture_buffer_size - 1;
    if (wm_capture_buffer_len < available) {
      size_t to_copy = MIN(len, available - wm_capture_buffer_len);
      memcpy(wm_capture_buffer + wm_capture_buffer_len, buf, to_copy);
      wm_capture_buffer_len += to_copy;
      wm_capture_buffer[wm_capture_buffer_len] = '\0';
    }
  }
}

static WorkspaceMask get_full_workspace_mask(void) {
  if (workspaces_len == 0) {
    return 0;
  }
  if (workspaces_len >= 64) {
    return ~0ULL;
  }
  return (1ULL << workspaces_len) - 1ULL;
}

static Workspace *workspace_by_index(size_t index) {
  if (index >= workspaces_len) {
    return NULL;
  }
  return &workspaces[index];
}

static WorkspaceMask workspace_mask_from_index(size_t index) {
  if (index >= 64) {
    return 0;
  }
  return 1ULL << index;
}

static const char *workspace_name_from_mask(WorkspaceMask mask) {
  size_t i;
  for (i = 0; i < workspaces_len; i++) {
    if (mask & workspace_mask_from_index(i)) {
      return workspaces[i].name;
    }
  }
  return "";
}

static size_t workspace_count_visible(void) { return workspaces_len; }

static size_t workspace_find(const char *name) {
  size_t i;
  if (!name || name[0] == '\0') {
    return SIZE_MAX;
  }
  for (i = 0; i < workspaces_len; i++) {
    if (strcmp(workspaces[i].name, name) == 0) {
      return i;
    }
  }
  return SIZE_MAX;
}

static size_t workspace_index_from_mask(WorkspaceMask mask) {
  size_t i;
  if (!mask || (mask & (mask - 1ULL))) {
    return SIZE_MAX;
  }
  for (i = 0; i < workspaces_len; i++) {
    if (mask == workspace_mask_from_index(i)) {
      return i;
    }
  }
  return SIZE_MAX;
}

static int workspace_has_clients(size_t index) {
  Monitor *m;
  WorkspaceMask mask;
  Client *c;

  if (index >= workspaces_len) {
    return 0;
  }
  mask = workspace_mask_from_index(index);
  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->tags & mask) {
        return 1;
      }
    }
  }
  return 0;
}

static int workspace_is_visible_on_any_monitor(size_t index) {
  Monitor *m;
  WorkspaceMask mask;

  if (index >= workspaces_len) {
    return 0;
  }
  mask = workspace_mask_from_index(index);
  for (m = mons; m; m = m->next) {
    if (m->tagset[m->seltags] & mask) {
      return 1;
    }
  }
  return 0;
}

static WorkspaceMask workspace_reindex_mask_after_remove(WorkspaceMask mask,
                                                         size_t removed_index) {
  WorkspaceMask result = 0;
  size_t i;

  for (i = 0; i < 64; i++) {
    WorkspaceMask bit = 1ULL << i;
    if (!(mask & bit)) {
      continue;
    }
    if (i == removed_index) {
      continue;
    }
    if (i > removed_index) {
      result |= 1ULL << (i - 1);
    } else {
      result |= bit;
    }
  }
  return result;
}

static void workspace_sync_after_registry_change(void) {
  Monitor *m;
  WorkspaceMask full = get_full_workspace_mask();
  WorkspaceMask fallback = workspaces_len ? workspace_mask_from_index(0) : 0;

  for (m = mons; m; m = m->next) {
    m->tagset[0] &= full;
    m->tagset[1] &= full;
    if (!m->tagset[0]) {
      m->tagset[0] = fallback;
    }
    if (!m->tagset[1]) {
      m->tagset[1] = fallback;
    }
  }
}

static size_t workspace_ensure(const char *name) {
  Workspace *next;
  size_t found;

  if (!name || name[0] == '\0') {
    return SIZE_MAX;
  }
  found = workspace_find(name);
  if (found != SIZE_MAX) {
    return found;
  }
  if (workspaces_len >= 64) {
    return SIZE_MAX;
  }
  next = static_cast<Workspace *>(
      realloc(workspaces, (workspaces_len + 1) * sizeof(Workspace)));
  if (!next) {
    die("realloc:");
  }
  workspaces = next;
  workspaces[workspaces_len].mask = workspace_mask_from_index(workspaces_len);
  workspaces[workspaces_len].name = xstrdup_local(name);
  workspaces_len++;
  workspace_sync_after_registry_change();
  return workspaces_len - 1;
}

static int workspace_remove_index(size_t index) {
  size_t i;
  Monitor *m;

  if (index >= workspaces_len) {
    return 0;
  }

  free(workspaces[index].name);
  for (i = index + 1; i < workspaces_len; i++) {
    workspaces[i - 1] = workspaces[i];
  }
  workspaces_len--;

  if (workspaces_len == 0) {
    free(workspaces);
    workspaces = NULL;
  } else {
    for (i = index; i < workspaces_len; i++) {
      workspaces[i].mask = workspace_mask_from_index(i);
    }
  }

  for (m = mons; m; m = m->next) {
    Client *c;
    m->tagset[0] = workspace_reindex_mask_after_remove(m->tagset[0], index);
    m->tagset[1] = workspace_reindex_mask_after_remove(m->tagset[1], index);
    for (c = m->clients; c; c = c->next) {
      c->tags = workspace_reindex_mask_after_remove(c->tags, index);
      client_save_workspace_tags(c);
    }
  }

  workspace_sync_after_registry_change();
  return 1;
}

static int workspace_rename(size_t index, const char *name) {
  char *replacement;
  size_t found;
  if (index >= workspaces_len || !name || name[0] == '\0') {
    return 0;
  }
  found = workspace_find(name);
  if (found != SIZE_MAX && found != index) {
    return 0;
  }
  replacement = xstrdup_local(name);
  free(workspaces[index].name);
  workspaces[index].name = replacement;
  return 1;
}

static void workspace_set_defaults(void) {
  size_t i;
  char name[16];
  if (workspaces_len > 0) {
    return;
  }
  for (i = 0; i < 9; i++) {
    snprintf(name, sizeof(name), "%zu", i + 1);
    if (workspace_ensure(name) == SIZE_MAX) {
      die("failed to create default workspace");
    }
  }
}

static int workspace_set_list_from_lua(lua_State *L, int index) {
  size_t len;
  size_t i;
  Workspace *old_workspaces = workspaces;
  size_t old_len = workspaces_len;

  luaL_checktype(L, index, LUA_TTABLE);
  workspaces = NULL;
  workspaces_len = 0;
  len = (size_t)lua_rawlen(L, index);
  for (i = 1; i <= len; i++) {
    lua_rawgeti(L, index, (int)i);
    if (!lua_isstring(L, -1) ||
        workspace_ensure(lua_tostring(L, -1)) == SIZE_MAX) {
      size_t j;
      lua_pop(L, 1);
      for (j = 0; j < workspaces_len; j++) {
        free(workspaces[j].name);
      }
      free(workspaces);
      workspaces = old_workspaces;
      workspaces_len = old_len;
      return 0;
    }
    lua_pop(L, 1);
  }
  if (workspaces_len == 0) {
    workspace_set_defaults();
  }
  if (old_workspaces) {
    for (i = 0; i < old_len; i++) {
      free(old_workspaces[i].name);
    }
    free(old_workspaces);
  }
  workspace_sync_after_registry_change();
  return 1;
}

static void workspace_view_mask(Monitor *m, WorkspaceMask mask) {
  if (!m) {
    return;
  }
  if ((mask & get_full_workspace_mask()) == m->tagset[m->seltags]) {
    return;
  }
  m->seltags ^= 1;
  if (mask & get_full_workspace_mask()) {
    m->tagset[m->seltags] = mask & get_full_workspace_mask();
  }
  if (!m->tagset[m->seltags] && workspaces_len > 0) {
    m->tagset[m->seltags] = workspace_mask_from_index(0);
  }
  focus(NULL);
  arrange(m);
}

static void get_workspace_socket_path(char *buf, size_t size) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  const char *display_name = getenv("DISPLAY");
  if (runtime && runtime[0] != '\0') {
    snprintf(buf, size, "%s/mwm-%s.sock", runtime,
             display_name && display_name[0] ? display_name : "display");
    return;
  }
  snprintf(buf, size, "/tmp/mwm-%s.sock",
           display_name && display_name[0] ? display_name : "display");
}

// {{{ void applyrules(Client *c)
void applyrules(Client *c) {
  const char *class_name, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = {NULL, NULL};

  /* rule matching */
  c->isfloating = 0;
  c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class_name = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name ? ch.res_name : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title)) &&
        (!r->class_name || strstr(class_name, r->class_name)) &&
        (!r->instance || strstr(instance, r->instance))) {
      c->isfloating = r->isfloating;
      c->tags |= r->tags;
      for (m = mons; m && m->num != r->monitor; m = m->next) {
        ;
      }
      if (m) {
        c->mon = m;
      }
    }
  }
  if (ch.res_class) {
    XFree(ch.res_class);
  }
  if (ch.res_name) {
    XFree(ch.res_name);
  }
  c->tags = c->tags & get_full_workspace_mask()
                ? c->tags & get_full_workspace_mask()
                : c->mon->tagset[c->mon->seltags];
}
// }}} void applyrules(Client *c)

// {{{ int applysizehints(Client *c, int *x, int
//                        *y, int *w, int *h, int interact)
int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact) {
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw) {
      *x = sw - WIDTH(c);
    }
    if (*y > sh) {
      *y = sh - HEIGHT(c);
    }
    if (*x + *w + 2 * c->bw < 0) {
      *x = 0;
    }
    if (*y + *h + 2 * c->bw < 0) {
      *y = 0;
    }
  } else {
    if (*x >= m->wx + m->ww) {
      *x = m->wx + m->ww - WIDTH(c);
    }
    if (*y >= m->wy + m->wh) {
      *y = m->wy + m->wh - HEIGHT(c);
    }
    if (*x + *w + 2 * c->bw <= m->wx) {
      *x = m->wx;
    }
    if (*y + *h + 2 * c->bw <= m->wy) {
      *y = m->wy;
    }
  }
  if (*h < bh) {
    *h = bh;
  }
  if (*w < bh) {
    *w = bh;
  }
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    if (!c->hintsvalid) {
      updatesizehints(c);
    }
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h) {
        *w = *h * c->maxa + 0.5;
      } else if (c->mina < (float)*h / *w) {
        *h = *w * c->mina + 0.5;
      }
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw) {
      *w -= *w % c->incw;
    }
    if (c->inch) {
      *h -= *h % c->inch;
    }
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw) {
      *w = MIN(*w, c->maxw);
    }
    if (c->maxh) {
      *h = MIN(*h, c->maxh);
    }
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}
// }}} int applysizehints()

// {{{ void arrange(Monitor *m)
void arrange(Monitor *m) {
  if (m) {
    showhide(m->stack);
  } else {
    for (m = mons; m; m = m->next) {
      showhide(m->stack);
    }
  }
  if (m) {
    arrangemon(m);
    restack(m);
  } else {
    for (m = mons; m; m = m->next) {
      arrangemon(m);
    }
  }
}
// }}} void arrange(Monitor *m)

// {{{ void arrangemon(Monitor *m)
void arrangemon(Monitor *m) {
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange) {
    m->lt[m->sellt]->arrange(m);
  }
}
// }}} void arrangemon(Monitor *m)

// {{{ void attach(Client *c)
void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}
// }}} void attach(Client *c)

// {{{ void attachstack(Client *c)
void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}
// }}} void attachstack(Client *c)

// {{{ void buttonpress(XEvent *e)
void buttonpress(XEvent *e) {
  unsigned int x, click;
  size_t i;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;
  unsigned int statusw, trayw;

  click = ClkRootWin;
  if (slider_popup_handle_button(ev)) {
    return;
  }
  if (notif_sidebar_handle_button(ev)) {
    return;
  }
  if (todo_sidebar_handle_button(ev)) {
    return;
  }
  if (agent_sidebar_handle_button(ev)) {
    return;
  }
  /* focus the monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  if (widget_handle_button(m, ev)) {
    return;
  }
  // If the window is equal to the selected monitor's bar window
  if (ev->window == selmon->barwin) {
    i = x = 0;
    statusw = TEXTW(stext);
    trayw = (showsystray && systray && selmon == m) ? getsystraywidth() : 0;
    while (i < workspace_count_visible()) {
      Workspace *ws = workspace_by_index(i);
      if (!ws) {
        break;
      }
      x += TEXTW(ws->name);
      if (ev->x < x) {
        break;
      }
      i++;
    }
    if (i < workspace_count_visible()) {
      click = ClkTagBar;
      arg.ull = workspace_mask_from_index(i);
    } else if (ev->x < x + TEXTW(selmon->ltsymbol)) {
      click = ClkLtSymbol;
    } else if (ev->x > selmon->ww - (int)statusw - (int)trayw) {
      click = ClkStatusText;
    } else {
      click = ClkWinTitle;
    }
  } else if ((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  // Iterate over all buttons in buttons list
  for (i = 0; i < LENGTH(buttons); i++) {
    // If click event matches
    if (click == buttons[i].click && buttons[i].func &&
        buttons[i].button == ev->button &&
        CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)) {
      buttons[i].func(click == ClkTagBar && buttons[i].arg.i == 0 ? &arg
                                                                  : &buttons[i].arg);
    }
  }
}
// }}} void buttonpress(XEvent *e)

// {{{ void buttonrelease(XEvent *e)
void buttonrelease(XEvent *e) {
  XButtonReleasedEvent *ev = &e->xbutton;

  (void)ev;
  slider_popup_dragging = 0;
}
// }}} void buttonrelease(XEvent *e)

// {{{ void checkargs(int argc, char *argv[])
void checkargs(int argc, char *argv[]) {
  if (argc == 2 && !strcmp("-v", argv[1])) {
    die(" ");
  } else if (argc != 1) {
    die("usage: mwm [-v]");
  }
}
// }}} void checkargs(int argc, char *argv[])

// {{{ void checklocale(void)
void checklocale(void) {
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale()) {
    fputs("warning: no locale support\n", stderr);
  }
}
// }}} void checklocale(void)

// {{{ void checkotherwm(void)
void checkotherwm(void) {
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}
// }}} void checkotherwm(void)

// {{{ void checkx11(void)
void checkx11(void) {
  if (!(dpy = XOpenDisplay(NULL))) {
    die("mwm: cannot open display");
  }
}
// }}} void checkx11(void)

// {{{ theme config
static void copy_color_value(char dest[32], const char *src) {
  if (!dest || !src) {
    return;
  }
  snprintf(dest, 32, "%s", src);
}

static void set_default_bar_theme(void) {
  copy_color_value(bar_theme_dark.normal.fg, default_col_gray3);
  copy_color_value(bar_theme_dark.normal.bg, default_col_gray1);
  copy_color_value(bar_theme_dark.normal.border, default_col_gray2);
  copy_color_value(bar_theme_dark.selected.fg, default_col_gray4);
  copy_color_value(bar_theme_dark.selected.bg, default_col_cyan);
  copy_color_value(bar_theme_dark.selected.border, default_col_cyan);

  copy_color_value(bar_theme_light.normal.fg, default_col_light_fg);
  copy_color_value(bar_theme_light.normal.bg, default_col_light_bg);
  copy_color_value(bar_theme_light.normal.border, default_col_light_border);
  copy_color_value(bar_theme_light.selected.fg, default_col_light_accent_fg);
  copy_color_value(bar_theme_light.selected.bg, default_col_light_accent);
  copy_color_value(bar_theme_light.selected.border, default_col_light_accent);
}

static const BarThemeConfig *active_bar_theme(void) {
  return theme_resolved_dark ? &bar_theme_dark : &bar_theme_light;
}

static void freecolorscheme(void) {
  size_t i;

  if (!scheme) {
    return;
  }
  for (i = 0; i < SchemeSel + 1; i++) {
    free(scheme[i]);
  }
  free(scheme);
  scheme = NULL;
}

static void setcolorscheme(void) {
  const BarThemeConfig *active = active_bar_theme();
  const char *colors[SchemeSel + 1][3] = {
      {active->normal.fg, active->normal.bg, active->normal.border},
      {active->selected.fg, active->selected.bg, active->selected.border},
  };
  size_t i;

  freecolorscheme();
  scheme = ecalloc_type<Clr *>(SchemeSel + 1);
  for (i = 0; i < SchemeSel + 1; i++) {
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  }
}

static int system_prefers_dark(void) {
  FILE *fp;
  char buf[128];
  char *tok;
  char *last_tok = NULL;
  int val;

  fp = popen("busctl --user call org.freedesktop.portal.Desktop "
             "/org/freedesktop/portal/desktop org.freedesktop.portal.Settings "
             "Read ss org.freedesktop.appearance color-scheme 2>/dev/null",
             "r");
  if (!fp) {
    return 1;
  }
  if (!fgets(buf, sizeof(buf), fp)) {
    pclose(fp);
    return 1;
  }
  pclose(fp);

  tok = strtok(buf, " \t\n");
  while (tok) {
    last_tok = tok;
    tok = strtok(NULL, " \t\n");
  }
  if (!last_tok || sscanf(last_tok, "%d", &val) != 1) {
    return 1;
  }
  /* xdg-desktop-portal color-scheme: 0 = no preference, 1 = prefer dark,
   * 2 = prefer light. Default to dark when there's no explicit preference. */
  return val != 2;
}

static int theme_resolve_prefers_dark(void) {
  switch (theme_mode) {
  case ThemeModeDark:
    return 1;
  case ThemeModeLight:
    return 0;
  case ThemeModeSystem:
  default:
    return system_prefers_dark();
  }
}

static void widget_refresh_client_borders(void) {
  Monitor *m;
  Client *c;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      XSetWindowBorder(dpy, c->win,
                        scheme[c == m->sel ? SchemeSel : SchemeNorm][ColBorder]
                            .pixel);
    }
  }
}

static void widget_apply_theme(void) {
  setcolorscheme();
  /* Called from updatewidgets(), which setup() also runs once before
   * updategeom() creates the first monitor -- selmon is NULL at that
   * point and focus(NULL) unconditionally dereferences it. */
  if (!selmon) {
    return;
  }
  widget_refresh_client_borders();
  focus(NULL);
  drawbars();
}

static void widget_update_theme(void) {
  int resolved = theme_resolve_prefers_dark();

  if (resolved != theme_resolved_dark) {
    theme_resolved_dark = resolved;
    widget_apply_theme();
  }
}

static void widget_format_theme(char *buf, size_t size) {
  const char *mode_name;
  const char *icon;

  switch (theme_mode) {
  case ThemeModeDark:
    mode_name = "dark";
    icon = icon_theme_dark;
    break;
  case ThemeModeLight:
    mode_name = "light";
    icon = icon_theme_light;
    break;
  case ThemeModeSystem:
  default:
    mode_name = "auto";
    icon = icon_theme_auto;
    break;
  }

  if (theme_mode == ThemeModeSystem) {
    snprintf(buf, size, "%s %s (%s)", icon, mode_name,
             theme_resolved_dark ? "dark" : "light");
    return;
  }
  snprintf(buf, size, "%s %s", icon, mode_name);
}

static void get_theme_state_path(char *buf, size_t size) {
  const char *xdg_cache_home;
  const char *home;

  if (!buf || size == 0) {
    return;
  }
  xdg_cache_home = getenv("XDG_CACHE_HOME");
  if (xdg_cache_home && xdg_cache_home[0] != '\0') {
    snprintf(buf, size, "%s/mwm/theme_mode", xdg_cache_home);
    return;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(buf, size, "%s/.cache/mwm/theme_mode", home);
    return;
  }
  buf[0] = '\0';
}

static void load_theme_mode_state(void) {
  char path[PATH_MAX];
  char line[32];
  FILE *fp;

  theme_mode = ThemeModeSystem;
  get_theme_state_path(path, sizeof(path));
  if (path[0] == '\0') {
    return;
  }

  fp = fopen(path, "r");
  if (!fp) {
    return;
  }
  if (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (strcmp(line, "dark") == 0) {
      theme_mode = ThemeModeDark;
    } else if (strcmp(line, "light") == 0) {
      theme_mode = ThemeModeLight;
    } else {
      theme_mode = ThemeModeSystem;
    }
  }
  fclose(fp);
}

static void save_theme_mode_state(void) {
  char path[PATH_MAX];
  char dir[PATH_MAX];
  char *slash;
  FILE *fp;
  const char *name;

  get_theme_state_path(path, sizeof(path));
  if (path[0] == '\0') {
    return;
  }

  snprintf(dir, sizeof(dir), "%s", path);
  slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    mkdir(dir, 0755);
  }

  switch (theme_mode) {
  case ThemeModeDark:
    name = "dark";
    break;
  case ThemeModeLight:
    name = "light";
    break;
  case ThemeModeSystem:
  default:
    name = "system";
    break;
  }

  fp = fopen(path, "w");
  if (!fp) {
    return;
  }
  fprintf(fp, "%s\n", name);
  fclose(fp);
}

static void widget_cycle_theme_mode(int direction) {
  int next = (((int)theme_mode + direction) % ThemeModeCount + ThemeModeCount) %
             ThemeModeCount;

  theme_mode = (enum ThemeMode)next;
  theme_resolved_dark = theme_resolve_prefers_dark();
  save_theme_mode_state();
  widget_apply_theme();
}

static void get_theme_config_path(char *buf, size_t size) {
  const char *xdg_config_home;
  const char *home;

  if (!buf || size == 0) {
    return;
  }
  xdg_config_home = getenv("XDG_CONFIG_HOME");
  if (xdg_config_home && xdg_config_home[0] != '\0') {
    snprintf(buf, size, "%s/mwm/config.lua", xdg_config_home);
    return;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(buf, size, "%s/.config/mwm/config.lua", home);
    return;
  }
  buf[0] = '\0';
}

static void lua_read_colorset(lua_State *L, int index, ColorSetConfig *config) {
  if (!lua_istable(L, index) || !config) {
    return;
  }
  lua_getfield(L, index, "fg");
  if (lua_isstring(L, -1)) {
    copy_color_value(config->fg, lua_tostring(L, -1));
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "bg");
  if (lua_isstring(L, -1)) {
    copy_color_value(config->bg, lua_tostring(L, -1));
  }
  lua_pop(L, 1);
  lua_getfield(L, index, "border");
  if (lua_isstring(L, -1)) {
    copy_color_value(config->border, lua_tostring(L, -1));
  }
  lua_pop(L, 1);
}

static int lua_mwm_theme(lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);

  lua_getfield(L, 1, "topbar");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "normal");
    if (lua_istable(L, -1)) {
      lua_read_colorset(L, lua_gettop(L), &bar_theme_dark.normal);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "selected");
    if (lua_istable(L, -1)) {
      lua_read_colorset(L, lua_gettop(L), &bar_theme_dark.selected);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "topbar_light");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "normal");
    if (lua_istable(L, -1)) {
      lua_read_colorset(L, lua_gettop(L), &bar_theme_light.normal);
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "selected");
    if (lua_istable(L, -1)) {
      lua_read_colorset(L, lua_gettop(L), &bar_theme_light.selected);
    }
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  return 0;
}

static int lua_print_to_client(lua_State *L) {
  int n = lua_gettop(L);
  int i;
  for (i = 1; i <= n; i++) {
    size_t len;
    const char *text;
    luaL_tolstring(L, i, &len);
    wm_output_append(text, len);
    if (i < n) {
      wm_output_append("\t", 1);
    }
     lua_pop(L, 1);
  }
  wm_output_append("\n", 1);
  return 0;
}

static size_t lua_workspace_index_arg(lua_State *L, int arg_index) {
  lua_Integer idx;
  size_t named;
  if (lua_type(L, arg_index) == LUA_TNUMBER) {
    idx = lua_tointeger(L, arg_index);
    if (idx <= 0 || (size_t)idx > workspaces_len) {
      return SIZE_MAX;
    }
    return (size_t)(idx - 1);
  }
  if (lua_type(L, arg_index) == LUA_TSTRING) {
    named = workspace_find(lua_tostring(L, arg_index));
    return named;
  }
  return SIZE_MAX;
}

static int lua_mwm_workspaces(lua_State *L) {
  if (!workspace_set_list_from_lua(L, 1)) {
    return luaL_error(L, "invalid workspace list");
  }
  return 0;
}

static int lua_mwm_create_workspace(lua_State *L) {
  const char *name = luaL_checkstring(L, 1);
  size_t index = workspace_ensure(name);
  if (index == SIZE_MAX) {
    return luaL_error(L, "unable to create workspace");
  }
  drawbars();
  lua_pushinteger(L, (lua_Integer)index + 1);
  return 1;
}

static int lua_mwm_focus_workspace(lua_State *L) {
  const char *name = NULL;
  size_t old_index = SIZE_MAX;
  size_t index;
  Monitor *m;

  if (lua_type(L, 1) == LUA_TNUMBER) {
    index = lua_workspace_index_arg(L, 1);
    if (index == SIZE_MAX) {
      return luaL_error(L, "unknown workspace");
    }
    workspace_view_mask(selmon, workspace_mask_from_index(index));
    lua_pushinteger(L, (lua_Integer)index + 1);
    return 1;
  }

  name = luaL_checkstring(L, 1);
  if (selmon) {
    old_index = workspace_index_from_mask(selmon->tagset[selmon->seltags]);
  }
  index = workspace_ensure(name);
  if (index == SIZE_MAX) {
    return luaL_error(L, "unable to focus workspace");
  }

  workspace_view_mask(selmon, workspace_mask_from_index(index));

  if (old_index != SIZE_MAX && old_index < workspaces_len &&
      old_index != workspace_index_from_mask(selmon->tagset[selmon->seltags]) &&
      !workspace_has_clients(old_index) &&
      !workspace_is_visible_on_any_monitor(old_index)) {
    workspace_remove_index(old_index);
    focus(NULL);
    for (m = mons; m; m = m->next) {
      arrange(m);
    }
    drawbars();
  }

  index = workspace_find(name);
  if (index == SIZE_MAX) {
    return luaL_error(L, "workspace disappeared");
  }
  lua_pushinteger(L, (lua_Integer)index + 1);
  return 1;
}

static int lua_mwm_rename_workspace(lua_State *L) {
  size_t index = lua_workspace_index_arg(L, 1);
  const char *name = luaL_checkstring(L, 2);
  if (index == SIZE_MAX) {
    return luaL_error(L, "unknown workspace");
  }
  if (!workspace_rename(index, name)) {
    return luaL_error(L, "unable to rename workspace");
  }
  drawbars();
  lua_pushstring(L, workspaces[index].name);
  return 1;
}

static int lua_mwm_list_workspaces(lua_State *L) {
  size_t i;
  lua_createtable(L, (int)workspaces_len, 0);
  for (i = 0; i < workspaces_len; i++) {
    lua_pushstring(L, workspaces[i].name);
    lua_rawseti(L, -2, (int)i + 1);
  }
  return 1;
}

static int lua_mwm_exec(lua_State *L) {
  const char *cmd;
  FILE *fp;
  char buf[4096];
  size_t len;

  cmd = luaL_checkstring(L, 1);
  fp = popen(cmd, "r");
  if (!fp) {
    lua_pushstring(L, "");
    return 1;
  }
  len = fread(buf, 1, sizeof(buf) - 1, fp);
  pclose(fp);
  buf[len] = '\0';
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
    buf[--len] = '\0';
  }
  lua_pushstring(L, buf);
  return 1;
}

static int lua_mwm_widget(lua_State *L) {
  LuaWidget *widget;

  luaL_checktype(L, 1, LUA_TTABLE);

  if (lua_widgets_len >= MAX_LUA_WIDGETS) {
    return luaL_error(L, "too many widgets (max %d)", MAX_LUA_WIDGETS);
  }

  widget = &lua_widgets[lua_widgets_len];
  memset(widget, 0, sizeof(*widget));
  widget->update_ref = LUA_NOREF;
  widget->click_ref = LUA_NOREF;

  lua_getfield(L, 1, "name");
  if (lua_isstring(L, -1)) {
    snprintf(widget->name, sizeof(widget->name), "%s", lua_tostring(L, -1));
  } else {
    snprintf(widget->name, sizeof(widget->name), "widget%zu", lua_widgets_len + 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "highlight");
  widget->highlight = lua_toboolean(L, -1);
  lua_pop(L, 1);

  lua_getfield(L, 1, "update");
  if (lua_isfunction(L, -1)) {
    widget->update_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    lua_pop(L, 1);
  }

  lua_getfield(L, 1, "click");
  if (lua_isfunction(L, -1)) {
    widget->click_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  } else {
    lua_pop(L, 1);
  }

  snprintf(widget->text, sizeof(widget->text), "%s", widget->name);
  lua_widgets_len++;
  return 0;
}

static void reset_lua_widgets(void) {
  /* The Lua state that owns update_ref/click_ref is about to be closed and
   * recreated by the caller, which invalidates every registry ref -- there
   * is nothing to individually unref. */
  lua_widgets_len = 0;
}

static void widget_update_lua_widgets(void) {
  size_t i;
  int status;
  const char *result;

  if (!command_lua) {
    return;
  }

  for (i = 0; i < lua_widgets_len; i++) {
    LuaWidget *widget = &lua_widgets[i];
    if (widget->update_ref == LUA_NOREF) {
      continue;
    }
    lua_rawgeti(command_lua, LUA_REGISTRYINDEX, widget->update_ref);
    status = lua_pcall(command_lua, 0, 1, 0);
    if (status != LUA_OK) {
      fprintf(stderr, "mwm: widget '%s' update error: %s\n", widget->name,
              lua_tostring(command_lua, -1));
      lua_pop(command_lua, 1);
      snprintf(widget->text, sizeof(widget->text), "%s: error", widget->name);
      continue;
    }
    result = lua_tostring(command_lua, -1);
    if (result) {
      snprintf(widget->text, sizeof(widget->text), "%s", result);
    } else {
      widget->text[0] = '\0';
    }
    lua_pop(command_lua, 1);
  }
}

static void widget_lua_click(size_t index, int button) {
  int status;
  LuaWidget *widget;

  if (!command_lua || index >= lua_widgets_len) {
    return;
  }
  widget = &lua_widgets[index];
  if (widget->click_ref == LUA_NOREF) {
    return;
  }
  lua_rawgeti(command_lua, LUA_REGISTRYINDEX, widget->click_ref);
  lua_pushinteger(command_lua, button);
  status = lua_pcall(command_lua, 1, 0, 0);
  if (status != LUA_OK) {
    fprintf(stderr, "mwm: widget '%s' click error: %s\n", widget->name,
            lua_tostring(command_lua, -1));
    lua_pop(command_lua, 1);
  }
  updatewidgets();
  drawbars();
}

/* {{{ notification sidebar */

static unsigned int notification_add(const char *app_name, const char *summary,
                                     const char *body, unsigned int replaces_id,
                                     int expire_timeout_ms) {
  Notification *n = NULL;
  time_t now = time(NULL);

  if (replaces_id != 0) {
    for (n = notifications; n; n = n->next) {
      if (n->id == replaces_id) {
        break;
      }
    }
  }

  if (!n) {
    n = ecalloc_type<Notification>();
    n->id = notification_next_id++;
    n->next = notifications;
    notifications = n;
    notification_count++;
  }

  snprintf(n->app_name, sizeof(n->app_name), "%s", app_name ? app_name : "");
  snprintf(n->summary, sizeof(n->summary), "%s", summary ? summary : "");
  snprintf(n->body, sizeof(n->body), "%s", body ? body : "");
  n->received = now;
  n->expire_at = expire_timeout_ms > 0 ? now + expire_timeout_ms / 1000 : 0;

  notification_unread++;

  while (notification_count > MAX_NOTIFICATIONS) {
    Notification *prev = notifications;
    if (!prev || !prev->next) {
      break;
    }
    while (prev->next && prev->next->next) {
      prev = prev->next;
    }
    free(prev->next);
    prev->next = NULL;
    notification_count--;
  }

  if (notif_sidebar_visible) {
    draw_notif_sidebar();
  }
  drawbars();
  return n->id;
}

static void notification_remove(unsigned int id, unsigned int reason) {
  Notification *cur = notifications;
  Notification *prev = NULL;

  while (cur) {
    if (cur->id == id) {
      if (prev) {
        prev->next = cur->next;
      } else {
        notifications = cur->next;
      }
      free(cur);
      if (notification_count > 0) {
        notification_count--;
      }
      notif_dbus_emit_notification_closed(id, reason);
      return;
    }
    prev = cur;
    cur = cur->next;
  }
}

static void notification_clear_all(void) {
  Notification *cur = notifications;

  while (cur) {
    Notification *next = cur->next;
    notif_dbus_emit_notification_closed(cur->id, 2);
    free(cur);
    cur = next;
  }
  notifications = NULL;
  notification_count = 0;
  notification_unread = 0;
}

static void notification_expire_check(void) {
  Notification *cur = notifications;
  Notification *prev = NULL;
  time_t now = time(NULL);
  int changed = 0;

  while (cur) {
    if (cur->expire_at != 0 && cur->expire_at <= now) {
      Notification *dead = cur;

      if (prev) {
        prev->next = cur->next;
      } else {
        notifications = cur->next;
      }
      cur = cur->next;
      if (notification_count > 0) {
        notification_count--;
      }
      notif_dbus_emit_notification_closed(dead->id, 1);
      free(dead);
      changed = 1;
      continue;
    }
    prev = cur;
    cur = cur->next;
  }
  if (changed && notif_sidebar_visible) {
    draw_notif_sidebar();
  }
}

static void notif_dbus_try_acquire(void) {
  DBusError err;
  int ret;

  if (!notif_dbus_conn || notif_dbus_owned) {
    return;
  }
  dbus_error_init(&err);
  ret = dbus_bus_request_name(notif_dbus_conn, "org.freedesktop.Notifications",
                               DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
  if (dbus_error_is_set(&err)) {
    dbus_error_free(&err);
  }
  if (ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    notif_dbus_owned = 1;
    fprintf(stderr, "mwm: acquired org.freedesktop.Notifications\n");
  }
}

static void notif_dbus_init(void) {
  DBusError err;

  dbus_error_init(&err);
  notif_dbus_conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err) || !notif_dbus_conn) {
    fprintf(stderr, "mwm: dbus session bus connect failed: %s\n",
            dbus_error_is_set(&err) ? err.message : "unknown error");
    dbus_error_free(&err);
    notif_dbus_conn = NULL;
    return;
  }
  dbus_connection_set_exit_on_disconnect(notif_dbus_conn, FALSE);

  notif_dbus_try_acquire();
  if (!notif_dbus_owned) {
    fprintf(stderr,
            "mwm: another notification daemon already owns "
            "org.freedesktop.Notifications (dunst/mako/etc?) -- disable it "
            "for mwm's notification sidebar to receive notifications. "
            "Retrying periodically.\n");
  }
}

static void notif_dbus_shutdown(void) {
  if (!notif_dbus_conn) {
    return;
  }
  if (notif_dbus_owned) {
    dbus_bus_release_name(notif_dbus_conn, "org.freedesktop.Notifications", NULL);
  }
  dbus_connection_close(notif_dbus_conn);
  dbus_connection_unref(notif_dbus_conn);
  notif_dbus_conn = NULL;
  notif_dbus_owned = 0;
}

static void notif_dbus_emit_notification_closed(unsigned int id, unsigned int reason) {
  DBusMessage *signal;

  if (!notif_dbus_conn || !notif_dbus_owned) {
    return;
  }
  signal = dbus_message_new_signal("/org/freedesktop/Notifications",
                                   "org.freedesktop.Notifications",
                                   "NotificationClosed");
  if (!signal) {
    return;
  }
  dbus_message_append_args(signal, DBUS_TYPE_UINT32, &id, DBUS_TYPE_UINT32,
                           &reason, DBUS_TYPE_INVALID);
  dbus_connection_send(notif_dbus_conn, signal, NULL);
  dbus_message_unref(signal);
}

static void notif_dbus_handle_notify(DBusMessage *msg) {
  DBusMessageIter iter;
  DBusMessageIter reply_iter;
  const char *app_name = "";
  const char *app_icon = "";
  const char *summary = "";
  const char *body = "";
  dbus_uint32_t replaces_id = 0;
  dbus_int32_t expire_timeout = -1;
  dbus_uint32_t id = 0;
  DBusMessage *reply;

  if (dbus_message_iter_init(msg, &iter)) {
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &app_name);
    }
    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
      dbus_message_iter_get_basic(&iter, &replaces_id);
    }
    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &app_icon);
    }
    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &summary);
    }
    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
      dbus_message_iter_get_basic(&iter, &body);
    }
    dbus_message_iter_next(&iter);
    /* actions: as -- skip */
    dbus_message_iter_next(&iter);
    /* hints: a{sv} -- skip */
    dbus_message_iter_next(&iter);
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
      dbus_message_iter_get_basic(&iter, &expire_timeout);
    }
  }
  (void)app_icon;

  id = notification_add(app_name, summary, body, replaces_id, expire_timeout);

  reply = dbus_message_new_method_return(msg);
  if (reply) {
    dbus_message_iter_init_append(reply, &reply_iter);
    dbus_message_iter_append_basic(&reply_iter, DBUS_TYPE_UINT32, &id);
    dbus_connection_send(notif_dbus_conn, reply, NULL);
    dbus_message_unref(reply);
  }
}

static void notif_dbus_handle_close_notification(DBusMessage *msg) {
  DBusMessageIter iter;
  dbus_uint32_t id = 0;
  DBusMessage *reply;

  if (dbus_message_iter_init(msg, &iter) &&
      dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT32) {
    dbus_message_iter_get_basic(&iter, &id);
  }

  notification_remove(id, 3);
  if (notif_sidebar_visible) {
    draw_notif_sidebar();
  }
  drawbars();

  reply = dbus_message_new_method_return(msg);
  if (reply) {
    dbus_connection_send(notif_dbus_conn, reply, NULL);
    dbus_message_unref(reply);
  }
}

static void notif_dbus_handle_get_capabilities(DBusMessage *msg) {
  DBusMessage *reply;
  DBusMessageIter iter, arr;
  const char *cap = "body";

  reply = dbus_message_new_method_return(msg);
  if (!reply) {
    return;
  }
  dbus_message_iter_init_append(reply, &iter);
  dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &arr);
  dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &cap);
  dbus_message_iter_close_container(&iter, &arr);
  dbus_connection_send(notif_dbus_conn, reply, NULL);
  dbus_message_unref(reply);
}

static void notif_dbus_handle_get_server_information(DBusMessage *msg) {
  DBusMessage *reply;
  const char *name = "mwm";
  const char *vendor = "mwm";
  const char *version = "1.0";
  const char *spec_version = "1.2";

  reply = dbus_message_new_method_return(msg);
  if (!reply) {
    return;
  }
  dbus_message_append_args(reply, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING,
                           &vendor, DBUS_TYPE_STRING, &version,
                           DBUS_TYPE_STRING, &spec_version, DBUS_TYPE_INVALID);
  dbus_connection_send(notif_dbus_conn, reply, NULL);
  dbus_message_unref(reply);
}

static void notif_dbus_handle_introspect(DBusMessage *msg) {
  DBusMessage *reply;
  const char *xml =
      "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection "
      "1.0//EN\"\n"
      "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
      "<node>\n"
      "  <interface name=\"org.freedesktop.Notifications\">\n"
      "    <method name=\"Notify\">\n"
      "      <arg type=\"s\" name=\"app_name\" direction=\"in\"/>\n"
      "      <arg type=\"u\" name=\"replaces_id\" direction=\"in\"/>\n"
      "      <arg type=\"s\" name=\"app_icon\" direction=\"in\"/>\n"
      "      <arg type=\"s\" name=\"summary\" direction=\"in\"/>\n"
      "      <arg type=\"s\" name=\"body\" direction=\"in\"/>\n"
      "      <arg type=\"as\" name=\"actions\" direction=\"in\"/>\n"
      "      <arg type=\"a{sv}\" name=\"hints\" direction=\"in\"/>\n"
      "      <arg type=\"i\" name=\"expire_timeout\" direction=\"in\"/>\n"
      "      <arg type=\"u\" name=\"id\" direction=\"out\"/>\n"
      "    </method>\n"
      "    <method name=\"CloseNotification\">\n"
      "      <arg type=\"u\" name=\"id\" direction=\"in\"/>\n"
      "    </method>\n"
      "    <method name=\"GetCapabilities\">\n"
      "      <arg type=\"as\" direction=\"out\"/>\n"
      "    </method>\n"
      "    <method name=\"GetServerInformation\">\n"
      "      <arg type=\"s\" direction=\"out\"/>\n"
      "      <arg type=\"s\" direction=\"out\"/>\n"
      "      <arg type=\"s\" direction=\"out\"/>\n"
      "      <arg type=\"s\" direction=\"out\"/>\n"
      "    </method>\n"
      "    <signal name=\"NotificationClosed\">\n"
      "      <arg type=\"u\"/>\n"
      "      <arg type=\"u\"/>\n"
      "    </signal>\n"
      "    <signal name=\"ActionInvoked\">\n"
      "      <arg type=\"u\"/>\n"
      "      <arg type=\"s\"/>\n"
      "    </signal>\n"
      "  </interface>\n"
      "</node>\n";

  reply = dbus_message_new_method_return(msg);
  if (!reply) {
    return;
  }
  dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml, DBUS_TYPE_INVALID);
  dbus_connection_send(notif_dbus_conn, reply, NULL);
  dbus_message_unref(reply);
}

static void notif_dbus_handle_message(DBusMessage *msg) {
  if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications", "Notify")) {
    notif_dbus_handle_notify(msg);
  } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications",
                                         "CloseNotification")) {
    notif_dbus_handle_close_notification(msg);
  } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications",
                                         "GetCapabilities")) {
    notif_dbus_handle_get_capabilities(msg);
  } else if (dbus_message_is_method_call(msg, "org.freedesktop.Notifications",
                                         "GetServerInformation")) {
    notif_dbus_handle_get_server_information(msg);
  } else if (dbus_message_is_method_call(msg, DBUS_INTERFACE_INTROSPECTABLE,
                                         "Introspect")) {
    notif_dbus_handle_introspect(msg);
  } else if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL) {
    DBusMessage *reply = dbus_message_new_error(
        msg, DBUS_ERROR_UNKNOWN_METHOD, "mwm: method not implemented");
    if (reply) {
      dbus_connection_send(notif_dbus_conn, reply, NULL);
      dbus_message_unref(reply);
    }
  }
}

static void notif_dbus_process(void) {
  DBusMessage *msg;

  if (!notif_dbus_conn) {
    return;
  }
  dbus_connection_read_write(notif_dbus_conn, 0);
  while ((msg = dbus_connection_pop_message(notif_dbus_conn)) != NULL) {
    notif_dbus_handle_message(msg);
    dbus_message_unref(msg);
  }
}

static void widget_format_notifications(char *buf, size_t size) {
  if (notification_unread > 0) {
    snprintf(buf, size, "%s %zu", icon_bell, notification_unread);
  } else {
    snprintf(buf, size, "%s", icon_bell_outline);
  }
}

static size_t notif_wrap_body(const char *text, int max_width, char out[][256],
                              size_t max_lines) {
  size_t line_count = 0;
  char word[256];
  char current[256];
  const char *p = text;
  size_t wlen;

  current[0] = '\0';
  if (!text) {
    return 0;
  }
  while (*p && line_count < max_lines) {
    while (*p == ' ') {
      p++;
    }
    wlen = 0;
    while (*p && *p != ' ' && wlen < sizeof(word) - 1) {
      word[wlen++] = *p++;
    }
    word[wlen] = '\0';
    if (wlen == 0) {
      break;
    }

    {
      char trial[256];
      if (current[0]) {
        snprintf(trial, sizeof(trial), "%s %s", current, word);
      } else {
        snprintf(trial, sizeof(trial), "%s", word);
      }
      if (!current[0] || (int)drw_fontset_getwidth(drw, trial) <= max_width) {
        snprintf(current, sizeof(current), "%s", trial);
      } else {
        snprintf(out[line_count], 256, "%s", current);
        line_count++;
        snprintf(current, sizeof(current), "%s", word);
      }
    }
  }
  if (current[0] && line_count < max_lines) {
    snprintf(out[line_count], 256, "%s", current);
    line_count++;
  }
  if (*p && line_count > 0) {
    size_t len = strlen(out[line_count - 1]);
    if (len > 250) {
      len = 250;
    }
    snprintf(out[line_count - 1] + len, 256 - len, "...");
  }
  return line_count;
}

static void notif_format_relative_time(time_t t, char *buf, size_t size) {
  time_t now = time(NULL);
  long diff = (long)(now - t);

  if (diff < 60) {
    snprintf(buf, size, "now");
  } else if (diff < 3600) {
    snprintf(buf, size, "%ldm", diff / 60);
  } else if (diff < 86400) {
    snprintf(buf, size, "%ldh", diff / 3600);
  } else {
    snprintf(buf, size, "%ldd", diff / 86400);
  }
}

static int notif_sidebar_content_height(void) {
  Notification *n;
  int total = 0;
  char lines[3][256];
  int max_w = notif_sidebar_w - 2 * notif_row_pad;

  for (n = notifications; n; n = n->next) {
    size_t nlines = notif_wrap_body(n->body, max_w, lines, 3);
    total += notif_row_pad + bh + bh + (int)nlines * bh + notif_row_pad;
  }
  return total;
}

static void notif_sidebar_position(Monitor *m) {
  if (!m || notif_sidebar_win == None) {
    return;
  }
  XMoveResizeWindow(dpy, notif_sidebar_win, notif_sidebar_cur_x, m->wy,
                    notif_sidebar_w, m->wh);
}

static void draw_notif_sidebar(void) {
  Notification *n;
  int header_h;
  int content_h;
  int max_scroll;
  int panel_h;
  int y;
  int max_w;
  char time_buf[16];
  char lines[3][256];

  if (notif_sidebar_win == None) {
    return;
  }

  header_h = bh * 2;
  panel_h = notif_sidebar_mon ? notif_sidebar_mon->wh : header_h;
  max_w = notif_sidebar_w - 2 * notif_row_pad;

  content_h = notif_sidebar_content_height();
  max_scroll = content_h - (panel_h - header_h);
  if (max_scroll < 0) {
    max_scroll = 0;
  }
  if (notif_scroll_offset > max_scroll) {
    notif_scroll_offset = max_scroll;
  }
  if (notif_scroll_offset < 0) {
    notif_scroll_offset = 0;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, notif_sidebar_w, panel_h, 1, 1);

  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, notif_row_pad, 0, notif_sidebar_w - 2 * notif_row_pad, header_h,
           0, "Notifications", 0);

  notif_clear_h = bh;
  notif_clear_w = (int)drw_fontset_getwidth(drw, "clear") + notif_row_pad;
  notif_clear_x = notif_sidebar_w - notif_row_pad - notif_clear_w;
  notif_clear_y = 0;
  if (notification_count > 0) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, notif_clear_x, notif_clear_y, notif_clear_w, header_h, 0,
             "clear", 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, header_h - 1, notif_sidebar_w, 1, 1, 0);

  notif_row_layout_len = 0;
  y = header_h - notif_scroll_offset;

  if (!notifications) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, notif_row_pad, header_h, notif_sidebar_w - 2 * notif_row_pad,
             bh, 0, "No notifications", 0);
  }

  for (n = notifications; n; n = n->next) {
    size_t nlines = notif_wrap_body(n->body, max_w, lines, 3);
    int card_h = notif_row_pad + bh + bh + (int)nlines * bh + notif_row_pad;
    int card_y = y;

    if (card_y + card_h >= header_h && card_y < panel_h) {
      int row_y = card_y + notif_row_pad;
      int close_w = bh;
      int close_x = notif_sidebar_w - notif_row_pad - close_w;
      int time_w = 40;
      int time_x = close_x - time_w;
      int name_w = time_x - notif_row_pad;
      size_t li;

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, notif_row_pad, row_y, name_w, bh, 0, n->app_name, 0);
      notif_format_relative_time(n->received, time_buf, sizeof(time_buf));
      drw_text(drw, time_x, row_y, time_w, bh, 0, time_buf, 0);
      row_y += bh;

      drw_setscheme(drw, scheme[SchemeSel]);
      drw_text(drw, notif_row_pad, row_y, max_w, bh, 0, n->summary, 0);
      row_y += bh;

      drw_setscheme(drw, scheme[SchemeNorm]);
      for (li = 0; li < nlines; li++) {
        drw_text(drw, notif_row_pad, row_y, max_w, bh, 0, lines[li], 0);
        row_y += bh;
      }

      if (notif_row_layout_len < MAX_NOTIFICATIONS) {
        NotifRowLayout *row = &notif_row_layout[notif_row_layout_len++];
        row->id = n->id;
        row->close_w = close_w;
        row->close_h = bh;
        row->close_x = close_x;
        row->close_y = card_y;
        drw_setscheme(drw, scheme[SchemeNorm]);
        drw_text(drw, row->close_x, row->close_y, row->close_w, row->close_h, 0,
                 icon_close, 0);
      }

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, notif_row_pad, card_y + card_h - 1, max_w, 1, 1, 0);
    }

    y += card_h;
  }

  drw_map(drw, notif_sidebar_win, 0, 0, notif_sidebar_w, panel_h);
}

static void notif_sidebar_open(Monitor *m) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }

  if (notif_sidebar_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;
    notif_sidebar_cur_x = m->wx + m->ww;
    notif_sidebar_win = XCreateWindow(
        dpy, root, notif_sidebar_cur_x, m->wy, notif_sidebar_w, m->wh, 1,
        DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, notif_sidebar_win, cursor[CurNormal]->cursor);
  }

  notif_sidebar_mon = m;
  notif_sidebar_visible = 1;
  notification_unread = 0;
  notif_scroll_offset = 0;

  notif_anim_from_x = notif_sidebar_cur_x;
  notif_anim_to_x = m->wx + m->ww - notif_sidebar_w;
  notif_anim_state = SidebarAnimOpening;
  gettimeofday(&notif_anim_start, NULL);

  notif_sidebar_position(m);
  draw_notif_sidebar();
  XMapRaised(dpy, notif_sidebar_win);
  drawbars();
}

static void notif_sidebar_close(void) {
  if (notif_sidebar_win == None || !notif_sidebar_visible) {
    return;
  }
  notif_anim_from_x = notif_sidebar_cur_x;
  notif_anim_to_x = notif_sidebar_mon ? notif_sidebar_mon->wx + notif_sidebar_mon->ww
                                      : notif_sidebar_cur_x;
  notif_anim_state = SidebarAnimClosing;
  gettimeofday(&notif_anim_start, NULL);
}

static void notif_sidebar_toggle(Monitor *m) {
  if (notif_sidebar_visible) {
    notif_sidebar_close();
  } else {
    notif_sidebar_open(m);
  }
}

static void notif_sidebar_advance_animation(void) {
  struct timeval now;
  long elapsed_ms;
  double t;
  int mon_y = notif_sidebar_mon ? notif_sidebar_mon->wy : 0;

  if (notif_anim_state == SidebarAnimIdle || notif_sidebar_win == None) {
    return;
  }

  gettimeofday(&now, NULL);
  elapsed_ms = (now.tv_sec - notif_anim_start.tv_sec) * 1000 +
              (now.tv_usec - notif_anim_start.tv_usec) / 1000;

  if (elapsed_ms >= notif_anim_duration_ms) {
    notif_sidebar_cur_x = notif_anim_to_x;
    XMoveWindow(dpy, notif_sidebar_win, notif_sidebar_cur_x, mon_y);
    if (notif_anim_state == SidebarAnimClosing) {
      XUnmapWindow(dpy, notif_sidebar_win);
      notif_sidebar_visible = 0;
    }
    notif_anim_state = SidebarAnimIdle;
    return;
  }

  t = (double)elapsed_ms / (double)notif_anim_duration_ms;
  t = 1.0 - pow(1.0 - t, 3.0);
  notif_sidebar_cur_x =
      notif_anim_from_x + (int)((notif_anim_to_x - notif_anim_from_x) * t);
  XMoveWindow(dpy, notif_sidebar_win, notif_sidebar_cur_x, mon_y);
}

static int notif_sidebar_handle_button(XButtonPressedEvent *ev) {
  size_t i;

  if (notif_sidebar_win == None ||
      (!notif_sidebar_visible && notif_anim_state == SidebarAnimIdle)) {
    return 0;
  }

  if (ev->window != notif_sidebar_win) {
    if (notif_sidebar_visible) {
      notif_sidebar_close();
    }
    return 0;
  }

  if (ev->button == Button4) {
    notif_scroll_offset -= 40;
    draw_notif_sidebar();
    return 1;
  }
  if (ev->button == Button5) {
    notif_scroll_offset += 40;
    draw_notif_sidebar();
    return 1;
  }
  if (ev->button != Button1) {
    return 1;
  }

  if (notification_count > 0 && ev->y >= notif_clear_y &&
      ev->y < notif_clear_y + notif_clear_h && ev->x >= notif_clear_x &&
      ev->x < notif_clear_x + notif_clear_w) {
    notification_clear_all();
    draw_notif_sidebar();
    drawbars();
    return 1;
  }

  for (i = 0; i < notif_row_layout_len; i++) {
    NotifRowLayout *row = &notif_row_layout[i];
    if (ev->x >= row->close_x && ev->x < row->close_x + row->close_w &&
        ev->y >= row->close_y && ev->y < row->close_y + row->close_h) {
      notification_remove(row->id, 2);
      draw_notif_sidebar();
      drawbars();
      return 1;
    }
  }

  return 1;
}

/* }}} notification sidebar */

/* {{{ todo sidebar */

static unsigned int todo_add(const char *text) {
  Todo *t = ecalloc_type<Todo>();

  t->id = todo_next_id++;
  snprintf(t->text, sizeof(t->text), "%s", text ? text : "");
  t->done = 0;
  t->created = time(NULL);
  t->next = todos;
  todos = t;
  todo_count++;
  todo_incomplete++;

  while (todo_count > MAX_TODOS) {
    Todo *prev = todos;
    if (!prev || !prev->next) {
      break;
    }
    while (prev->next && prev->next->next) {
      prev = prev->next;
    }
    if (!prev->next->done && todo_incomplete > 0) {
      todo_incomplete--;
    }
    free(prev->next);
    prev->next = NULL;
    todo_count--;
  }

  if (todo_sidebar_visible) {
    draw_todo_sidebar();
  }
  drawbars();
  return t->id;
}

static void todo_remove(unsigned int id) {
  Todo *cur = todos;
  Todo *prev = NULL;

  while (cur) {
    if (cur->id == id) {
      if (prev) {
        prev->next = cur->next;
      } else {
        todos = cur->next;
      }
      if (!cur->done && todo_incomplete > 0) {
        todo_incomplete--;
      }
      free(cur);
      if (todo_count > 0) {
        todo_count--;
      }
      return;
    }
    prev = cur;
    cur = cur->next;
  }
}

static void todo_toggle(unsigned int id) {
  Todo *t;

  for (t = todos; t; t = t->next) {
    if (t->id == id) {
      t->done = !t->done;
      if (t->done) {
        if (todo_incomplete > 0) {
          todo_incomplete--;
        }
      } else {
        todo_incomplete++;
      }
      return;
    }
  }
}

static void todo_clear_completed(void) {
  Todo *cur = todos;
  Todo *prev = NULL;

  while (cur) {
    if (cur->done) {
      Todo *dead = cur;

      if (prev) {
        prev->next = cur->next;
      } else {
        todos = cur->next;
      }
      cur = cur->next;
      if (todo_count > 0) {
        todo_count--;
      }
      free(dead);
      continue;
    }
    prev = cur;
    cur = cur->next;
  }
}

static void todo_seed_fake(void) {
  unsigned int id;

  todo_add("Reply to the mwm sidebar feedback thread");
  todo_add("Buy groceries");
  id = todo_add("Write changelog for the workspace rewrite");
  todo_toggle(id);
}

static void widget_format_todos(char *buf, size_t size) {
  if (todo_incomplete > 0) {
    snprintf(buf, size, "%s %zu", icon_todo, todo_incomplete);
  } else {
    snprintf(buf, size, "%s", icon_todo);
  }
}

static int todo_sidebar_content_height(void) {
  Todo *t;
  int total = 0;
  char lines[3][256];
  int max_w = todo_sidebar_w - 3 * todo_row_pad - bh;

  for (t = todos; t; t = t->next) {
    size_t nlines = notif_wrap_body(t->text, max_w, lines, 3);
    if (nlines < 1) {
      nlines = 1;
    }
    total += todo_row_pad + (int)nlines * bh + todo_row_pad;
  }
  return total;
}

static void todo_sidebar_position(Monitor *m) {
  if (!m || todo_sidebar_win == None) {
    return;
  }
  XMoveResizeWindow(dpy, todo_sidebar_win, todo_sidebar_cur_x, m->wy,
                    todo_sidebar_w, m->wh);
}

static void draw_todo_sidebar(void) {
  Todo *t;
  int header_h;
  int content_h;
  int max_scroll;
  int panel_h;
  int y;
  int max_w;
  char lines[3][256];
  int have_completed = 0;

  if (todo_sidebar_win == None) {
    return;
  }

  header_h = bh * 2;
  panel_h = todo_sidebar_mon ? todo_sidebar_mon->wh : header_h;
  max_w = todo_sidebar_w - 3 * todo_row_pad - bh;

  content_h = todo_sidebar_content_height();
  max_scroll = content_h - (panel_h - header_h);
  if (max_scroll < 0) {
    max_scroll = 0;
  }
  if (todo_scroll_offset > max_scroll) {
    todo_scroll_offset = max_scroll;
  }
  if (todo_scroll_offset < 0) {
    todo_scroll_offset = 0;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, todo_sidebar_w, panel_h, 1, 1);

  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, todo_row_pad, 0, todo_sidebar_w - 2 * todo_row_pad, header_h,
           0, "Todos", 0);

  for (t = todos; t; t = t->next) {
    if (t->done) {
      have_completed = 1;
      break;
    }
  }

  todo_clear_h = bh;
  todo_clear_w = (int)drw_fontset_getwidth(drw, "clear done") + todo_row_pad;
  todo_clear_x = todo_sidebar_w - todo_row_pad - todo_clear_w;
  todo_clear_y = 0;
  if (have_completed) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, todo_clear_x, todo_clear_y, todo_clear_w, header_h, 0,
             "clear done", 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, header_h - 1, todo_sidebar_w, 1, 1, 0);

  todo_row_layout_len = 0;
  y = header_h - todo_scroll_offset;

  if (!todos) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, todo_row_pad, header_h, todo_sidebar_w - 2 * todo_row_pad,
             bh, 0, "No todos", 0);
  }

  for (t = todos; t; t = t->next) {
    size_t nlines = notif_wrap_body(t->text, max_w, lines, 3);
    int row_lines = nlines < 1 ? 1 : (int)nlines;
    int card_h = todo_row_pad + row_lines * bh + todo_row_pad;
    int card_y = y;

    if (card_y + card_h >= header_h && card_y < panel_h) {
      int row_y = card_y + todo_row_pad;
      int check_w = bh;
      int check_x = todo_row_pad;
      int close_w = bh;
      int close_x = todo_sidebar_w - todo_row_pad - close_w;
      int text_x = check_x + check_w + todo_row_pad;
      int text_w = close_x - todo_row_pad - text_x;
      size_t li;

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, check_x, row_y, check_w, bh, 0,
               t->done ? icon_checkbox_checked : icon_checkbox_unchecked, 0);

      drw_setscheme(drw, scheme[t->done ? SchemeNorm : SchemeSel]);
      if (nlines == 0) {
        drw_text(drw, text_x, row_y, text_w, bh, 0, t->text, 0);
      } else {
        for (li = 0; li < nlines; li++) {
          drw_text(drw, text_x, row_y + (int)li * bh, text_w, bh, 0, lines[li],
                   0);
        }
      }

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, close_x, card_y, close_w, bh, 0, icon_close, 0);

      if (todo_row_layout_len < MAX_TODOS) {
        TodoRowLayout *row = &todo_row_layout[todo_row_layout_len++];
        row->id = t->id;
        row->check_x = check_x;
        row->check_y = row_y;
        row->check_w = check_w;
        row->check_h = bh;
        row->close_x = close_x;
        row->close_y = card_y;
        row->close_w = close_w;
        row->close_h = bh;
      }

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, todo_row_pad, card_y + card_h - 1,
               todo_sidebar_w - 2 * todo_row_pad, 1, 1, 0);
    }

    y += card_h;
  }

  drw_map(drw, todo_sidebar_win, 0, 0, todo_sidebar_w, panel_h);
}

static void todo_sidebar_open(Monitor *m) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }

  if (todo_sidebar_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;
    todo_sidebar_cur_x = m->wx + m->ww;
    todo_sidebar_win = XCreateWindow(
        dpy, root, todo_sidebar_cur_x, m->wy, todo_sidebar_w, m->wh, 1,
        DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, todo_sidebar_win, cursor[CurNormal]->cursor);
  }

  todo_sidebar_mon = m;
  todo_sidebar_visible = 1;
  todo_scroll_offset = 0;

  todo_anim_from_x = todo_sidebar_cur_x;
  todo_anim_to_x = m->wx + m->ww - todo_sidebar_w;
  todo_anim_state = SidebarAnimOpening;
  gettimeofday(&todo_anim_start, NULL);

  todo_sidebar_position(m);
  draw_todo_sidebar();
  XMapRaised(dpy, todo_sidebar_win);
  drawbars();
}

static void todo_sidebar_close(void) {
  if (todo_sidebar_win == None || !todo_sidebar_visible) {
    return;
  }
  todo_anim_from_x = todo_sidebar_cur_x;
  todo_anim_to_x = todo_sidebar_mon ? todo_sidebar_mon->wx + todo_sidebar_mon->ww
                                    : todo_sidebar_cur_x;
  todo_anim_state = SidebarAnimClosing;
  gettimeofday(&todo_anim_start, NULL);
}

static void todo_sidebar_toggle(Monitor *m) {
  if (todo_sidebar_visible) {
    todo_sidebar_close();
  } else {
    todo_sidebar_open(m);
  }
}

static void todo_sidebar_advance_animation(void) {
  struct timeval now;
  long elapsed_ms;
  double t;
  int mon_y = todo_sidebar_mon ? todo_sidebar_mon->wy : 0;

  if (todo_anim_state == SidebarAnimIdle || todo_sidebar_win == None) {
    return;
  }

  gettimeofday(&now, NULL);
  elapsed_ms = (now.tv_sec - todo_anim_start.tv_sec) * 1000 +
              (now.tv_usec - todo_anim_start.tv_usec) / 1000;

  if (elapsed_ms >= todo_anim_duration_ms) {
    todo_sidebar_cur_x = todo_anim_to_x;
    XMoveWindow(dpy, todo_sidebar_win, todo_sidebar_cur_x, mon_y);
    if (todo_anim_state == SidebarAnimClosing) {
      XUnmapWindow(dpy, todo_sidebar_win);
      todo_sidebar_visible = 0;
    }
    todo_anim_state = SidebarAnimIdle;
    return;
  }

  t = (double)elapsed_ms / (double)todo_anim_duration_ms;
  t = 1.0 - pow(1.0 - t, 3.0);
  todo_sidebar_cur_x =
      todo_anim_from_x + (int)((todo_anim_to_x - todo_anim_from_x) * t);
  XMoveWindow(dpy, todo_sidebar_win, todo_sidebar_cur_x, mon_y);
}

static int todo_sidebar_handle_button(XButtonPressedEvent *ev) {
  size_t i;

  if (todo_sidebar_win == None ||
      (!todo_sidebar_visible && todo_anim_state == SidebarAnimIdle)) {
    return 0;
  }

  if (ev->window != todo_sidebar_win) {
    if (todo_sidebar_visible) {
      todo_sidebar_close();
    }
    return 0;
  }

  if (ev->button == Button4) {
    todo_scroll_offset -= 40;
    draw_todo_sidebar();
    return 1;
  }
  if (ev->button == Button5) {
    todo_scroll_offset += 40;
    draw_todo_sidebar();
    return 1;
  }
  if (ev->button != Button1) {
    return 1;
  }

  if (ev->y >= todo_clear_y && ev->y < todo_clear_y + todo_clear_h &&
      ev->x >= todo_clear_x && ev->x < todo_clear_x + todo_clear_w) {
    todo_clear_completed();
    draw_todo_sidebar();
    drawbars();
    return 1;
  }

  for (i = 0; i < todo_row_layout_len; i++) {
    TodoRowLayout *row = &todo_row_layout[i];
    if (ev->x >= row->check_x && ev->x < row->check_x + row->check_w &&
        ev->y >= row->check_y && ev->y < row->check_y + row->check_h) {
      todo_toggle(row->id);
      draw_todo_sidebar();
      drawbars();
      return 1;
    }
    if (ev->x >= row->close_x && ev->x < row->close_x + row->close_w &&
        ev->y >= row->close_y && ev->y < row->close_y + row->close_h) {
      todo_remove(row->id);
      draw_todo_sidebar();
      drawbars();
      return 1;
    }
  }

  return 1;
}

/* }}} todo sidebar */

/* {{{ agent sidebar */

/* Pulls "field":"value" out of the small flat JSON status files written by
 * agent-status-hook.sh. Not a general JSON parser -- only handles the
 * single-level, string/number-valued shape that script emits. */
static int agent_json_field(const char *json, const char *field, char *out,
                            size_t out_size) {
  char needle[64];
  const char *p;
  size_t oi = 0;

  snprintf(needle, sizeof(needle), "\"%s\"", field);
  p = strstr(json, needle);
  if (!p) {
    out[0] = '\0';
    return 0;
  }
  p += strlen(needle);
  while (*p == ' ' || *p == ':') {
    p++;
  }
  if (*p != '"') {
    out[0] = '\0';
    return 0;
  }
  p++;
  while (*p && *p != '"' && oi + 1 < out_size) {
    if (*p == '\\' && *(p + 1)) {
      p++;
    }
    out[oi++] = *p++;
  }
  out[oi] = '\0';
  return 1;
}

static long agent_json_number(const char *json, const char *field) {
  char needle[64];
  const char *p;

  snprintf(needle, sizeof(needle), "\"%s\"", field);
  p = strstr(json, needle);
  if (!p) {
    return 0;
  }
  p += strlen(needle);
  while (*p == ' ' || *p == ':') {
    p++;
  }
  return atol(p);
}

static unsigned int agent_hash_id(const char *s) {
  unsigned int h = 2166136261u;

  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h == 0 ? 1 : h;
}

static void get_agent_status_dir(char *buf, size_t size) {
  const char *runtime = getenv("XDG_RUNTIME_DIR");
  const char *user = getenv("USER");

  if (runtime && runtime[0] != '\0') {
    snprintf(buf, size, "%s/mwm-agents", runtime);
    return;
  }
  snprintf(buf, size, "/tmp/mwm-agents-%s", user && user[0] ? user : "mwm");
}

static void agents_free_list(void) {
  while (agents) {
    AgentStatus *next = agents->next;
    free(agents);
    agents = next;
  }
}

static AgentStatus *agent_list_find(AgentStatus *head, const char *kind,
                                    const char *cwd) {
  AgentStatus *a;

  for (a = head; a; a = a->next) {
    if (strcmp(a->kind, kind) == 0 && strcmp(a->cwd, cwd) == 0) {
      return a;
    }
  }
  return NULL;
}

static void agents_refresh(void) {
  AgentStatus *new_list = NULL;
  size_t count = 0;
  size_t needs_input = 0;
  DIR *dir;
  struct dirent *entry;

  dir = opendir(agent_status_dir);
  if (dir) {
    while (count < MAX_AGENTS && (entry = readdir(dir))) {
      size_t len = strlen(entry->d_name);
      char path[PATH_MAX];
      FILE *fp;
      char buf[2048];
      size_t n;
      AgentStatus *a;

      if (len < 6 || strcmp(entry->d_name + len - 5, ".json") != 0) {
        continue;
      }
      snprintf(path, sizeof(path), "%s/%s", agent_status_dir, entry->d_name);
      fp = fopen(path, "r");
      if (!fp) {
        continue;
      }
      n = fread(buf, 1, sizeof(buf) - 1, fp);
      fclose(fp);
      buf[n] = '\0';

      a = ecalloc_type<AgentStatus>();
      if (!agent_json_field(buf, "agent", a->kind, sizeof(a->kind)) ||
          !agent_json_field(buf, "status", a->status, sizeof(a->status)) ||
          !agent_json_field(buf, "cwd", a->cwd, sizeof(a->cwd))) {
        free(a);
        continue;
      }
      agent_json_field(buf, "label", a->label, sizeof(a->label));
      a->updated = (time_t)agent_json_number(buf, "updated");
      a->from_file = 1;
      snprintf(a->file_path, sizeof(a->file_path), "%s", path);

      a->next = new_list;
      new_list = a;
      count++;
      if (strcmp(a->status, "needs_input") == 0) {
        needs_input++;
      }
    }
    closedir(dir);
  }

  dir = opendir("/proc");
  if (dir) {
    while (count < MAX_AGENTS && (entry = readdir(dir))) {
      const char *p;
      char comm_path[64];
      char cwd_path[64];
      char comm[32];
      char cwd_link[384];
      FILE *fp;
      ssize_t link_len;
      AgentStatus *a;

      for (p = entry->d_name; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
          break;
        }
      }
      if (*p != '\0' || entry->d_name[0] == '\0') {
        continue;
      }

      snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);
      fp = fopen(comm_path, "r");
      if (!fp) {
        continue;
      }
      comm[0] = '\0';
      if (fgets(comm, sizeof(comm), fp)) {
        size_t clen = strlen(comm);
        if (clen && comm[clen - 1] == '\n') {
          comm[clen - 1] = '\0';
        }
      }
      fclose(fp);

      if (strcmp(comm, "claude") != 0 && strcmp(comm, "codex") != 0) {
        continue;
      }

      snprintf(cwd_path, sizeof(cwd_path), "/proc/%s/cwd", entry->d_name);
      link_len = readlink(cwd_path, cwd_link, sizeof(cwd_link) - 1);
      if (link_len <= 0) {
        continue;
      }
      cwd_link[link_len] = '\0';

      if (agent_list_find(new_list, comm, cwd_link)) {
        continue;
      }

      a = ecalloc_type<AgentStatus>();
      snprintf(a->kind, sizeof(a->kind), "%s", comm);
      snprintf(a->status, sizeof(a->status), "running");
      snprintf(a->label, sizeof(a->label), "running");
      snprintf(a->cwd, sizeof(a->cwd), "%s", cwd_link);
      a->updated = time(NULL);
      a->from_file = 0;
      a->file_path[0] = '\0';

      a->next = new_list;
      new_list = a;
      count++;
    }
    closedir(dir);
  }

  agents_free_list();
  agents = new_list;
  agent_count = count;
  agent_needs_input = needs_input;
  agents_last_refresh = time(NULL);
}

static void widget_format_agents(char *buf, size_t size) {
  if (agent_needs_input > 0) {
    snprintf(buf, size, "%s %zu", icon_agents, agent_needs_input);
  } else if (agent_count > 0) {
    snprintf(buf, size, "%s %zu", icon_agents, agent_count);
  } else {
    snprintf(buf, size, "%s", icon_agents);
  }
}

static int agent_sidebar_content_height(void) {
  AgentStatus *a;
  int total = 0;
  char lines[3][256];
  int max_w = agent_sidebar_w - 2 * agent_row_pad;

  for (a = agents; a; a = a->next) {
    size_t nlines = notif_wrap_body(a->label, max_w, lines, 2);
    if (nlines < 1) {
      nlines = 1;
    }
    total += agent_row_pad + bh + (int)nlines * bh + bh + agent_row_pad;
  }
  return total;
}

static void agent_sidebar_position(Monitor *m) {
  if (!m || agent_sidebar_win == None) {
    return;
  }
  XMoveResizeWindow(dpy, agent_sidebar_win, agent_sidebar_cur_x, m->wy,
                    agent_sidebar_w, m->wh);
}

static void draw_agent_sidebar(void) {
  AgentStatus *a;
  int header_h;
  int content_h;
  int max_scroll;
  int panel_h;
  int y;
  int max_w;
  char time_buf[16];
  char lines[3][256];
  char kind_label[32];
  int have_dismissible = 0;

  if (agent_sidebar_win == None) {
    return;
  }

  header_h = bh * 2;
  panel_h = agent_sidebar_mon ? agent_sidebar_mon->wh : header_h;
  max_w = agent_sidebar_w - 2 * agent_row_pad;

  content_h = agent_sidebar_content_height();
  max_scroll = content_h - (panel_h - header_h);
  if (max_scroll < 0) {
    max_scroll = 0;
  }
  if (agent_scroll_offset > max_scroll) {
    agent_scroll_offset = max_scroll;
  }
  if (agent_scroll_offset < 0) {
    agent_scroll_offset = 0;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, agent_sidebar_w, panel_h, 1, 1);

  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, agent_row_pad, 0, agent_sidebar_w - 2 * agent_row_pad, header_h,
           0, "Agents", 0);

  for (a = agents; a; a = a->next) {
    if (a->from_file) {
      have_dismissible = 1;
      break;
    }
  }

  agent_clear_h = bh;
  agent_clear_w = (int)drw_fontset_getwidth(drw, "clear") + agent_row_pad;
  agent_clear_x = agent_sidebar_w - agent_row_pad - agent_clear_w;
  agent_clear_y = 0;
  if (have_dismissible) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, agent_clear_x, agent_clear_y, agent_clear_w, header_h, 0,
             "clear", 0);
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, header_h - 1, agent_sidebar_w, 1, 1, 0);

  agent_row_layout_len = 0;
  y = header_h - agent_scroll_offset;

  if (!agents) {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, agent_row_pad, header_h, agent_sidebar_w - 2 * agent_row_pad,
             bh, 0, "No agents running", 0);
  }

  for (a = agents; a; a = a->next) {
    size_t nlines = notif_wrap_body(a->label, max_w, lines, 2);
    int label_lines = nlines < 1 ? 1 : (int)nlines;
    int card_h = agent_row_pad + bh + label_lines * bh + bh + agent_row_pad;
    int card_y = y;
    int needs_input = strcmp(a->status, "needs_input") == 0;
    unsigned int row_id;

    if (card_y + card_h >= header_h && card_y < panel_h) {
      int row_y = card_y + agent_row_pad;
      int close_w = bh;
      int close_x = agent_sidebar_w - agent_row_pad - close_w;
      int time_w = 40;
      int time_x = close_x - time_w;
      int name_w = time_x - agent_row_pad;
      size_t li;

      snprintf(kind_label, sizeof(kind_label), "%s", a->kind);
      if (kind_label[0] >= 'a' && kind_label[0] <= 'z') {
        kind_label[0] = (char)(kind_label[0] - 'a' + 'A');
      }

      drw_setscheme(drw, scheme[needs_input ? SchemeSel : SchemeNorm]);
      drw_text(drw, agent_row_pad, row_y, name_w, bh, 0, kind_label, 0);
      notif_format_relative_time(a->updated, time_buf, sizeof(time_buf));
      drw_text(drw, time_x, row_y, time_w, bh, 0, time_buf, 0);
      row_y += bh;

      for (li = 0; li < (size_t)label_lines; li++) {
        const char *line_text = li < nlines ? lines[li] : "";
        drw_text(drw, agent_row_pad, row_y, max_w, bh, 0, line_text, 0);
        row_y += bh;
      }

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, agent_row_pad, row_y, max_w, bh, 0, a->cwd, 0);
      row_y += bh;

      if (a->from_file && agent_row_layout_len < MAX_AGENTS) {
        AgentRowLayout *row = &agent_row_layout[agent_row_layout_len++];
        row_id = agent_hash_id(a->file_path);
        row->id = row_id;
        row->close_w = close_w;
        row->close_h = bh;
        row->close_x = close_x;
        row->close_y = card_y;
        drw_setscheme(drw, scheme[SchemeNorm]);
        drw_text(drw, row->close_x, row->close_y, row->close_w, row->close_h,
                 0, icon_close, 0);
      }

      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, agent_row_pad, card_y + card_h - 1, max_w, 1, 1, 0);
    }

    y += card_h;
  }

  drw_map(drw, agent_sidebar_win, 0, 0, agent_sidebar_w, panel_h);
}

static void agent_sidebar_open(Monitor *m) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }

  if (agent_sidebar_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;
    agent_sidebar_cur_x = m->wx + m->ww;
    agent_sidebar_win = XCreateWindow(
        dpy, root, agent_sidebar_cur_x, m->wy, agent_sidebar_w, m->wh, 1,
        DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, agent_sidebar_win, cursor[CurNormal]->cursor);
  }

  agent_sidebar_mon = m;
  agent_sidebar_visible = 1;
  agent_scroll_offset = 0;
  agents_refresh();

  agent_anim_from_x = agent_sidebar_cur_x;
  agent_anim_to_x = m->wx + m->ww - agent_sidebar_w;
  agent_anim_state = SidebarAnimOpening;
  gettimeofday(&agent_anim_start, NULL);

  agent_sidebar_position(m);
  draw_agent_sidebar();
  XMapRaised(dpy, agent_sidebar_win);
  drawbars();
}

static void agent_sidebar_close(void) {
  if (agent_sidebar_win == None || !agent_sidebar_visible) {
    return;
  }
  agent_anim_from_x = agent_sidebar_cur_x;
  agent_anim_to_x = agent_sidebar_mon
                        ? agent_sidebar_mon->wx + agent_sidebar_mon->ww
                        : agent_sidebar_cur_x;
  agent_anim_state = SidebarAnimClosing;
  gettimeofday(&agent_anim_start, NULL);
}

static void agent_sidebar_toggle(Monitor *m) {
  if (agent_sidebar_visible) {
    agent_sidebar_close();
  } else {
    agent_sidebar_open(m);
  }
}

static void agent_sidebar_advance_animation(void) {
  struct timeval now;
  long elapsed_ms;
  double t;
  int mon_y = agent_sidebar_mon ? agent_sidebar_mon->wy : 0;

  if (agent_anim_state == SidebarAnimIdle || agent_sidebar_win == None) {
    return;
  }

  gettimeofday(&now, NULL);
  elapsed_ms = (now.tv_sec - agent_anim_start.tv_sec) * 1000 +
              (now.tv_usec - agent_anim_start.tv_usec) / 1000;

  if (elapsed_ms >= agent_anim_duration_ms) {
    agent_sidebar_cur_x = agent_anim_to_x;
    XMoveWindow(dpy, agent_sidebar_win, agent_sidebar_cur_x, mon_y);
    if (agent_anim_state == SidebarAnimClosing) {
      XUnmapWindow(dpy, agent_sidebar_win);
      agent_sidebar_visible = 0;
    }
    agent_anim_state = SidebarAnimIdle;
    return;
  }

  t = (double)elapsed_ms / (double)agent_anim_duration_ms;
  t = 1.0 - pow(1.0 - t, 3.0);
  agent_sidebar_cur_x =
      agent_anim_from_x + (int)((agent_anim_to_x - agent_anim_from_x) * t);
  XMoveWindow(dpy, agent_sidebar_win, agent_sidebar_cur_x, mon_y);
}

static int agent_sidebar_handle_button(XButtonPressedEvent *ev) {
  size_t i;

  if (agent_sidebar_win == None ||
      (!agent_sidebar_visible && agent_anim_state == SidebarAnimIdle)) {
    return 0;
  }

  if (ev->window != agent_sidebar_win) {
    if (agent_sidebar_visible) {
      agent_sidebar_close();
    }
    return 0;
  }

  if (ev->button == Button4) {
    agent_scroll_offset -= 40;
    draw_agent_sidebar();
    return 1;
  }
  if (ev->button == Button5) {
    agent_scroll_offset += 40;
    draw_agent_sidebar();
    return 1;
  }
  if (ev->button != Button1) {
    return 1;
  }

  if (ev->y >= agent_clear_y && ev->y < agent_clear_y + agent_clear_h &&
      ev->x >= agent_clear_x && ev->x < agent_clear_x + agent_clear_w) {
    AgentStatus *a;
    for (a = agents; a; a = a->next) {
      if (a->from_file) {
        unlink(a->file_path);
      }
    }
    agents_refresh();
    draw_agent_sidebar();
    drawbars();
    return 1;
  }

  for (i = 0; i < agent_row_layout_len; i++) {
    AgentRowLayout *row = &agent_row_layout[i];
    if (ev->x >= row->close_x && ev->x < row->close_x + row->close_w &&
        ev->y >= row->close_y && ev->y < row->close_y + row->close_h) {
      AgentStatus *a;
      for (a = agents; a; a = a->next) {
        if (a->from_file && agent_hash_id(a->file_path) == row->id) {
          unlink(a->file_path);
          break;
        }
      }
      agents_refresh();
      draw_agent_sidebar();
      drawbars();
      return 1;
    }
  }

  return 1;
}

/* }}} agent sidebar */

static void lua_register_mwm_api(lua_State *L) {
  lua_pushcfunction(L, lua_print_to_client);
  lua_setglobal(L, "print");
  lua_newtable(L);
  lua_pushcfunction(L, lua_mwm_theme);
  lua_setfield(L, -2, "theme");
  lua_pushcfunction(L, lua_mwm_workspaces);
  lua_setfield(L, -2, "workspaces");
  lua_pushcfunction(L, lua_mwm_create_workspace);
  lua_setfield(L, -2, "create_workspace");
  lua_pushcfunction(L, lua_mwm_focus_workspace);
  lua_setfield(L, -2, "focus_workspace");
  lua_pushcfunction(L, lua_mwm_rename_workspace);
  lua_setfield(L, -2, "rename_workspace");
  lua_pushcfunction(L, lua_mwm_list_workspaces);
  lua_setfield(L, -2, "list_workspaces");
  lua_pushcfunction(L, lua_mwm_widget);
  lua_setfield(L, -2, "widget");
  lua_pushcfunction(L, lua_mwm_exec);
  lua_setfield(L, -2, "exec");
  lua_setglobal(L, "mwm");
  lua_pushcfunction(L, lua_mwm_focus_workspace);
  lua_setglobal(L, "focus_workspace");
}

static int load_config_from_lua(void) {
  char config_path[PATH_MAX];

  if (command_lua) {
    return 0;
  }
  command_lua = luaL_newstate();
  if (!command_lua) {
    fprintf(stderr, "mwm: failed to initialize Lua state\n");
    return -1;
  }

  luaL_openlibs(command_lua);
  lua_register_mwm_api(command_lua);
  get_theme_config_path(config_path, sizeof(config_path));
  if (config_path[0] == '\0' || access(config_path, F_OK) != 0) {
    return 0;
  }

  if (luaL_dofile(command_lua, config_path) != LUA_OK) {
    fprintf(stderr, "mwm: failed to load %s: %s\n", config_path,
            lua_tostring(command_lua, -1));
    return -1;
  }
  return 0;
}

static int load_theme_from_lua(void) {
  if (command_lua) {
    return 0;
  }
  return load_config_from_lua();
}

static int wm_eval_lua_buffer(const char *chunk, size_t len, int fd) {
  int status;
  int top;
  int i;

  if (!command_lua) {
    if (fd >= 0) {
      write_all(fd, "ERR no lua state\n", strlen("ERR no lua state\n"));
    } else if (wm_capture_buffer && wm_capture_buffer_size > 0) {
      snprintf(wm_capture_buffer, wm_capture_buffer_size, "ERR no lua state");
      wm_capture_buffer_len = strlen(wm_capture_buffer);
    }
    return -1;
  }
  top = lua_gettop(command_lua);
  wm_command_reply_fd = fd;
  status = luaL_loadbuffer(command_lua, chunk, len, "mwm-cli");
  if (status == LUA_OK) {
    status = lua_pcall(command_lua, 0, LUA_MULTRET, 0);
  }
  if (status != LUA_OK) {
    const char *err = lua_tostring(command_lua, -1);
    wm_output_append("ERR ", 4);
    wm_output_append(err, strlen(err));
    wm_output_append("\n", 1);
    lua_settop(command_lua, top);
    wm_command_reply_fd = -1;
    return -1;
  }
  if (lua_gettop(command_lua) > top) {
    for (i = top + 1; i <= lua_gettop(command_lua); i++) {
      size_t out_len;
      const char *text = luaL_tolstring(command_lua, i, &out_len);
      wm_output_append(text, out_len);
      wm_output_append("\n", 1);
      lua_pop(command_lua, 1);
    }
  }
  lua_settop(command_lua, top);
  wm_command_reply_fd = -1;
  return 0;
}

static int wm_command_handle_client(int fd) {
  char *buffer = NULL;
  size_t len = 0;
  size_t cap = 0;
  ssize_t nread;
  char chunk[4096];
  int rc;

  while ((nread = read(fd, chunk, sizeof(chunk))) > 0) {
    if (len + (size_t)nread + 1 > cap) {
      size_t next_cap = cap ? cap * 2 : 4096;
      char *next_buffer;
      while (next_cap < len + (size_t)nread + 1) {
        next_cap *= 2;
      }
      next_buffer = static_cast<char *>(realloc(buffer, next_cap));
      if (!next_buffer) {
        free(buffer);
        return -1;
      }
      buffer = next_buffer;
      cap = next_cap;
    }
    memcpy(buffer + len, chunk, (size_t)nread);
    len += (size_t)nread;
  }
  if (!buffer) {
    return 0;
  }
  buffer[len] = '\0';
  rc = wm_eval_lua_buffer(buffer, len, fd);
  free(buffer);
  return rc;
}

static int wm_command_server_init(void) {
  struct sockaddr_un addr;

  get_workspace_socket_path(wm_socket_path, sizeof(wm_socket_path));
  if (wm_socket_path[0] == '\0') {
    return -1;
  }
  wm_command_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (wm_command_fd < 0) {
    return -1;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", wm_socket_path);
  unlink(wm_socket_path);
  if (bind(wm_command_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
      listen(wm_command_fd, 16) != 0) {
    close(wm_command_fd);
    wm_command_fd = -1;
    return -1;
  }
  return 0;
}

static void wm_command_server_shutdown(void) {
  if (wm_command_fd >= 0) {
    close(wm_command_fd);
    wm_command_fd = -1;
  }
  if (wm_socket_path[0] != '\0') {
    unlink(wm_socket_path);
    wm_socket_path[0] = '\0';
  }
}
// }}} theme config

// {{{ void cleanup(void)
/*
 * Cleanup function to free all allocated resources
 * and restore the state of the X server.
 */
void cleanup(void) {
  Arg a = {.ull = ~0ULL};
  Layout foo = {"", NULL};
  Monitor *m;
  size_t i;
  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next) {
    while (m->stack) {
      unmanage(m->stack, 0);
    }
  }
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons) {
    cleanupmon(mons);
  }
  for (i = 0; i < CurLast; i++) {
    drw_cur_free(drw, cursor[i]);
  }
  freecolorscheme();
  if (systray) {
    while (systray->icons) {
      XUnmapWindow(dpy, systray->icons->win);
      XReparentWindow(dpy, systray->icons->win, root, 0, 0);
      removesystrayicon(systray->icons);
    }
    XSetSelectionOwner(dpy, netatom[NetSystemTray], None, CurrentTime);
    XDestroyWindow(dpy, systray->win);
    free(systray);
    systray = NULL;
  }
  if (slider_popup_win != None) {
    XDestroyWindow(dpy, slider_popup_win);
    slider_popup_win = None;
  }
  if (notif_sidebar_win != None) {
    XDestroyWindow(dpy, notif_sidebar_win);
    notif_sidebar_win = None;
  }
  while (notifications) {
    Notification *next = notifications->next;
    free(notifications);
    notifications = next;
  }
  if (todo_sidebar_win != None) {
    XDestroyWindow(dpy, todo_sidebar_win);
    todo_sidebar_win = None;
  }
  while (todos) {
    Todo *next = todos->next;
    free(todos);
    todos = next;
  }
  if (agent_sidebar_win != None) {
    XDestroyWindow(dpy, agent_sidebar_win);
    agent_sidebar_win = None;
  }
  agents_free_list();
  notif_dbus_shutdown();
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  if (command_lua) {
    lua_close(command_lua);
    command_lua = NULL;
  }
  wm_command_server_shutdown();
  for (i = 0; i < workspaces_len; i++) {
    free(workspaces[i].name);
  }
  free(workspaces);
  workspaces = NULL;
  workspaces_len = 0;
}
// }}} void cleanup(void)

// {{{ void cleanupmon(Monitor *mon)
void cleanupmon(Monitor *mon) {
  Monitor *m;

  if (mon == mons) {
    mons = mons->next;
  } else {
    for (m = mons; m && m->next != mon; m = m->next) {
      ;
    }
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  if (mon->widgetbarwin) {
    XUnmapWindow(dpy, mon->widgetbarwin);
    XDestroyWindow(dpy, mon->widgetbarwin);
  }
  free(mon);
}
// }}} void cleanupmon(Monitor *mon)

// {{{ void clientmessage(XEvent *e)
void clientmessage(XEvent *e) {
  Client *i;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);
  XWindowAttributes wa;

  if (showsystray && systray && cme->message_type == netatom[NetSystemTrayOP] &&
      cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
    if (!(i = wintosystrayicon(cme->data.l[2]))) {
      if (!XGetWindowAttributes(dpy, cme->data.l[2], &wa)) {
        return;
      }
      i = ecalloc_type<Client>();
      i->win = cme->data.l[2];
      i->mon = selmon;
      updatesystrayicongeom(i, wa.width, wa.height);
      i->next = systray->icons;
      systray->icons = i;
      XAddToSaveSet(dpy, i->win);
      XSelectInput(dpy, i->win,
                   StructureNotifyMask | PropertyChangeMask |
                       ResizeRedirectMask);
      XSetWindowBorderWidth(dpy, i->win, 0);
      XReparentWindow(dpy, i->win, systray->win, 0, 0);
      cme->window = i->win;
      cme->message_type = xatom[Xembed];
      cme->format = 32;
      cme->data.l[0] = CurrentTime;
      cme->data.l[1] = XEMBED_EMBEDDED_NOTIFY;
      cme->data.l[2] = 0;
      cme->data.l[3] = systray->win;
      cme->data.l[4] = 0;
      XSendEvent(dpy, i->win, False, NoEventMask, e);
      updatesystrayiconstate(i);
      updatesystray();
    }
    return;
  }
  if (!c) {
    return;
  }
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen] ||
        cme->data.l[2] == netatom[NetWMFullscreen]) {
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                        || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ &&
                            !c->isfullscreen)));
    }
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    if (c != selmon->sel && !c->isurgent) {
      seturgent(c, 1);
    }
  }
}
// }}} void clientmessage(XEvent *e)

// {{{ void configure(Client *c)
void configure(Client *c) {
  XConfigureEvent ce;
  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}
// }}} void configure(Client *c)

// {{{ void configurenotify(XEvent *e)
void configurenotify(XEvent *e) {
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next) {
          if (c->isfullscreen) {
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
          }
        }
        XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
        XMoveResizeWindow(dpy, m->widgetbarwin, m->wx, m->bby, m->ww, bh);
      }
      if (slider_popup_visible && slider_popup_mon) {
        widget_position_slider_popup(slider_popup_mon);
        draw_slider_popup();
      }
      if (notif_sidebar_win != None && notif_sidebar_mon) {
        if (notif_sidebar_visible && notif_anim_state == SidebarAnimIdle) {
          notif_sidebar_cur_x =
              notif_sidebar_mon->wx + notif_sidebar_mon->ww - notif_sidebar_w;
        }
        notif_sidebar_position(notif_sidebar_mon);
        draw_notif_sidebar();
      }
      if (todo_sidebar_win != None && todo_sidebar_mon) {
        if (todo_sidebar_visible && todo_anim_state == SidebarAnimIdle) {
          todo_sidebar_cur_x =
              todo_sidebar_mon->wx + todo_sidebar_mon->ww - todo_sidebar_w;
        }
        todo_sidebar_position(todo_sidebar_mon);
        draw_todo_sidebar();
      }
      if (agent_sidebar_win != None && agent_sidebar_mon) {
        if (agent_sidebar_visible && agent_anim_state == SidebarAnimIdle) {
          agent_sidebar_cur_x =
              agent_sidebar_mon->wx + agent_sidebar_mon->ww - agent_sidebar_w;
        }
        agent_sidebar_position(agent_sidebar_mon);
        draw_agent_sidebar();
      }
      updatesystray();
      focus(NULL);
      arrange(NULL);
    }
  }
}
// }}} void configurenotify(XEvent *e)

// {{{ void configurerequest(XEvent *e)
void configurerequest(XEvent *e) {
  Client *c;
  Client *i;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;
  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    updatesystray();
  } else if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth) {
      c->bw = ev->border_width;
    } else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX | CWY)) &&
          !(ev->value_mask & (CWWidth | CWHeight))) {
        configure(c);
      }
      if (ISVISIBLE(c)) {
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
      }
    } else {
      configure(c);
    }
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}
// }}} void configurerequest(XEvent *e)

// {{{ Monitor *createmon(void)
Monitor *createmon(void) {
  Monitor *m;
  m = ecalloc_type<Monitor>();
  m->tagset[0] = m->tagset[1] =
      workspaces_len ? workspace_mask_from_index(0) : 0;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  return m;
}
// }}} Monitor *createmon(void)

// {{{ void destroynotify(XEvent *e)
void destroynotify(XEvent *e) {
  Client *c;
  Client *i;
  XDestroyWindowEvent *ev = &e->xdestroywindow;
  if ((i = wintosystrayicon(ev->window))) {
    removesystrayicon(i);
    updatesystray();
  } else if ((c = wintoclient(ev->window))) {
    unmanage(c, 1);
  }
}
// }}} void destroynotify(XEvent *e)

// {{{ void detach(Client *c)
void detach(Client *c) {
  Client **tc;
  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next) {
    ;
  }
  *tc = c->next;
}
// }}} void detach(Client *c)

// {{{ void detachstack(Client *c)
void detachstack(Client *c) {
  Client **tc, *t;
  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext) {
    ;
  }
  *tc = c->snext;
  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext) {
      ;
    }
    c->mon->sel = t;
  }
}
// }}} void detachstack(Client *c)

// {{{ Monitor *dirtomon(int dir)
Monitor *dirtomon(int dir) {
  Monitor *m = NULL;
  if (dir > 0) {
    if (!(m = selmon->next)) {
      m = mons;
    }
  } else if (selmon == mons) {
    for (m = mons; m->next; m = m->next) {
      ;
    }
  } else {
    for (m = mons; m->next != selmon; m = m->next) {
      ;
    }
  }
  return m;
}
// }}} Monitor *dirtomon(int dir)

// {{{ void drawbar(Monitor *m)
void drawbar(Monitor *m) {
  int x, w, tw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  char prompt_display[sizeof(command_prompt) + 16];
  const char *center_text = NULL;
  size_t i;
  WorkspaceMask occ = 0, urg = 0;
  unsigned int trayw = 0;
  Client *c;
  if (!m->showbar) {
    return;
  }
  /* draw status first so it can be overdrawn by tags later */
  if (m == selmon) { /* status is only drawn on selected monitor */
    drw_setscheme(drw, scheme[SchemeNorm]);
    trayw = showsystray && systray ? getsystraywidth() : 0;
    tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
    drw_text(drw, m->ww - tw - trayw, 0, tw, bh, 0, stext, 0);
  }
  for (c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent) {
      urg |= c->tags;
    }
  }
  x = 0;
  for (i = 0; i < workspace_count_visible(); i++) {
    WorkspaceMask mask = workspace_mask_from_index(i);
    Workspace *ws = workspace_by_index(i);
    if (!ws) {
      continue;
    }
    w = TEXTW(ws->name);
    drw_setscheme(drw,
                  scheme[m->tagset[m->seltags] & mask ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, ws->name, urg & mask);
    if (occ & mask) {
      drw_rect(drw, x + boxs, boxs, boxw, boxw,
               m == selmon && selmon->sel && (selmon->sel->tags & mask),
               urg & mask);
    }
    x += w;
  }
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
  if ((w = m->ww - tw - trayw - x) > bh) {
    if (m == selmon && command_prompt_active) {
      snprintf(prompt_display, sizeof(prompt_display), "lua> %s", command_prompt);
      center_text = prompt_display;
    } else if (m == selmon && command_prompt_feedback[0] != '\0') {
      center_text = command_prompt_feedback;
    } else if (m->sel) {
      center_text = m->sel->name;
    }
    if (center_text) {
      drw_setscheme(drw, scheme[m == selmon ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, center_text, 0);
      if (m->sel && !command_prompt_active && center_text == m->sel->name &&
          m->sel->isfloating) {
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
      }
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->ww, bh);
  if (m == selmon) {
    updatesystray();
  }
}
// }}} void drawbar(Monitor *m)

// {{{ void drawwidgetbar(Monitor *m)
void drawwidgetbar(Monitor *m) {
  char text[WidgetCount][64];
  size_t i;
  size_t n = sizeof(widget_draw_order) / sizeof(widget_draw_order[0]);
  int x;

  if (!m->showbar) {
    return;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, m->ww, bh, 1, 1);

  widget_format_battery(text[WidgetBattery], sizeof(text[WidgetBattery]));
  widget_format_backlight(text[WidgetBacklight], sizeof(text[WidgetBacklight]));
  widget_format_volume(text[WidgetVolume], sizeof(text[WidgetVolume]));
  widget_format_theme(text[WidgetTheme], sizeof(text[WidgetTheme]));
  widget_format_notifications(text[WidgetNotif], sizeof(text[WidgetNotif]));
  widget_format_todos(text[WidgetTodo], sizeof(text[WidgetTodo]));
  widget_format_agents(text[WidgetAgents], sizeof(text[WidgetAgents]));
  widget_format_cpu(text[WidgetCpu], sizeof(text[WidgetCpu]));
  widget_format_mem(text[WidgetMem], sizeof(text[WidgetMem]));
  widget_format_disk(text[WidgetDisk], sizeof(text[WidgetDisk]));
  widget_format_net(text[WidgetNet], sizeof(text[WidgetNet]));
  widget_format_clock(text[WidgetClock], sizeof(text[WidgetClock]));

  x = m->ww;
  for (i = 0; i < n; i++) {
    enum WidgetId id = widget_draw_order[i];
    int w = (int)TEXTW(text[id]);
    int highlighted = id == WidgetBacklight || id == WidgetVolume ||
                      id == WidgetTheme ||
                      (id == WidgetNotif && notification_unread > 0) ||
                      (id == WidgetTodo && todo_incomplete > 0) ||
                      (id == WidgetAgents && agent_needs_input > 0);

    if (i > 0) {
      x -= lrpad;
    }
    x -= w;
    widget_layout[id].x = x;
    widget_layout[id].w = w;

    drw_setscheme(drw, scheme[highlighted ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, text[id], 0);
  }

  for (i = 0; i < lua_widgets_len; i++) {
    LuaWidget *widget = &lua_widgets[i];
    int w = (int)TEXTW(widget->text);

    x -= lrpad;
    x -= w;
    widget->x = x;
    widget->w = w;

    drw_setscheme(drw, scheme[widget->highlight ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, widget->text, 0);
  }

  drw_map(drw, m->widgetbarwin, 0, 0, m->ww, bh);
}
// }}} void drawwidgetbar(Monitor *m)

// {{{ void drawbars(void)
void drawbars(void) {
  Monitor *m;
  for (m = mons; m; m = m->next) {
    drawbar(m);
    drawwidgetbar(m);
  }
  draw_slider_popup();
}
// }}} void drawbars(void)

// {{{ void enternotify(XEvent *e)
void enternotify(XEvent *e) {
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;
  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) &&
      ev->window != root) {
    return;
  }
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel) {
    return;
  }
  focus(c);
}
// }}} void enternotify(XEvent *e)

// {{{ void expose(XEvent *e)
void expose(XEvent *e) {
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;
  if (slider_popup_visible && ev->window == slider_popup_win) {
    draw_slider_popup();
    return;
  }
  if (notif_sidebar_win != None && ev->window == notif_sidebar_win) {
    draw_notif_sidebar();
    return;
  }
  if (todo_sidebar_win != None && ev->window == todo_sidebar_win) {
    draw_todo_sidebar();
    return;
  }
  if (agent_sidebar_win != None && ev->window == agent_sidebar_win) {
    draw_agent_sidebar();
    return;
  }
  if (ev->count == 0 && (m = wintomon(ev->window))) {
    if (ev->window == m->barwin) {
      drawbar(m);
    } else if (ev->window == m->widgetbarwin) {
      drawwidgetbar(m);
    }
  }
}
// }}} void expose(XEvent *e)

// {{{ void focus(Client *c)
void focus(Client *c) {
  if (!c || !ISVISIBLE(c)) {
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext) {
      ;
    }
  }
  if (selmon->sel && selmon->sel != c) {
    unfocus(selmon->sel, 0);
  }
  if (c) {
    if (c->mon != selmon) {
      selmon = c->mon;
    }
    if (c->isurgent) {
      seturgent(c, 0);
    }
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
}
// }}} void focus(Client *c)

// {{{ void focusin(XEvent *e)
/* there are some broken focus acquiring clients needing extra handling */
void focusin(XEvent *e) {
  XFocusChangeEvent *ev = &e->xfocus;
  if (selmon->sel && ev->window != selmon->sel->win) {
    setfocus(selmon->sel);
  }
}
// }}} void focusin(XEvent *e)

// {{{ void focusmon(const Arg *arg)
void focusmon(const Arg *arg) {
  Monitor *m;
  if (!mons->next) {
    return;
  }
  if ((m = dirtomon(arg->i)) == selmon) {
    return;
  }
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
}
// }}} void focusmon(const Arg *arg)

// {{{ void focusstack(const Arg *arg)
void focusstack(const Arg *arg) {
  Client *c = NULL, *i;
  if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen)) {
    return;
  }
  if (arg->i > 0) {
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next) {
      ;
    }
    if (!c) {
      for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next) {
        ;
      }
    }
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next) {
      if (ISVISIBLE(i)) {
        c = i;
      }
    }
    if (!c) {
      for (; i; i = i->next) {
        if (ISVISIBLE(i)) {
          c = i;
        }
      }
    }
  }
  if (c) {
    focus(c);
    restack(selmon);
  }
}
// }}} void focusstack(const Arg *arg)

// {{{ void focusdir(const Arg *arg)
void focusdir(const Arg *arg) {
  Client *best, *c, *sel;
  int selcx, selcy, cx, cy, primary, secondary;
  long bestscore, score;

  if (!(sel = selmon->sel) || (sel->isfullscreen && lockfullscreen)) {
    return;
  }

  selcx = sel->x + sel->w / 2;
  selcy = sel->y + sel->h / 2;
  best = NULL;
  bestscore = 0;

  for (c = selmon->clients; c; c = c->next) {
    if (c == sel || !ISVISIBLE(c)) {
      continue;
    }

    cx = c->x + c->w / 2;
    cy = c->y + c->h / 2;
    primary = 0;
    secondary = 0;

    switch (arg->i) {
    case DIR_LEFT:
      primary = selcx - cx;
      secondary = abs(selcy - cy);
      break;
    case DIR_DOWN:
      primary = cy - selcy;
      secondary = abs(selcx - cx);
      break;
    case DIR_UP:
      primary = selcy - cy;
      secondary = abs(selcx - cx);
      break;
    case DIR_RIGHT:
      primary = cx - selcx;
      secondary = abs(selcy - cy);
      break;
    default:
      return;
    }

    if (primary <= 0) {
      continue;
    }

    score = (long)primary * 10000L + (long)secondary;
    if (!best || score < bestscore) {
      best = c;
      bestscore = score;
    }
  }

  if (best) {
    focus(best);
    restack(selmon);
  }
}
// }}} void focusdir(const Arg *arg)

// {{{ Atom getatomprop(Client *c, Atom prop)
Atom getatomprop(Client *c, Atom prop) {
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;
  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
                         &da, &di, &dl, &dl, &p) == Success &&
      p) {
    atom = *(Atom *)p;
    XFree(p);
  }
  return atom;
}
// }}} Atom getatomprop(Client *c, Atom prop)

// {{{ unsigned int getsystraywidth(void)
unsigned int getsystraywidth(void) {
  unsigned int width = 0;
  Client *i;

  if (!showsystray || !systray) {
    return 0;
  }
  for (i = systray->icons; i; i = i->next) {
    width += i->w + systrayspacing;
  }
  return width ? width + systrayspacing : 0;
}
// }}} unsigned int getsystraywidth(void)

// {{{ int getrootptr(int *x, int *y)
int getrootptr(int *x, int *y) {
  int di;
  unsigned int dui;
  Window dummy;
  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}
// }}} int getrootptr(int *x, int *y)

// {{{ long getstate(Window w)
long getstate(Window w) {
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;
  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False,
                         wmatom[WMState], &real, &format, &n, &extra,
                         (unsigned char **)&p) != Success) {
    return -1;
  }
  if (n != 0) {
    result = *p;
  }
  XFree(p);
  return result;
}
// }}} long getstate(Window w)

// {{{ int gettextprop(Window w, Atom atom, char *text, unsigned int size)
int gettextprop(Window w, Atom atom, char *text, unsigned int size) {
  char **list = NULL;
  int n;
  XTextProperty name;
  if (!text || size == 0) {
    return 0;
  }
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems) {
    return 0;
  }
  if (name.encoding == XA_STRING) {
    strncpy(text, (char *)name.value, size - 1);
  } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success &&
             n > 0 && *list) {
    strncpy(text, *list, size - 1);
    XFreeStringList(list);
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}
// }}} int gettextprop(Window w, Atom atom, char *text, unsigned int size)

// {{{ void grabbuttons(Client *c, int focused)
void grabbuttons(Client *c, int focused) {
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, BUTTONMASK,
                  GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++) {
      if (buttons[i].click == ClkClientWin) {
        for (j = 0; j < LENGTH(modifiers); j++) {
          XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j],
                      c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync,
                      None, None);
        }
      }
    }
  }
}
// }}} void grabbuttons(Client *c, int focused)

// {{{ void grabkeys(void)
void grabkeys(void) {
  updatenumlockmask();
  {
    unsigned int i, j, k;
    unsigned int modifiers[] = {0, LockMask, numlockmask,
                                numlockmask | LockMask};
    int start, end, skip;
    KeySym *syms;
    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    XDisplayKeycodes(dpy, &start, &end);
    syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
    if (!syms) {
      return;
    }
    for (k = start; k <= end; k++) {
      for (i = 0; i < LENGTH(keys); i++) {
        /* skip modifier codes, we do that ourselves */
        if (keys[i].keysym == syms[(k - start) * skip]) {
          for (j = 0; j < LENGTH(modifiers); j++) {
            XGrabKey(dpy, k, keys[i].mod | modifiers[j], root, True,
                     GrabModeAsync, GrabModeAsync);
          }
        }
      }
    }
    XFree(syms);
  }
}
// }}} void grabkeys(void)

// {{{ void incnmaster(const Arg *arg)
void incnmaster(const Arg *arg) {
  selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}
// }}} void incnmaster(const Arg *arg)

static void command_prompt_stop(void) {
  command_prompt_active = 0;
  command_prompt_len = 0;
  command_prompt[0] = '\0';
  XUngrabKeyboard(dpy, CurrentTime);
  drawbars();
}

static void command_prompt_submit(void) {
  char *newline;

  command_prompt_feedback[0] = '\0';
  wm_capture_buffer = command_prompt_feedback;
  wm_capture_buffer_size = sizeof(command_prompt_feedback);
  wm_capture_buffer_len = 0;
  wm_eval_lua_buffer(command_prompt, command_prompt_len, -1);
  wm_capture_buffer = NULL;
  wm_capture_buffer_size = 0;
  wm_capture_buffer_len = 0;
  newline = strchr(command_prompt_feedback, '\n');
  if (newline) {
    *newline = '\0';
  }
  command_prompt_stop();
}

static int handle_command_prompt_keypress(XKeyEvent *ev) {
  char buf[64];
  KeySym keysym = NoSymbol;
  int len = XLookupString(ev, buf, sizeof(buf), &keysym, NULL);
  unsigned int state = CLEANMASK(ev->state);
  int i;

  switch (keysym) {
  case XK_Escape:
    command_prompt_stop();
    return 1;
  case XK_Return:
  case XK_KP_Enter:
    command_prompt_submit();
    return 1;
  case XK_BackSpace:
    if (command_prompt_len > 0) {
      command_prompt[--command_prompt_len] = '\0';
      drawbars();
    }
    return 1;
  default:
    break;
  }

  if (state == ControlMask && keysym == XK_u) {
    command_prompt_len = 0;
    command_prompt[0] = '\0';
    drawbars();
    return 1;
  }

  for (i = 0; i < len; i++) {
    if (!isprint((unsigned char)buf[i])) {
      continue;
    }
    if (command_prompt_len + 1 >= sizeof(command_prompt)) {
      break;
    }
    command_prompt[command_prompt_len++] = buf[i];
    command_prompt[command_prompt_len] = '\0';
  }
  if (len > 0) {
    drawbars();
    return 1;
  }
  return 0;
}

// {{{ int isuniquegeom(XineramaScreenInfo *unique, size_t n,
//                      XineramaScreenInfo *info)
#ifdef XINERAMA
static int isuniquegeom(XineramaScreenInfo *unique, size_t n,
                        XineramaScreenInfo *info) {
  while (n--) {
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org &&
        unique[n].width == info->width && unique[n].height == info->height) {
      return 0;
    }
  }
  return 1;
}
#endif /* XINERAMA */
// }}} int isuniquegeom()

// {{{ void keypress(XEvent *e)
void keypress(XEvent *e) {
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;
  ev = &e->xkey;
  if (command_prompt_active && handle_command_prompt_keypress(ev)) {
    return;
  }
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++) {
    if (keysym == keys[i].keysym &&
        CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].func) {
      keys[i].func(&(keys[i].arg));
    }
  }
}
// }}} void keypress(XEvent *e)

// {{{ void killclient(const Arg *arg)
void killclient(const Arg *arg) {
  if (!selmon->sel) {
    return;
  }
  if (!sendevent(selmon->sel, wmatom[WMDelete])) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}
// }}} void killclient(const Arg *arg)

// {{{ void manage(Window w, XWindowAttributes *wa)
void manage(Window w, XWindowAttributes *wa) {
  Client *c, *t = NULL;
  Window trans = None;
  XWindowChanges wc;
  WorkspaceMask restored_tags = 0;
  int has_restored_tags = 0;
  c = ecalloc_type<Client>();
  c->cfact = 1.0f;
  c->win = w;
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;
  updatetitle(c);
  has_restored_tags = client_restore_workspace_tags(w, &restored_tags);
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    c->tags = has_restored_tags ? restored_tags : t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
    if (has_restored_tags) {
      c->tags = restored_tags;
    }
  }
  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww) {
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  }
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh) {
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  }
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy);
  c->bw = borderpx;
  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  updatewmhints(c);
  XSelectInput(dpy, w,
               EnterWindowMask | FocusChangeMask | PropertyChangeMask |
                   StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating) {
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  }
  if (c->isfloating) {
    XRaiseWindow(dpy, c->win);
  }
  client_save_workspace_tags(c);
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                  PropModeAppend, (unsigned char *)&(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w,
                    c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if (c->mon == selmon) {
    unfocus(selmon->sel, 0);
  }
  c->mon->sel = c;
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  focus(NULL);
}
// }}} void manage(Window w, XWindowAttributes *wa)

// {{{ void mappingnotify(XEvent *e)
void mappingnotify(XEvent *e) {
  XMappingEvent *ev = &e->xmapping;
  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard) {
    grabkeys();
  }
}
// }}} void mappingnotify(XEvent *e)

// {{{ void maprequest(XEvent *e)
void maprequest(XEvent *e) {
  static XWindowAttributes wa;
  Client *i;
  XMapRequestEvent *ev = &e->xmaprequest;
  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayiconstate(i);
    updatesystray();
    return;
  }
  if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect) {
    return;
  }
  if (!wintoclient(ev->window)) {
    manage(ev->window, &wa);
  }
}
// }}} void maprequest(XEvent *e)

// {{{ void monocle(Monitor *m)
void monocle(Monitor *m) {
  unsigned int n = 0;
  Client *c;
  for (c = m->clients; c; c = c->next) {
    if (ISVISIBLE(c)) {
      n++;
    }
  }
  if (n > 0) { /* override layout symbol */
    snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
  }
  for (c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
  }
}
// }}} void monocle(Monitor *m)

// {{{ void motionnotify(XEvent *e)
void motionnotify(XEvent *e) {
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;
  if (slider_popup_handle_motion(ev)) {
    return;
  }
  if (ev->window != root) {
    return;
  }
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  mon = m;
}
// }}} void motionnotify(XEvent *e)

// {{{ void movemouse(const Arg *arg)
void movemouse(const Arg *arg) {
  int x, y, ocx, ocy, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel)) {
    return;
  }
  if (c->isfullscreen) { /* no support moving fullscreen windows by mouse */
    return;
  }
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess) {
    return;
  }
  if (!getrootptr(&x, &y)) {
    return;
  }
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
        continue;
      }
      lasttime = ev.xmotion.time;
      nx = ocx + (ev.xmotion.x - x);
      ny = ocy + (ev.xmotion.y - y);
      if (abs(selmon->wx - nx) < snap) {
        nx = selmon->wx;
      } else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap) {
        nx = selmon->wx + selmon->ww - WIDTH(c);
      }
      if (abs(selmon->wy - ny) < snap) {
        ny = selmon->wy;
      } else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap) {
        ny = selmon->wy + selmon->wh - HEIGHT(c);
      }
      if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
          (abs(nx - c->x) > snap || abs(ny - c->y) > snap)) {
        togglefloating(NULL);
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
        resize(c, nx, ny, c->w, c->h, 1);
      }
      break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}
// }}} void movemouse(const Arg *arg)

// {{{ Client *nexttiled(Client *c)
Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next) {
    ;
  }
  return c;
}
// }}} Client *nexttiled(Client *c)

// {{{ void pop(Client *c)
void pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}
// }}} void pop(Client *c)

void promptcommand(const Arg *arg) {
  (void)arg;
  if (command_prompt_active) {
    command_prompt_stop();
    return;
  }
  if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync,
                    CurrentTime) != GrabSuccess) {
    snprintf(command_prompt_feedback, sizeof(command_prompt_feedback),
             "ERR could not grab keyboard");
    drawbars();
    return;
  }
  command_prompt_feedback[0] = '\0';
  command_prompt_len = 0;
  command_prompt[0] = '\0';
  command_prompt_active = 1;
  drawbars();
}

// {{{ void propertynotify(XEvent *e)
void propertynotify(XEvent *e) {
  Client *c;
  Client *i;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;
  if ((ev->window == root) && (ev->atom == XA_WM_NAME)) {
    updatestatus();
  } else if (ev->state == PropertyDelete) {
    return; /* ignore */
  } else if ((i = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS || ev->atom == xatom[XembedInfo]) {
      updatesystrayiconstate(i);
      updatesystray();
    }
  } else if ((c = wintoclient(ev->window))) {
    switch (ev->atom) {
    default:
      break;
    case XA_WM_TRANSIENT_FOR:
      if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
          (c->isfloating = (wintoclient(trans)) != NULL)) {
        arrange(c->mon);
      }
      break;
    case XA_WM_NORMAL_HINTS:
      c->hintsvalid = 0;
      break;
    case XA_WM_HINTS:
      updatewmhints(c);
      drawbars();
      break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel) {
        drawbar(c->mon);
      }
    }
    if (ev->atom == netatom[NetWMWindowType]) {
      updatewindowtype(c);
    }
  }
}
// }}} void propertynotify(XEvent *e)

// {{{ void quit(const Arg *arg)
void quit(const Arg *arg) { running = 0; }
// }}} void quit(const Arg *arg)

// {{{ Monitor *recttomon(int x, int y, int w, int h)
Monitor *recttomon(int x, int y, int w, int h) {
  Monitor *m, *r = selmon;
  int a, area = 0;
  for (m = mons; m; m = m->next) {
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  }
  return r;
}
// }}} Monitor *recttomon(int x, int y, int w, int h)

// {{{ void removesystrayicon(Client *i)
void removesystrayicon(Client *i) {
  Client **ii;

  if (!systray || !i) {
    return;
  }
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next) {
    ;
  }
  if (*ii) {
    *ii = i->next;
  }
  free(i);
}
// }}} void removesystrayicon(Client *i)

// {{{ void resize(Client *c, int x, int y, int w, int h, int interact)
void resize(Client *c, int x, int y, int w, int h, int interact) {
  if (applysizehints(c, &x, &y, &w, &h, interact)) {
    resizeclient(c, x, y, w, h);
  }
}
// }}} void resize(Client *c, int x, int y, int w, int h, int interact)

// {{{ void resizebydir(const Arg *arg)
void resizebydir(const Arg *arg) {
  Client *c;
  unsigned int i, n, column_count;
  int step, w, h;
  int in_master, delta;
  float fact_step, next_cfact;
  float new_mfact;

  if (!(c = selmon->sel) || c->isfullscreen) {
    return;
  }

  if (!c->isfloating && selmon->lt[selmon->sellt]->arrange) {
    if (selmon->lt[selmon->sellt]->arrange != tile) {
      return;
    }

    n = tiledcount(selmon);
    if (n == 0) {
      return;
    }

    for (i = 0, in_master = 0, column_count = 0, c = nexttiled(selmon->clients);
         c; c = nexttiled(c->next), i++) {
      if (c != selmon->sel) {
        continue;
      }
      in_master = i < (unsigned int)selmon->nmaster;
      break;
    }
    if (!c) {
      return;
    }

    if (arg->i == DIR_LEFT || arg->i == DIR_RIGHT) {
      if (n <= (unsigned int)selmon->nmaster || selmon->nmaster <= 0) {
        return;
      }
      delta = ((arg->i == DIR_RIGHT) ? 1 : -1) * (in_master ? 1 : -1);
      new_mfact = selmon->mfact + (0.05f * delta);
      if (new_mfact < 0.05f || new_mfact > 0.95f) {
        return;
      }
      selmon->mfact = new_mfact;
      arrange(selmon);
      return;
    }

    for (i = 0, c = nexttiled(selmon->clients); c; c = nexttiled(c->next), i++) {
      if ((i < (unsigned int)selmon->nmaster) == in_master) {
        column_count++;
      }
    }
    if (column_count < 2) {
      return;
    }

    fact_step = 0.25f;
    next_cfact = selmon->sel->cfact +
                 ((arg->i == DIR_DOWN) ? fact_step : -fact_step);
    if (next_cfact < 0.25f || next_cfact > 4.0f) {
      return;
    }
    selmon->sel->cfact = next_cfact;
    arrange(selmon);
    return;
  }

  step = 32;
  w = c->w;
  h = c->h;

  switch (arg->i) {
  case DIR_LEFT:
    w -= step;
    break;
  case DIR_DOWN:
    h += step;
    break;
  case DIR_UP:
    h -= step;
    break;
  case DIR_RIGHT:
    w += step;
    break;
  default:
    return;
  }

  resize(c, c->x, c->y, MAX(w, 1), MAX(h, 1), 0);
}
// }}} void resizebydir(const Arg *arg)

// {{{ void resizeclient(Client *c, int x, int y, int w, int h)
void resizeclient(Client *c, int x, int y, int w, int h) {
  XWindowChanges wc;
  c->oldx = c->x;
  c->x = wc.x = x;
  c->oldy = c->y;
  c->y = wc.y = y;
  c->oldw = c->w;
  c->w = wc.width = w;
  c->oldh = c->h;
  c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(dpy, c->win, CWX | CWY | CWWidth | CWHeight | CWBorderWidth,
                   &wc);
  configure(c);
  XSync(dpy, False);
}
// }}} void resizeclient(Client *c, int x, int y, int w, int h)

// {{{ void resizemouse(const Arg *arg)
void resizemouse(const Arg *arg) {
  int ocx, ocy, nw, nh;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;
  if (!(c = selmon->sel)) {
    return;
  }
  if (c->isfullscreen) { /* no support resizing fullscreen windows by mouse */
    return;
  }
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
                   None, cursor[CurResize]->cursor,
                   CurrentTime) != GrabSuccess) {
    return;
  }
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  do {
    XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
    switch (ev.type) {
    case ConfigureRequest:
    case Expose:
    case MapRequest:
      handler[ev.type](&ev);
      break;
    case MotionNotify:
      if ((ev.xmotion.time - lasttime) <= (1000 / 60)) {
        continue;
      }
      lasttime = ev.xmotion.time;
      nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
      nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
      if (c->mon->wx + nw >= selmon->wx &&
          c->mon->wx + nw <= selmon->wx + selmon->ww &&
          c->mon->wy + nh >= selmon->wy &&
          c->mon->wy + nh <= selmon->wy + selmon->wh) {
        if (!c->isfloating && selmon->lt[selmon->sellt]->arrange &&
            (abs(nw - c->w) > snap || abs(nh - c->h) > snap)) {
          togglefloating(NULL);
        }
      }
      if (!selmon->lt[selmon->sellt]->arrange || c->isfloating) {
        resize(c, c->x, c->y, nw, nh, 1);
      }
      break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1,
               c->h + c->bw - 1);
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
    ;
  }
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}
// }}} void resizemouse(const Arg *arg)

// {{{ void reloadconfig(const Arg *arg)
void reloadconfig(const Arg *arg) {
  Monitor *m;

  if (command_lua) {
    lua_close(command_lua);
    command_lua = NULL;
  }
  reset_lua_widgets();

  set_default_bar_theme();
  load_config_from_lua();
  setcolorscheme();
  focus(NULL);
  for (m = mons; m; m = m->next) {
    arrange(m);
  }
  drawbars();
}
// }}} void reloadconfig(const Arg *arg)

// {{{ void restartwm(const Arg *arg)
void restartwm(const Arg *arg) {
  restart_requested = 1;
  running = 0;
}
// }}} void restartwm(const Arg *arg)

// {{{ void restack(Monitor *m)
void restack(Monitor *m) {
  Client *c;
  XEvent ev;
  XWindowChanges wc;
  drawbar(m);
  if (!m->sel) {
    return;
  }
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange) {
    XRaiseWindow(dpy, m->sel->win);
  }
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext) {
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling | CWStackMode, &wc);
        wc.sibling = c->win;
      }
    }
  }
  XSync(dpy, False);
  if (showsystray && systray) {
    XRaiseWindow(dpy, systray->win);
  }
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev)) {
    ;
  }
}
// }}} void restack(Monitor *m)

// {{{ void run(void)
/*
 * Main event loop
 *
 * Handles events from the X server.
 */
void run(void) {
  int xfd;
  int maxfd;
  int dbus_fd;
  fd_set readfds;
  XEvent ev;

  XSync(dpy, False);
  xfd = ConnectionNumber(dpy);
  while (running) {
    struct timeval tv;
    time_t now;

    now = time(NULL);
    if (widgets_last_refresh == 0 || now - widgets_last_refresh >= 2) {
      updatewidgets();
      notification_expire_check();
      notif_dbus_try_acquire();
      agents_refresh();
      if (agent_sidebar_visible) {
        draw_agent_sidebar();
      }
      widgets_last_refresh = now;
      drawbars();
    }
    FD_ZERO(&readfds);
    FD_SET(xfd, &readfds);
    maxfd = xfd;
    if (wm_command_fd >= 0) {
      FD_SET(wm_command_fd, &readfds);
      if (wm_command_fd > maxfd) {
        maxfd = wm_command_fd;
      }
    }
    dbus_fd = -1;
    if (notif_dbus_conn && dbus_connection_get_unix_fd(notif_dbus_conn, &dbus_fd) &&
        dbus_fd >= 0) {
      FD_SET(dbus_fd, &readfds);
      if (dbus_fd > maxfd) {
        maxfd = dbus_fd;
      }
    }
    if (notif_anim_state != SidebarAnimIdle || todo_anim_state != SidebarAnimIdle ||
        agent_anim_state != SidebarAnimIdle) {
      tv.tv_sec = 0;
      tv.tv_usec = 16000;
    } else {
      tv.tv_sec = 1;
      tv.tv_usec = 0;
    }
    if (select(maxfd + 1, &readfds, NULL, NULL, &tv) < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (wm_command_fd >= 0 && FD_ISSET(wm_command_fd, &readfds)) {
      int client_fd = accept(wm_command_fd, NULL, NULL);
      if (client_fd >= 0) {
        wm_command_handle_client(client_fd);
        close(client_fd);
      }
    }
    if (dbus_fd >= 0 && FD_ISSET(dbus_fd, &readfds)) {
      notif_dbus_process();
    }
    notif_sidebar_advance_animation();
    todo_sidebar_advance_animation();
    agent_sidebar_advance_animation();
    while (XPending(dpy)) {
      XNextEvent(dpy, &ev);
      if (handler[ev.type]) {
        handler[ev.type](&ev);
      }
    }
  }
}
// }}} void run(void)

// {{{ void scan(void)
/*
 * Scan existing windows on screen
 *
 * This function is called at startup to manage any windows that are
 * already open on the screen.
 *
 * Windows with the override_redirect attribute set are ignored (this
 * includes most docks, toolbars and desktop windows), as are windows
 * that have a transient parent.
 *
 * Windows are managed in two passes to ensure that any transient windows are
 * managed after their parent windows.
 *
 * Windows that are already mapped (visible) or have the iconic state
 * (minimized) are managed.
 */
void scan(void) {
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;
  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa) || wa.override_redirect ||
          XGetTransientForHint(dpy, wins[i], &d1)) {
        continue;
      }
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState) {
        manage(wins[i], &wa);
      }
    }
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa)) {
        continue;
      }
      if (XGetTransientForHint(dpy, wins[i], &d1) &&
          (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)) {
        manage(wins[i], &wa);
      }
    }
    if (wins) {
      XFree(wins);
    }
  }
}
// }}} void scan(void)

// {{{ void sendmon(Client *c, Monitor *m)
void sendmon(Client *c, Monitor *m) {
  if (c->mon == m) {
    return;
  }
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  client_save_workspace_tags(c);
  attach(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}
// }}} void sendmon(Client *c, Monitor *m)

// {{{ void setclientstate(Client *c, long state)
void setclientstate(Client *c, long state) {
  long data[] = {state, None};
  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
                  PropModeReplace, (unsigned char *)data, 2);
}
// }}} void setclientstate(Client *c, long state)

// {{{ int sendevent(Client *c, Atom proto)
int sendevent(Client *c, Atom proto) {
  int n;
  Atom *protocols;
  int exists = 0;
  XEvent ev;
  if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
    while (!exists && n--) {
      exists = protocols[n] == proto;
    }
    XFree(protocols);
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = c->win;
    ev.xclient.message_type = wmatom[WMProtocols];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = proto;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, c->win, False, NoEventMask, &ev);
  }
  return exists;
}
// }}} int sendevent(Client *c, Atom proto)

// {{{ void setfocus(Client *c)
void setfocus(Client *c) {
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow], XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&(c->win), 1);
  }
  sendevent(c, wmatom[WMTakeFocus]);
}
// }}} void setfocus(Client *c)

// {{{ void setfullscreen(Client *c, int fullscreen)
void setfullscreen(Client *c, int fullscreen) {
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&netatom[NetWMFullscreen],
                    1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}
// }}} void setfullscreen(Client *c, int fullscreen)

// {{{ void setlayout(const Arg *arg)
void setlayout(const Arg *arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt]) {
    selmon->sellt ^= 1;
  }
  if (arg && arg->v) {
    selmon->lt[selmon->sellt] = (Layout *)arg->v;
  }
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol,
          sizeof selmon->ltsymbol);
  if (selmon->sel) {
    arrange(selmon);
  } else {
    drawbar(selmon);
  }
}
// }}} void setlayout(const Arg *arg)

// {{{ void setmfact(const Arg *arg)
/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;
  if (!arg || !selmon->lt[selmon->sellt]->arrange) {
    return;
  }
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95) {
    return;
  }
  selmon->mfact = f;
  arrange(selmon);
}
// }}} void setmfact(const Arg *arg)

// {{{ unsigned int tiledcount(Monitor *m)
unsigned int tiledcount(Monitor *m) {
  unsigned int n;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++) {
    ;
  }
  return n;
}
// }}} unsigned int tiledcount(Monitor *m)

// {{{ void setup(void)
void setup(void) {
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;
  struct sigaction sa;
  char atomname[32];
  /* do not transform children into zombies when they terminate */
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
  sa.sa_handler = SIG_IGN;
  sigaction(SIGCHLD, &sa, NULL);
  /* clean up any zombies (inherited from .xinitrc etc) immediately */
  while (waitpid(-1, NULL, WNOHANG) > 0) {
    ;
  }
  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts))) {
    die("no fonts could be loaded.");
  }
  lrpad = drw->fonts->h;
  bh = drw->fonts->h + 2;
  load_theme_mode_state();
  set_default_bar_theme();
  workspace_set_defaults();
  load_theme_from_lua();
  notif_dbus_init();
  todo_seed_fake();
  get_agent_status_dir(agent_status_dir, sizeof(agent_status_dir));
  mkdir(agent_status_dir, 0700);
  agents_refresh();
  updatewidgets();
  widgets_last_refresh = time(NULL);
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  snprintf(atomname, sizeof atomname, "_NET_SYSTEM_TRAY_S%d", screen);
  netatom[NetSystemTray] = XInternAtom(dpy, atomname, False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] =
      XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] =
      XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] =
      XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] =
      XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  setcolorscheme();
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
                  PropModeReplace, (unsigned char *)"dwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
                  PropModeReplace, (unsigned char *)&wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
                  PropModeReplace, (unsigned char *)netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask |
                  ButtonPressMask | PointerMotionMask | EnterWindowMask |
                  LeaveWindowMask | StructureNotifyMask | PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  if (wm_command_server_init() != 0) {
    fprintf(stderr, "mwm: failed to initialize command socket at %s\n",
            wm_socket_path);
  }
  grabkeys();
  focus(NULL);
}
// }}} void setup(void)

// {{{ void seturgent(Client *c, int urg)
void seturgent(Client *c, int urg) {
  XWMHints *wmh;
  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win))) {
    return;
  }
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}
// }}} void seturgent(Client *c, int urg)

// {{{ void showhide(Client *c)
void showhide(Client *c) {
  if (!c) {
    return;
  }
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) &&
        !c->isfullscreen) {
      resize(c, c->x, c->y, c->w, c->h, 0);
    }
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}
// }}} void showhide(Client *c)

// {{{ void spawn(const Arg *arg)
void spawn(const Arg *arg) {
  struct sigaction sa;
  if (arg->v == dmenucmd) {
    dmenumon[0] = '0' + selmon->num;
  }
  if (fork() == 0) {
    if (dpy) {
      close(ConnectionNumber(dpy));
    }
    setsid();
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL;
    sigaction(SIGCHLD, &sa, NULL);
    execvp(((char **)arg->v)[0], (char **)arg->v);
    die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
  }
}
// }}} void spawn(const Arg *arg)

// {{{ void tag(const Arg *arg)
void tag(const Arg *arg) {
  WorkspaceMask mask = arg->ull & get_full_workspace_mask();
  if (selmon->sel && mask) {
    selmon->sel->tags = mask;
    client_save_workspace_tags(selmon->sel);
    focus(NULL);
    arrange(selmon);
  }
}
// }}} void tag(const Arg *arg)

// {{{ void tagmon(const Arg *arg)
void tagmon(const Arg *arg) {
  if (!selmon->sel || !mons->next) {
    return;
  }
  sendmon(selmon->sel, dirtomon(arg->i));
}
// }}} void tagmon(const Arg *arg)

// {{{ void tile(Monitor *m)
void tile(Monitor *m) {
  unsigned int i, n, h, mw, my, ty;
  float mfacts, sfacts, fact;
  Client *c;
  for (n = 0, mfacts = sfacts = 0.0f, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), n++) {
    if (n < (unsigned int)m->nmaster) {
      mfacts += c->cfact;
    } else {
      sfacts += c->cfact;
    }
  }
  if (n == 0) {
    return;
  }
  if (n > m->nmaster) {
    mw = m->nmaster ? m->ww * m->mfact : 0;
  } else {
    mw = m->ww;
  }
  for (i = my = ty = 0, c = nexttiled(m->clients); c;
       c = nexttiled(c->next), i++) {
    if (i < m->nmaster) {
      fact = mfacts > 0.0f ? c->cfact / mfacts : 1.0f;
      h = (unsigned int)((m->wh - my) * fact);
      resize(c, m->wx, m->wy + my, mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (my + HEIGHT(c) < m->wh) {
        my += HEIGHT(c);
      }
      mfacts -= c->cfact;
    } else {
      fact = sfacts > 0.0f ? c->cfact / sfacts : 1.0f;
      h = (unsigned int)((m->wh - ty) * fact);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw),
             h - (2 * c->bw), 0);
      if (ty + HEIGHT(c) < m->wh) {
        ty += HEIGHT(c);
      }
      sfacts -= c->cfact;
    }
  }
}
// }}} void tile(Monitor *m)

// {{{ void togglebar(const Arg *arg)
void togglebar(const Arg *arg) {
  selmon->showbar = !selmon->showbar;
  updatebarpos(selmon);
  XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww,
                    bh);
  XMoveResizeWindow(dpy, selmon->widgetbarwin, selmon->wx, selmon->bby,
                    selmon->ww, bh);
  updatesystray();
  arrange(selmon);
}
// }}} void togglebar(const Arg *arg)

// {{{ void togglefloating(const Arg *arg)
void togglefloating(const Arg *arg) {
  if (!selmon->sel) {
    return;
  }
  /* no support for fullscreen windows */
  if (selmon->sel->isfullscreen) {
    return;
  }
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating) {
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w,
           selmon->sel->h, 0);
  }
  arrange(selmon);
}
// }}} void togglefloating(const Arg *arg)

// {{{ void toggletag(const Arg *arg)
void toggletag(const Arg *arg) {
  WorkspaceMask newtags;
  if (!selmon->sel) {
    return;
  }
  newtags = selmon->sel->tags ^ (arg->ull & get_full_workspace_mask());
  if (newtags) {
    selmon->sel->tags = newtags;
    client_save_workspace_tags(selmon->sel);
    focus(NULL);
    arrange(selmon);
  }
}
// }}} void toggletag(const Arg *arg)

// {{{ void toggleview(const Arg *arg)
void toggleview(const Arg *arg) {
  WorkspaceMask newtagset =
      selmon->tagset[selmon->seltags] ^ (arg->ull & get_full_workspace_mask());

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;
    focus(NULL);
    arrange(selmon);
  }
}
// }}} void toggleview(const Arg *arg)

void tagnth(const Arg *arg) {
  Arg next = {.ull = workspace_mask_from_index((size_t)arg->i)};
  tag(&next);
}

void toggletagnth(const Arg *arg) {
  Arg next = {.ull = workspace_mask_from_index((size_t)arg->i)};
  toggletag(&next);
}

void toggleviewnth(const Arg *arg) {
  Arg next = {.ull = workspace_mask_from_index((size_t)arg->i)};
  toggleview(&next);
}

// {{{  void unfocus(Client *c, int setfocus)
void unfocus(Client *c, int setfocus) {
  if (!c) {
    return;
  }
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}
// }}}  void unfocus(Client *c, int setfocus)

// {{{ void unmanage(Client *c, int destroyed)
void unmanage(Client *c, int destroyed) {
  Monitor *m = c->mon;
  XWindowChanges wc;
  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XSelectInput(dpy, c->win, NoEventMask);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);
  focus(NULL);
  updateclientlist();
  arrange(m);
}
// }}} void unmanage(Client *c, int destroyed)

// {{{ void unmapnotify(XEvent *e)
void unmapnotify(XEvent *e) {
  Client *c;
  Client *i;
  XUnmapEvent *ev = &e->xunmap;
  if ((i = wintosystrayicon(ev->window))) {
    if (ev->send_event) {
      removesystrayicon(i);
      updatesystray();
    }
  } else if ((c = wintoclient(ev->window))) {
    if (ev->send_event) {
      setclientstate(c, WithdrawnState);
    } else {
      unmanage(c, 0);
    }
  }
}
// }}} void unmapnotify(XEvent *e)

// {{{ void updatebars(void)
void updatebars(void) {
  Monitor *m;
  XSetWindowAttributes wa = {};
  XClassHint ch = {};
  XEvent ev = {0};
  unsigned long orient = 0;
  wa.override_redirect = True;
  wa.background_pixmap = ParentRelative;
  wa.event_mask = ButtonPressMask | ExposureMask;
  ch.res_name = const_cast<char *>("dwm");
  ch.res_class = const_cast<char *>("dwm");
  for (m = mons; m; m = m->next) {
    if (m->barwin) {
      if (!m->widgetbarwin) {
        m->widgetbarwin = XCreateWindow(
            dpy, root, m->wx, m->bby, m->ww, bh, 0, DefaultDepth(dpy, screen),
            CopyFromParent, DefaultVisual(dpy, screen),
            CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
        XDefineCursor(dpy, m->widgetbarwin, cursor[CurNormal]->cursor);
        XMapRaised(dpy, m->widgetbarwin);
        XSetClassHint(dpy, m->widgetbarwin, &ch);
      }
      continue;
    }
    m->barwin = XCreateWindow(
        dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    m->widgetbarwin = XCreateWindow(
        dpy, root, m->wx, m->bby, m->ww, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    XDefineCursor(dpy, m->widgetbarwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, m->barwin);
    XMapRaised(dpy, m->widgetbarwin);
    XSetClassHint(dpy, m->barwin, &ch);
    XSetClassHint(dpy, m->widgetbarwin, &ch);
  }
  if (showsystray && !systray) {
    systray = ecalloc_type<Systray>();
    systray->win = XCreateSimpleWindow(dpy, root, 0, 0, 1, bh, 0, 0,
                                       scheme[SchemeNorm][ColBg].pixel);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation],
                    XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&orient, 1);
    XSelectInput(dpy, systray->win, ButtonPressMask | ExposureMask);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      ev.xclient.type = ClientMessage;
      ev.xclient.window = root;
      ev.xclient.message_type = xatom[Manager];
      ev.xclient.format = 32;
      ev.xclient.data.l[0] = CurrentTime;
      ev.xclient.data.l[1] = netatom[NetSystemTray];
      ev.xclient.data.l[2] = systray->win;
      ev.xclient.data.l[3] = 0;
      ev.xclient.data.l[4] = 0;
      XSendEvent(dpy, root, False, StructureNotifyMask, &ev);
    }
  }
}
// }}} void updatebars(void)

// {{{ void updatebarpos(Monitor *m)
void updatebarpos(Monitor *m) {
  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= 2 * bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    m->bby = m->topbar ? (m->my + m->mh - bh) : m->my;
    m->wy = m->my + bh;
  } else {
    m->by = -bh;
    m->bby = -bh;
  }
}
// }}} void updatebarpos(Monitor *m)

// {{{ void updateclientlist(void)
void updateclientlist() {
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32,
                      PropModeAppend, (unsigned char *)&(c->win), 1);
    }
  }
}
// }}} void updateclientlist(void)

// {{{ int updategeom(void)
int updategeom(void) {
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;
    for (n = 0, m = mons; m; m = m->next, n++) {
      ;
    }
    /* only consider unique geometries as separate screens */
    unique = ecalloc_type<XineramaScreenInfo>(nn);
    for (i = 0, j = 0; i < nn; i++) {
      if (isuniquegeom(unique, j, &info[i])) {
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
      }
    }
    XFree(info);
    nn = j;

    /* new monitors if nn > n */
    for (i = n; i < nn; i++) {
      for (m = mons; m && m->next; m = m->next) {
        ;
      }
      if (m) {
        m->next = createmon();
      } else {
        mons = createmon();
      }
    }
    for (i = 0, m = mons; i < nn && m; m = m->next, i++)
      if (i >= n || unique[i].x_org != m->mx || unique[i].y_org != m->my ||
          unique[i].width != m->mw || unique[i].height != m->mh) {
        dirty = 1;
        m->num = i;
        m->mx = m->wx = unique[i].x_org;
        m->my = m->wy = unique[i].y_org;
        m->mw = m->ww = unique[i].width;
        m->mh = m->wh = unique[i].height;
        updatebarpos(m);
      }
    /* removed monitors if n > nn */
    for (i = nn; i < n; i++) {
      for (m = mons; m && m->next; m = m->next) {
        ;
      }
      while ((c = m->clients)) {
        dirty = 1;
        m->clients = c->next;
        detachstack(c);
        c->mon = mons;
        attach(c);
        attachstack(c);
      }
      if (m == selmon) {
        selmon = mons;
      }
      cleanupmon(m);
    }
    free(unique);
  } else
#endif /* XINERAMA */
  {    /* default monitor setup */
    if (!mons) {
      mons = createmon();
    }
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}
// }}} int updategeom(void)

// {{{ void updatenumlockmask(void)
void updatenumlockmask(void) {
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++) {
    for (j = 0; j < modmap->max_keypermod; j++) {
      if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
          XKeysymToKeycode(dpy, XK_Num_Lock)) {
        numlockmask = (1 << i);
      }
    }
  }
  XFreeModifiermap(modmap);
}
// }}} void updatenumlockmask(void)

// {{{ void updatesystrayicongeom(Client *i, int w, int h)
void updatesystrayicongeom(Client *i, int w, int h) {
  if (!i) {
    return;
  }
  if (h <= 0) {
    h = bh;
  }
  if (w <= 0) {
    w = bh;
  }
  i->h = bh;
  i->w = h == bh ? w : (w * bh) / h;
  if (i->w <= 0) {
    i->w = bh;
  }
  if (i->w > bh * 2) {
    i->w = bh * 2;
  }
}
// }}} void updatesystrayicongeom(Client *i, int w, int h)

// {{{ void updatesystrayiconstate(Client *i)
void updatesystrayiconstate(Client *i) {
  int format;
  long flags = 0;
  int mapped = 1;
  unsigned long n, extra;
  unsigned char *p = NULL;
  Atom real;

  if (!i) {
    return;
  }
  if (XGetWindowProperty(dpy, i->win, xatom[XembedInfo], 0L, 2L, False,
                         xatom[XembedInfo], &real, &format, &n, &extra,
                         &p) == Success &&
      p) {
    flags = ((long *)p)[1];
    mapped = (flags & XEMBED_MAPPED) != 0;
    XFree(p);
  }
  if (mapped) {
    XMapRaised(dpy, i->win);
  } else {
    XUnmapWindow(dpy, i->win);
  }
}
// }}} void updatesystrayiconstate(Client *i)

// {{{ void updatesystray(void)
void updatesystray(void) {
  Client *i;
  unsigned int width, x;
  XWindowChanges wc;

  if (!showsystray || !systray) {
    return;
  }
  if (!selmon->showbar) {
    XUnmapWindow(dpy, systray->win);
    return;
  }

  width = getsystraywidth();
  if (width == 0) {
    XUnmapWindow(dpy, systray->win);
    return;
  }
  x = systrayspacing;
  for (i = systray->icons; i; i = i->next) {
    XMoveResizeWindow(dpy, i->win, x, (bh - i->h) / 2, i->w, i->h);
    x += i->w + systrayspacing;
  }

  wc.x = selmon->mx + selmon->ww - (int)width;
  wc.y = selmon->by;
  wc.width = width;
  wc.height = bh;
  wc.stack_mode = Above;
  wc.sibling = selmon->barwin;
  XConfigureWindow(dpy, systray->win,
                   CWX | CWY | CWWidth | CWHeight | CWSibling | CWStackMode,
                   &wc);
  XMapRaised(dpy, systray->win);
}
// }}} void updatesystray(void)

// {{{ void updatesizehints(Client *c)
void updatesizehints(Client *c) {
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize)) {
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  }
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else {
    c->basew = c->baseh = 0;
  }
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else {
    c->incw = c->inch = 0;
  }
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else {
    c->maxw = c->maxh = 0;
  }
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else {
    c->minw = c->minh = 0;
  }
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else {
    c->maxa = c->mina = 0.0;
  }
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
  c->hintsvalid = 1;
}
// }}} void updatesizehints(Client *c)

// {{{ void updatestatus(void)
void updatestatus(void) {
  if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext))) {
    strcpy(stext, " ");
  }
  drawbar(selmon);
}
// }}} void updatestatus(void)

// {{{ void updatetitle(Client *c)
void updatetitle(Client *c) {
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name)) {
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  }
  if (c->name[0] == '\0') { /* hack to mark broken clients */
    strcpy(c->name, broken);
  }
}
// }}} void updatetitle(Client *c)

// {{{ void updatewindowtype(Client *c)
void updatewindowtype(Client *c) {
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);
  if (state == netatom[NetWMFullscreen]) {
    setfullscreen(c, 1);
  }
  if (wtype == netatom[NetWMWindowTypeDialog]) {
    c->isfloating = 1;
  }
}
// }}} void updatewindowtype(Client *c)

// {{{ void updatewmhints(Client *c)
void updatewmhints(Client *c) {
  XWMHints *wmh;
  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else {
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    }
    if (wmh->flags & InputHint) {
      c->neverfocus = !wmh->input;
    } else {
      c->neverfocus = 0;
    }
    XFree(wmh);
  }
}
// }}} void updatewmhints(Client *c)

// {{{ void view(const Arg *arg)
void view(const Arg *arg) {
  workspace_view_mask(selmon, arg->ull & get_full_workspace_mask());
}
// }}} void view(const Arg *arg)

void viewnth(const Arg *arg) {
  Arg next = {.ull = workspace_mask_from_index((size_t)arg->i)};
  view(&next);
}

// {{{ Client *wintoclient(Window w)
Client *wintoclient(Window w) {
  Client *c;
  Monitor *m;
  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->win == w) {
        return c;
      }
    }
  }
  return NULL;
}
// }}} Client *wintoclient(Window w)

// {{{ Client *wintosystrayicon(Window w)
Client *wintosystrayicon(Window w) {
  Client *i;

  if (!systray) {
    return NULL;
  }
  for (i = systray->icons; i; i = i->next) {
    if (i->win == w) {
      return i;
    }
  }
  return NULL;
}
// }}} Client *wintosystrayicon(Window w)

// {{{ Monitor *wintomon(Window w)
/*
 * Set the W
 */
Monitor *wintomon(Window w) {
  int x, y;
  Client *c;
  Monitor *m;
  if (w == root && getrootptr(&x, &y)) {
    return recttomon(x, y, 1, 1);
  }
  // Iterate over each monitor (in linked list)
  for (m = mons; m; m = m->next) {
    // Return the monitor if the window matches the monitor's bar window
    // NOTE: barwin not in list of clients, so we check for it here
    if (w == m->barwin || w == m->widgetbarwin) {
      return m;
    }
  }
  if ((systray && w == systray->win) || wintosystrayicon(w)) {
    return selmon;
  }
  // Finds client based on this window id.
  if ((c = wintoclient(w))) {
    return c->mon;
  }
  return selmon;
}
// }}} Monitor *wintomon(Window w)

// {{{ int xerror(Display *dpy, XErrorEvent *ee)
/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int xerror(Display *dpy, XErrorEvent *ee) {
  if (ee->error_code == BadWindow ||
      (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch) ||
      (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolyFillRectangle &&
       ee->error_code == BadDrawable) ||
      (ee->request_code == X_PolySegment && ee->error_code == BadDrawable) ||
      (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch) ||
      (ee->request_code == X_GrabButton && ee->error_code == BadAccess) ||
      (ee->request_code == X_GrabKey && ee->error_code == BadAccess) ||
      (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
          ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}
// }}} int xerror(Display *dpy, XErrorEvent *ee)

// {{{ int xerrordummy(Display *dpy, XErrorEvent *ee)
int xerrordummy(Display *dpy, XErrorEvent *ee) { return 0; }
// }}} int xerrordummy(Display *dpy, XErrorEvent *ee)

// {{{ int xerrorstart(Display *dpy, XErrorEvent *ee)
/* Startup Error handler to check if another window manager
 * is already running. */
int xerrorstart(Display *dpy, XErrorEvent *ee) {
  die("dwm: another window manager is already running");
  return -1;
}
// }}} int xerrorstart(Display *dpy, XErrorEvent *ee)

// {{{  void zoom(const Arg *arg)
void zoom(const Arg *arg) {
  Client *c = selmon->sel;
  if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating) {
    return;
  }
  if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next))) {
    return;
  }
  pop(c);
}
// }}}  void zoom(const Arg *arg)

// }}} dwm

int main(int argc, char *argv[]) {
  saved_argv = argv;
  checkargs(argc, argv);
  checklocale();
  checkx11();
  checkotherwm();
  setup();
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);
  if (restart_requested) {
    execvp(saved_argv[0], saved_argv);
    die("mwm: execvp '%s' failed:", saved_argv[0]);
  }
  return EXIT_SUCCESS;
}
