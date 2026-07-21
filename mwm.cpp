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
#include <strings.h>
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
#include <X11/XKBlib.h>
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
  NetWMPid,
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
  XdndAwareAtom,
  XdndEnterAtom,
  XdndPositionAtom,
  XdndStatusAtom,
  XdndLeaveAtom,
  XdndDropAtom,
  XdndFinishedAtom,
  XdndSelectionAtom,
  XdndTypeListAtom,
  XdndActionCopyAtom,
  UriListAtom,
  XdndLast
}; /* XDND atoms */
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
  const char *desc; /* shown in the Mod+s keybinding-help overlay */
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
  /* Only meaningful when arrange == lua_layout_dispatch: which entry of
   * lua_layouts[] this Layout* stands in for. Compiled layouts leave this
   * zero-initialized and unread. */
  int lua_index;
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
  WidgetMic,
  WidgetMedia,
  WidgetTheme,
  WidgetNotif,
  WidgetTodo,
  WidgetAgents,
  WidgetGit,
  WidgetProjects,
  WidgetLoad,
  WidgetCpu,
  WidgetMem,
  WidgetDisk,
  WidgetNet,
  WidgetWifi,
  WidgetBluetooth,
  WidgetKbLayout,
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

#define MAX_LUA_KEYS 128

typedef struct {
  unsigned int mod;
  KeySym keysym;
  int callback_ref;
  char desc[128]; /* shown in the Mod+s keybinding-help overlay; may be empty */
} LuaKey;

/* One row of the Mod+s keybinding-help overlay -- built fresh from keys[]
 * and lua_keys[] every time it opens (see keybind_help_collect()), not
 * stored persistently. */
typedef struct {
  unsigned int mod;
  KeySym keysym;
  const char *desc;
} KeybindHelpEntry;

#define MAX_KEYBIND_HELP_ENTRIES 256

#define MAX_LUA_RULES 64

typedef struct {
  char class_name[64];
  char instance[64];
  char title[64];
  char workspace[128];
  int isfloating;
  int has_isfloating;
} LuaRule;

#define MAX_LUA_BUTTONS 64

typedef struct {
  int click; /* one of the Clk* enum values */
  unsigned int mod;
  unsigned int button;
  int callback_ref;
} LuaButton;

#define MAX_LUA_LAYOUTS 16
#define MAX_LUA_ARRANGE_CLIENTS 128

typedef struct {
  char name[32];
  char symbol[16];
  int arrange_ref;
} LuaLayout;

/* One entry per monitor, capturing which of its two layout slots (index by
 * m->lt[0]/m->lt[1]) held a Lua layout at reload time, by name -- see
 * snapshot_lua_layouts()/restore_lua_layouts(). */
typedef struct {
  int is_lua[2];
  char name[2][32];
} MonLayoutSnapshot;

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
  pid_t pid; /* 0 if not (yet) resolved against a live /proc process */
  AgentStatus *next;
};

typedef struct {
  unsigned int id;         /* hash of kind|cwd -- re-locates the agent to jump to it */
  unsigned int dismiss_id; /* hash of file_path, 0 when not dismissible */
  int card_y, card_h;
  int close_x, close_y, close_w, close_h;
} AgentRowLayout;

enum SidebarAnim { SidebarAnimIdle, SidebarAnimOpening, SidebarAnimClosing };

#define PICKER_QUERY_MAX 256

/* Visible rows in the popup at once -- not a cap on how many matches can be
 * browsed. project_filtered[] holds every match (up to MAX_PICK_CANDIDATES);
 * project_scroll picks which PROJECT_MAX_RESULTS-row window of it is on
 * screen, so Up/Down/Ctrl+P/N walk the whole list instead of wrapping
 * inside the first screenful (see project_ensure_sel_visible()). */
#define PROJECT_MAX_RESULTS 8
#define MAX_PROJECTS 500
/* Upper bound on candidates scored per keystroke: saved projects plus a
 * generous ceiling on $PATH-scanned executables (a few thousand on a large
 * system) -- also the storage cap for project_filtered[] now that it keeps
 * every match, not just the first screenful. */
#define MAX_PICK_CANDIDATES (MAX_PROJECTS + 4096)

enum PickResultKind { PickResultProject, PickResultApp, PickResultTheme };

/* What the picker is scoped to for this invocation -- Combined is the
 * original "search everything" behavior (still reachable via Mod+o);
 * ProjectOnly/AppOnly restrict it to one kind, like a dedicated project
 * switcher or a rofi-style drun launcher; Theme lists the named color
 * themes below. All four share the same picker window, filtering, and
 * keyboard handling. */
enum PickerMode {
  PickerModeCombined,
  PickerModeProjectOnly,
  PickerModeAppOnly,
  PickerModeTheme
};

typedef struct {
  int index; /* into projects[] or launcher_apps[], per kind */
  int score;
  enum PickResultKind kind;
} ProjectMatch;

typedef struct {
  int close_x, close_y, close_w, close_h;
} ProjectRowLayout;

#define MAX_DESKTOP_ICONS 500

typedef struct {
  char name[256]; /* basename within ~/Desktop; unused (empty) for the trash icon */
  int is_dir;
  int is_trash;
  int x, y, w, h; /* bounding box, recomputed on every draw for hit-testing */
} DesktopIcon;

#define MAX_WIFI_NETWORKS 64
#define MAX_WIFI_LIST_ROWS 8

typedef struct {
  char ssid[128];
  int signal; /* 0-100, -1 if nmcli didn't report one */
  int secured;
  int active; /* the currently-connected network */
} WifiNetwork;

/* A single nmcli invocation that might take real wall-clock time (a scan or
 * a connect attempt, both of which can take seconds) -- see wifi_job_start()
 * and wifi_job_poll(). Only one runs at a time; that's plenty for a popup
 * that's only ever driven by one person clicking one thing at a time. */
enum WifiJobKind { WifiJobNone, WifiJobScan, WifiJobConnect };

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
static void draw_vdivider(int x, int y, int h);
static void draw_hdivider(int x, int y, int w);
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
static int lua_mwm_agent_command(lua_State *L);
static int lua_mwm_create_workspace(lua_State *L);
static int lua_mwm_current_project(lua_State *L);
static int lua_mwm_exec(lua_State *L);
static int lua_mwm_focus_workspace(lua_State *L);
static int lua_mwm_list_projects(lua_State *L);
static int lua_mwm_list_workspaces(lua_State *L);
static int lua_mwm_rename_workspace(lua_State *L);
static int lua_mwm_switch_project(lua_State *L);
static int lua_mwm_theme(lua_State *L);
static int lua_mwm_widget(lua_State *L);
static int lua_mwm_workspaces(lua_State *L);
static void reset_lua_widgets(void);
static void widget_update_lua_widgets(void);
static void widget_lua_click(size_t index, int button);
static int lua_parse_mods(char **tokens, int count, unsigned int *mod_out);
static int lua_split_spec(char *buf, char **tokens, int max);
static int lua_parse_keybind(const char *spec, unsigned int *mod_out, KeySym *keysym_out);
static void lua_key_invoke(size_t index);
static int lua_mwm_keybind(lua_State *L);
static void reset_lua_keys(void);
static int lua_mwm_rule(lua_State *L);
static void reset_lua_rules(void);
static int lua_mwm_set_terminal(lua_State *L);
static int lua_mwm_set_mfact(lua_State *L);
static int lua_mwm_set_nmaster(lua_State *L);
static int lua_mwm_set_snap(lua_State *L);
static int lua_mwm_set_border_width(lua_State *L);
static int lua_mwm_set_gaps(lua_State *L);
static int lua_mwm_scratchpad_toggle(lua_State *L);
static int lua_mwm_scratchpad_set(lua_State *L);
static int lua_mwm_zoom(lua_State *L);
static int lua_mwm_toggle_floating(lua_State *L);
static int lua_mwm_kill_client(lua_State *L);
static int lua_mwm_toggle_bar(lua_State *L);
static int lua_parse_click_context(const char *name);
static int lua_parse_mousebind(const char *spec, unsigned int *mod_out, unsigned int *button_out);
static void lua_button_invoke(size_t index, const char *wsname);
static int lua_mwm_mousebind(lua_State *L);
static void reset_lua_buttons(void);
static void regrab_all_client_buttons(void);
static int lua_layout_index_by_symbol(const char *symbol);
static int lua_mwm_set_layout(lua_State *L);
static int lua_geom_field(lua_State *L, const char *field, int fallback);
static void lua_layout_arrange(Monitor *m, size_t layout_idx);
static void lua_layout_dispatch(Monitor *m);
static int lua_mwm_layout(lua_State *L);
static void reset_lua_layouts(void);
static int layout_is_lua(const Layout *lt);
static MonLayoutSnapshot *snapshot_lua_layouts(size_t *count_out);
static void restore_lua_layouts(MonLayoutSnapshot *snap, size_t count);
static size_t layout_total_count(void);
static const Layout *layout_at(size_t idx);
static void cycle_layout(int direction);
static void cyclelayout(const Arg *arg);
static int lua_mwm_cycle_layout(lua_State *L);

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
static unsigned int agent_row_id(const char *kind, const char *cwd);
static pid_t proc_ppid(pid_t pid);
static pid_t client_get_pid(Window w);
static Client *client_for_pid(pid_t target);
static void client_reveal(Client *c);
static void agent_jump_to(const AgentStatus *a);
static void agent_jump_next_needs_input(const Arg *arg);
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

/* {{{ launcher declarations (app index only -- see launcher globals above) */
static void launcher_scan_apps(void);
/* }}} launcher declarations */

/* {{{ project picker declarations */
static void get_projects_state_path(char *buf, size_t size);
static void projects_load(void);
static void projects_save(void);
static void projects_ensure_loaded(void);
static void project_expand_path(const char *raw, char *out, size_t outsz);
static int projects_add(const char *raw_path);
static void projects_remove_index(size_t idx);
static void projects_move_to_front(size_t idx);
static void project_collapse_home(const char *path, char *out, size_t outsz);
static int project_has_running_agent(const char *path);
static void project_basename(const char *path, char *out, size_t outsz);
static int project_find_for_path(const char *path);
static void project_label_for_path(const char *path, char *out, size_t outsz);
static int project_index_for_workspace_name(const char *wsname);
static void project_switch_workspace(Monitor *m, const char *path);
static void project_set_active(const char *path);
static void project_init_default(void);
static int project_resolve_selected_path(char *out, size_t outsz);
static void project_launch(int with_agent);
static void widget_format_projects(char *buf, size_t size);
static void project_filter(void);
static void project_ensure_sel_visible(void);
static void project_move_sel(int delta);
static int project_total_h(void);
static void project_position(Monitor *m);
static void project_draw(void);
static void project_open(Monitor *m, enum PickerMode mode);
static void project_close(void);
static void project_toggle(Monitor *m, enum PickerMode mode);
static void project_toggle_key(const Arg *arg);
static void project_submit(void);
static void project_submit_agent(void);
static void project_remove_selected(void);
static int project_handle_keypress(XKeyEvent *ev);
static int project_handle_button(XButtonPressedEvent *ev);
/* }}} project picker declarations */

/* {{{ keybinding help declarations */
static void keybind_help_mod_string(unsigned int mod, char *out, size_t outsz);
static void keybind_help_key_string(KeySym keysym, char *out, size_t outsz);
static size_t keybind_help_collect(KeybindHelpEntry *out, size_t cap);
static void keybind_help_open(Monitor *m);
static void keybind_help_close(void);
static void keybind_help_toggle(Monitor *m);
static void keybind_help_toggle_key(const Arg *arg);
static void keybind_help_draw(void);
static int keybind_help_handle_keypress(XKeyEvent *ev);
static int keybind_help_handle_button(XButtonPressedEvent *ev);
/* }}} keybinding help declarations */

/* {{{ wifi popup declarations */
static void wifi_job_reset(void);
static int wifi_job_start(enum WifiJobKind kind, char *const argv[], int capture_stderr);
static void wifi_job_poll(void);
static int nmcli_split_line(char *line, char *fields[], int max);
static int wifi_network_cmp(const void *a, const void *b);
static void wifi_parse_scan_output(char *buf);
static void wifi_start_scan(void);
static void wifi_start_connect(const char *ssid, const char *password);
static int wifi_popup_total_h(void);
static void wifi_popup_position(Monitor *m);
static void wifi_popup_draw(void);
static void wifi_popup_open(Monitor *m);
static void wifi_popup_close(void);
static void wifi_popup_toggle(Monitor *m);
static int wifi_handle_keypress(XKeyEvent *ev);
static int wifi_handle_button(XButtonPressedEvent *ev);
/* }}} wifi popup declarations */

/* {{{ desktop icons declarations */
static void get_desktop_dir(char *buf, size_t size);
static void get_trash_files_dir(char *buf, size_t size);
static void get_trash_info_dir(char *buf, size_t size);
static void get_desktop_layout_path(char *buf, size_t size);
static void desktop_layout_save(void);
static const char *desktop_icon_glyph(const DesktopIcon *icon);
static void get_trash_base_dir(char *buf, size_t size);
static void desktop_next_free_cell(const DesktopIcon *icons, size_t icons_len, int *out_x,
                                   int *out_y);
static void desktop_url_decode(const char *in, char *out, size_t outsz);
static void desktop_rescan(void);
static void desktop_draw(void);
static int desktop_hit_test(int x, int y);
static void desktop_open_icon(const DesktopIcon *icon);
static void desktop_trash_icon(size_t index);
static void desktop_dragmouse(size_t index);
static int desktop_handle_button(XButtonPressedEvent *ev);
static void desktop_setup(void);
static int desktop_copy_regular_file(const char *src, const char *dst, mode_t mode);
static int desktop_copy_path_recursive(const char *src, const char *dst);
static void desktop_xdnd_enter(XClientMessageEvent *cme);
static void desktop_xdnd_position(XClientMessageEvent *cme);
static void desktop_xdnd_leave(XClientMessageEvent *cme);
static void desktop_xdnd_drop(XClientMessageEvent *cme);
static void desktop_xdnd_selection_notify(XSelectionEvent *sev);
static int desktop_xdnd_import_uri_list(const char *data, int drop_x, int drop_y);
static void selectionnotify(XEvent *e);
/* }}} desktop icons declarations */

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
static void resize_cell(Client *c, int x, int y, int w, int h, int interact);
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
static int client_is_managed(Client *target);
static void scratchpad_hide(Client *c);
static void scratchpad_show(Client *c, Monitor *m);
static void scratchpad_set(const Arg *arg);
static void scratchpad_toggle(const Arg *arg);
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
static void widget_update_backlight(void);
static void widget_format_backlight(char *buf, size_t size);
static void widget_format_battery(char *buf, size_t size);
static void widget_format_clock(char *buf, size_t size);
static void widget_format_cpu(char *buf, size_t size);
static void widget_format_disk(char *buf, size_t size);
static void widget_format_mem(char *buf, size_t size);
static void widget_format_net(char *buf, size_t size);
static void widget_format_wifi(char *buf, size_t size);
static void widget_format_bluetooth(char *buf, size_t size);
static void widget_format_volume(char *buf, size_t size);
static void widget_format_mic(char *buf, size_t size);
static void widget_format_load(char *buf, size_t size);
static void widget_format_git(char *buf, size_t size);
static int run_capture_argv(const char *cwd, char *const argv[], char *out, size_t outsz,
                            int timeout_ms);
static void widget_amixer_read(const char *control, int *percent, int *muted);
static void widget_update_mic(void);
static void widget_toggle_mic_mute(void);
static void widget_update_load(void);
static long widget_ncpu(void);
static void widget_update_git(void);
static void widget_git_open_terminal(void);
static void widget_format_net_rate(double kbps, char *buf, size_t size);
static void widget_format_media(char *buf, size_t size);
static void widget_update_media(void);
static void widget_media_play_pause(void);
static void widget_media_next(void);
static void widget_media_previous(void);
static void widget_format_kblayout(char *buf, size_t size);
static void widget_refresh_kblayout_list(void);
static void widget_update_kblayout(void);
static void widget_cycle_kblayout(void);
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
static void run_detached_shell(const char *cmd);
static void widget_set_volume_percent_absolute(int percent);
static int widget_set_volume_percent(int delta_percent);
static void widget_toggle_volume_mute(void);
static void widget_update_cpu(void);
static void widget_update_disk(void);
static void widget_update_mem(void);
static void widget_update_net(void);
static void widget_update_wifi(void);
static void widget_update_bluetooth(void);
static int widget_rfkill_blocked(const char *type_name);
static void widget_toggle_bluetooth(void);
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
static int color_theme_find_by_name(const char *name);
static void color_theme_apply(size_t idx);
static void view(const Arg *arg);
static void viewnth(const Arg *arg);
static void tagnth(const Arg *arg);
static Client *wintoclient(Window w);
static Client *wintosystrayicon(Window w);
static Workspace *workspace_by_index(size_t index);
static size_t workspace_count_visible(void);
static size_t workspace_ensure(const char *name);
static size_t workspace_find(const char *name);
static size_t focus_workspace_by_name(Monitor *m, const char *name);
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
/* Pixels drawbar() reserves for the clock at the far right of the top bar,
 * past the systray -- updatesystray() shifts the tray left by this much so
 * the two never overlap. Set in drawbar() just before it calls
 * updatesystray(); 0 on any monitor that isn't selmon (no clock drawn there). */
static int clock_reserved_w = 0;
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
/* The one designated scratchpad client, if any -- see scratchpad_toggle().
 * Cleared by unmanage() when this client is closed, so it never dangles. */
static Client *scratchpad_client = NULL;
static const char scratchpad_workspace[] = "scratch";
static BarThemeConfig bar_theme_dark;
static BarThemeConfig bar_theme_light;
static enum ThemeMode theme_mode = ThemeModeSystem;
static int theme_resolved_dark = 1;
/* -1 means "no named theme picked -- just following theme_mode's plain
 * dark/light/system default"; otherwise an index into color_themes[] whose
 * colors were copied into both bar_theme_dark and bar_theme_light (a named
 * theme is one complete look, not a dark/light pair) by color_theme_apply(). */
static int active_color_theme = -1;
static LuaWidget lua_widgets[MAX_LUA_WIDGETS];
static size_t lua_widgets_len = 0;
static LuaKey lua_keys[MAX_LUA_KEYS];
static size_t lua_keys_len = 0;
static LuaRule lua_rules[MAX_LUA_RULES];
static size_t lua_rules_len = 0;
static LuaButton lua_buttons[MAX_LUA_BUTTONS];
static size_t lua_buttons_len = 0;
static LuaLayout lua_layouts[MAX_LUA_LAYOUTS];
static size_t lua_layouts_len = 0;
/* Layout* handles for the Lua layouts above, so setlayout()/m->lt[] can
 * point at something -- one static entry per lua_layouts[] slot, each
 * carrying its index via Layout.lua_index. */
static Layout lua_layout_entries[MAX_LUA_LAYOUTS];
static char terminal_cmd[64] = "kitty";

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
static unsigned int agent_last_jumped_id = 0;
/* }}} agent sidebar globals */

/* {{{ launcher globals */
/* The picker window itself is now project_*'s (see below) -- this is just
 * the $PATH-scanned app index it searches alongside saved projects, plus
 * the bar button that opens it. */
static char **launcher_apps = NULL;
static size_t launcher_apps_len = 0;
static size_t launcher_apps_cap = 0;
static int launcher_btn_x = 0;
static int launcher_btn_w = 0;
/* }}} launcher globals */

/* {{{ project picker globals */
static char **projects = NULL;
static size_t projects_len = 0;
static size_t projects_cap = 0;
static int projects_loaded = 0;
/* The project shown in drawbar()'s top-left corner -- independent of
 * whichever workspace/tag happens to be in view, so it doesn't disappear
 * just because you've switched to a plain numbered workspace. Always
 * non-empty: set to $HOME at every startup (see project_init_default() in
 * setup()) and updated whenever the picker or mwm.switch_project() actually
 * switches projects (project_set_active()). Deliberately not persisted --
 * every session starts on "default" again, per the user's request. */
static char active_project_path[PATH_MAX] = "";

static Window project_win = None;
static Monitor *project_mon = NULL;
static int project_visible = 0;
static enum PickerMode project_mode = PickerModeCombined;
static char project_query[PICKER_QUERY_MAX];
static size_t project_query_len = 0;
static ProjectMatch project_filtered[MAX_PICK_CANDIDATES];
static size_t project_filtered_len = 0;
static int project_sel = 0;
static size_t project_scroll = 0; /* index of the first visible row */
static const int project_w = 560;
static ProjectRowLayout project_row_layout[PROJECT_MAX_RESULTS];
static char agent_launch_command[64] = "claude";
/* }}} project picker globals */

/* {{{ keybinding help globals */
static Window keybind_help_win = None;
static int keybind_help_visible = 0;
static Monitor *keybind_help_mon = NULL;
/* }}} keybinding help globals */

/* {{{ desktop icons globals */
static Atom xdndatom[XdndLast];
static Window deskwin = None;
static DesktopIcon desktop_icons[MAX_DESKTOP_ICONS];
static size_t desktop_icons_len = 0;

static const int desktop_cell_w = 90;
static const int desktop_cell_h = 86;
static const int desktop_glyph_h = 40;
static const int desktop_margin = 12;

static int desktop_drag_active = 0;
static size_t desktop_drag_index = 0;
static int desktop_drag_x = 0;
static int desktop_drag_y = 0;

static struct timeval desktop_last_click_time;
static int desktop_last_click_index = -1;

/* xdnd target-side drag-in state */
static Window xdnd_source = None;
static int xdnd_version = 0;
static Atom xdnd_desired_type = None;
static int xdnd_last_x = 0, xdnd_last_y = 0;
static Window xdnd_pending_source = None;
/* }}} desktop icons globals */

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
static int mic_percent = -1;
static int mic_muted = 0;
static char mic_control[16] = "Capture";
static int mic_control_resolved = 0;
static unsigned long mic_next_probe_tick = 0;
static double load_avg1 = -1.0;
static char git_project_path[PATH_MAX];
static char git_branch[64];
static int git_dirty = 0;
static int git_ahead = 0;
static int git_behind = 0;
static int git_valid = 0;
static char git_last_project_path[PATH_MAX];
static unsigned long git_next_tick = 0;
static char media_status[16];
static char media_title[192];
#define KB_LAYOUT_MAX 8
static char kb_layout[16];
static char kb_layout_names[KB_LAYOUT_MAX][16];
static int kb_layout_count = 0;
static unsigned long kb_layout_list_next_tick = 0;
static long cpu_prev_total = -1;
static long cpu_prev_idle = -1;
static int cpu_percent = -1;
static int mem_percent = -1;
static int disk_percent = -1;
static char net_iface[64];
static int net_up = 0;
static char net_last_iface[64];
static long net_prev_rx = -1;
static long net_prev_tx = -1;
static time_t net_prev_time = 0;
static double net_rx_kbps = 0;
static double net_tx_kbps = 0;
static char wifi_iface[64];
static int wifi_blocked = 0;
static int wifi_up = 0;
static int wifi_quality = -1; /* 0-70 per /proc/net/wireless, -1 = unknown */
static char wifi_ssid[128];

/* {{{ wifi popup globals */
static const int wifi_popup_w = 320;
static Window wifi_win = None;
static int wifi_visible = 0;
static Monitor *wifi_mon = NULL;
static WifiNetwork wifi_networks[MAX_WIFI_NETWORKS];
static size_t wifi_networks_len = 0;
static int wifi_sel = 0;
static int wifi_scanning = 0;
static int wifi_connecting = 0;
static char wifi_connecting_ssid[128] = "";
static char wifi_status_msg[192] = "";

static int wifi_password_mode = 0;
static char wifi_password_ssid[128] = "";
static char wifi_password_query[128] = "";
static size_t wifi_password_query_len = 0;

static enum WifiJobKind wifi_job_kind = WifiJobNone;
static pid_t wifi_job_pid = -1;
static int wifi_job_fd = -1;
static int wifi_job_capture_stderr = 0;
static char wifi_job_buf[8192];
static size_t wifi_job_buf_len = 0;
/* }}} wifi popup globals */

static int bt_blocked = 0;
static int bt_connected_count = 0;
static char bt_first_device[128];
static time_t widgets_last_refresh = 0;
/* Incremented once per updatewidgets() call (~every 2s); lets individual
 * widgets throttle expensive work to less than every tick without each one
 * tracking its own wall-clock timer. */
static unsigned long widget_tick = 0;
static WidgetLayout widget_layout[WidgetCount];
/* WidgetClock is deliberately absent -- it's drawn directly in drawbar()
 * instead, at the far right of the top bar past the systray, rather than
 * among these in the bottom widget bar. */
static const enum WidgetId widget_draw_order[] = {
    WidgetBattery, WidgetBacklight, WidgetVolume,    WidgetMic,   WidgetMedia,
    WidgetTheme,   WidgetNotif,     WidgetTodo,      WidgetAgents, WidgetGit,
    WidgetProjects, WidgetLoad,     WidgetCpu,       WidgetMem,   WidgetDisk,
    WidgetNet,     WidgetWifi,      WidgetBluetooth, WidgetKbLayout,
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
  handlers[SelectionNotify] = selectionnotify;
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

/* Runs argv[0] with cwd (if given) via fork/exec -- no shell, so a project
 * path with odd characters (spaces, quotes) can't break out of a command
 * line the way it could through popen()/system(). Captures stdout, discards
 * stderr, returns whether the child exited 0.
 *
 * This runs on the main thread inside the X11 event loop, so a child that
 * hangs or just runs long (e.g. `git status` walking a huge or
 * network-mounted tree) would freeze the whole window manager for as long
 * as it takes. timeout_ms bounds that: past the deadline the child is
 * SIGKILLed and this reports failure, same as any other non-zero exit. */
static int run_capture_argv(const char *cwd, char *const argv[], char *out, size_t outsz,
                            int timeout_ms) {
  int pipefd[2];
  pid_t pid;
  int status;
  struct timespec start, now;
  size_t total = 0;
  int timed_out = 0;

  out[0] = '\0';
  if (pipe(pipefd) != 0) {
    return 0;
  }
  pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
  }
  if (pid == 0) {
    int devnull;
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    if (cwd && cwd[0] && chdir(cwd) != 0) {
      _exit(127);
    }
    execvp(argv[0], argv);
    _exit(127);
  }
  close(pipefd[1]);

  clock_gettime(CLOCK_MONOTONIC, &start);
  for (;;) {
    fd_set readfds;
    struct timeval tv;
    long elapsed_ms;
    long remaining_ms;
    int sr;
    ssize_t n;

    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed_ms = (now.tv_sec - start.tv_sec) * 1000 +
                (now.tv_nsec - start.tv_nsec) / 1000000;
    remaining_ms = timeout_ms - elapsed_ms;
    if (remaining_ms <= 0) {
      timed_out = 1;
      break;
    }

    FD_ZERO(&readfds);
    FD_SET(pipefd[0], &readfds);
    tv.tv_sec = remaining_ms / 1000;
    tv.tv_usec = (remaining_ms % 1000) * 1000;

    sr = select(pipefd[0] + 1, &readfds, NULL, NULL, &tv);
    if (sr <= 0) {
      if (sr < 0 && errno == EINTR) {
        continue;
      }
      timed_out = (sr == 0);
      break;
    }
    n = read(pipefd[0], out + total, outsz - 1 - total);
    if (n <= 0) {
      break; /* EOF, or a read error -- either way the child is done talking */
    }
    total += (size_t)n;
    if (total >= outsz - 1) {
      break;
    }
  }
  out[total] = '\0';
  close(pipefd[0]);

  /* Unconditional, not just on timeout: closing the read end above without
   * having drained everything (e.g. the buffer-full break) leaves the
   * child to discover that on its next write() -- normally via SIGPIPE,
   * but that's an assumption about the child's signal disposition this
   * function shouldn't have to make. Killing an already-exited pid here is
   * a harmless no-op, so there's no cost to always doing it. */
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);
  return !timed_out && WIFEXITED(status) && WEXITSTATUS(status) == 0;
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

/* Standalone (not folded into updatewidgets()) so the volume/backlight
 * slider popups can refresh just the one value they care about after a
 * drag motion event, instead of paying for every other widget's refresh
 * (several of which shell out or hit the network) on every mouse move --
 * see widget_set_backlight_percent_absolute(). */
static void widget_update_backlight(void) {
  long value;
  long max_value;

  backlight_percent = -1;
  if (read_long_from_file(backlight_brightness_path, &value) &&
      read_long_from_file(backlight_max_path, &max_value) && max_value > 0) {
    backlight_percent = (int)((value * 100 + (max_value / 2)) / max_value);
  }
}

static void updatewidgets(void) {
  FILE *fp;
  char status[64];
  long value;

  widget_tick++;
  widget_init_paths();

  widget_update_backlight();

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
  widget_update_wifi();
  widget_update_bluetooth();
  widget_update_volume();
  widget_update_mic();
  widget_update_media();
  widget_update_load();
  widget_update_git();
  widget_update_kblayout();
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
    net_rx_kbps = net_tx_kbps = 0;
    net_prev_time = 0;
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

  if (net_up) {
    char rx_path[160];
    char tx_path[160];
    long rx = -1, tx = -1;
    time_t now = time(NULL);

    snprintf(rx_path, sizeof(rx_path), "/sys/class/net/%s/statistics/rx_bytes", iface);
    snprintf(tx_path, sizeof(tx_path), "/sys/class/net/%s/statistics/tx_bytes", iface);
    if (read_long_from_file(rx_path, &rx) && read_long_from_file(tx_path, &tx)) {
      if (net_prev_time > 0 && now > net_prev_time &&
          strcmp(net_last_iface, iface) == 0 && rx >= net_prev_rx && tx >= net_prev_tx) {
        double dt = (double)(now - net_prev_time);
        net_rx_kbps = (double)(rx - net_prev_rx) / 1024.0 / dt;
        net_tx_kbps = (double)(tx - net_prev_tx) / 1024.0 / dt;
      } else {
        /* first sample, an interface change, or a counter reset */
        net_rx_kbps = net_tx_kbps = 0;
      }
      net_prev_rx = rx;
      net_prev_tx = tx;
      net_prev_time = now;
      snprintf(net_last_iface, sizeof(net_last_iface), "%s", iface);
    }
  } else {
    net_rx_kbps = net_tx_kbps = 0;
    net_prev_time = 0;
  }
}

/* Returns 1 if any rfkill device of the given type ("wlan"/"bluetooth") is
 * soft- or hard-blocked, 0 if none are, -1 if no device of that type exists. */
static int widget_rfkill_blocked(const char *type_name) {
  DIR *dp = opendir("/sys/class/rfkill");
  struct dirent *de;
  int found = 0;
  int blocked = 0;

  if (!dp) {
    return -1;
  }
  while ((de = readdir(dp)) != NULL) {
    char path[160];
    char type[32] = "";
    FILE *fp;
    long soft = 0, hard = 0;

    if (de->d_name[0] == '.') {
      continue;
    }
    snprintf(path, sizeof(path), "/sys/class/rfkill/%s/type", de->d_name);
    fp = fopen(path, "r");
    if (!fp) {
      continue;
    }
    if (fgets(type, sizeof(type), fp)) {
      type[strcspn(type, "\n")] = '\0';
    }
    fclose(fp);
    if (strcmp(type, type_name) != 0) {
      continue;
    }
    found = 1;
    snprintf(path, sizeof(path), "/sys/class/rfkill/%s/soft", de->d_name);
    read_long_from_file(path, &soft);
    snprintf(path, sizeof(path), "/sys/class/rfkill/%s/hard", de->d_name);
    read_long_from_file(path, &hard);
    if (soft || hard) {
      blocked = 1;
    }
  }
  closedir(dp);
  return found ? blocked : -1;
}

static void widget_update_wifi(void) {
  FILE *fp;
  char line[256];
  int blocked;

  wifi_iface[0] = '\0';
  wifi_up = 0;
  wifi_quality = -1;
  wifi_ssid[0] = '\0';

  blocked = widget_rfkill_blocked("wlan");
  wifi_blocked = blocked > 0;
  if (blocked < 0) {
    return; /* no wifi hardware */
  }

  fp = fopen("/proc/net/wireless", "r");
  if (fp) {
    if (fgets(line, sizeof(line), fp)) {
      /* discard header line 1 */
    }
    if (fgets(line, sizeof(line), fp)) {
      /* discard header line 2 */
    }
    if (fgets(line, sizeof(line), fp)) {
      char *colon = strchr(line, ':');
      if (colon) {
        int len = (int)(colon - line);
        double quality;
        char *p = line;
        if (len > 0 && len < (int)sizeof(wifi_iface)) {
          line[len] = '\0';
          while (*p == ' ') {
            p++;
          }
          snprintf(wifi_iface, sizeof(wifi_iface), "%s", p);
        }
        if (sscanf(colon + 1, "%*x %lf", &quality) == 1) {
          wifi_quality = (int)quality;
        }
      }
    }
    fclose(fp);
  }

  if (wifi_iface[0] != '\0' && !wifi_blocked) {
    char path[128];
    char state[32] = "";
    snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", wifi_iface);
    fp = fopen(path, "r");
    if (fp) {
      if (fgets(state, sizeof(state), fp)) {
        state[strcspn(state, "\n")] = '\0';
      }
      fclose(fp);
    }
    wifi_up = (strcmp(state, "up") == 0);

    if (wifi_up) {
      fp = popen("nmcli -t -f active,ssid dev wifi 2>/dev/null", "r");
      if (fp) {
        char buf[4096];
        size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
        char *saveptr = NULL;
        char *tok;
        pclose(fp);
        buf[len] = '\0';
        tok = strtok_r(buf, "\n", &saveptr);
        while (tok) {
          if (strncmp(tok, "yes:", 4) == 0) {
            snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", tok + 4);
            break;
          }
          tok = strtok_r(NULL, "\n", &saveptr);
        }
      }
    }
  }
}

static void widget_update_bluetooth(void) {
  FILE *fp;
  int blocked;

  bt_connected_count = 0;
  bt_first_device[0] = '\0';

  blocked = widget_rfkill_blocked("bluetooth");
  bt_blocked = blocked > 0;
  if (blocked < 0 || bt_blocked) {
    return;
  }

  fp = popen("bluetoothctl devices Connected 2>/dev/null", "r");
  if (fp) {
    char buf[4096];
    size_t len = fread(buf, 1, sizeof(buf) - 1, fp);
    char *saveptr = NULL;
    char *tok;
    pclose(fp);
    buf[len] = '\0';
    tok = strtok_r(buf, "\n", &saveptr);
    while (tok) {
      if (strncmp(tok, "Device ", 7) == 0) {
        if (bt_connected_count == 0) {
          const char *name = tok + 7;
          const char *name_start = strchr(name, ' ');
          snprintf(bt_first_device, sizeof(bt_first_device), "%s",
                   name_start ? name_start + 1 : name);
        }
        bt_connected_count++;
      }
      tok = strtok_r(NULL, "\n", &saveptr);
    }
  }
}

static void widget_toggle_bluetooth(void) {
  system(bt_blocked ? "rfkill unblock bluetooth" : "rfkill block bluetooth");
  widget_update_bluetooth();
  drawbars();
}

/* Shared by the Master (output) and Capture/Mic (input) widgets -- amixer's
 * "get" output has the same "[NN%] ... [on|off]" shape for both. */
static void widget_amixer_read(const char *control, int *percent, int *muted) {
  FILE *fp;
  char cmd[64];
  char buf[1024];
  size_t len;
  char *pct_end;
  char *digits_start;
  int pct;

  *percent = -1;
  *muted = 0;

  snprintf(cmd, sizeof(cmd), "amixer get %s 2>/dev/null", control);
  fp = popen(cmd, "r");
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
    *percent = MAX(0, MIN(100, pct));
    *muted = strstr(buf, "[off]") != NULL;
  }
}

static void widget_update_volume(void) {
  widget_amixer_read("Master", &volume_percent, &volume_muted);
}

/* Tries the common ALSA capture control names; most setups expose the mic
 * as "Capture", some HDA codecs use "Mic" instead. Once one resolves, stick
 * with it -- otherwise every tick would fork amixer twice forever just to
 * keep re-discovering the same answer. On a mic-less machine the "Mic"
 * fallback is only retried occasionally rather than on every tick, so it
 * doesn't cost two forks/tick indefinitely for a widget that's always
 * blank. */
static void widget_update_mic(void) {
  if (mic_control_resolved) {
    widget_amixer_read(mic_control, &mic_percent, &mic_muted);
    if (mic_percent >= 0) {
      return;
    }
    mic_control_resolved = 0; /* control disappeared -- reprobe below */
  }

  widget_amixer_read("Capture", &mic_percent, &mic_muted);
  if (mic_percent >= 0) {
    snprintf(mic_control, sizeof(mic_control), "Capture");
    mic_control_resolved = 1;
    return;
  }

  if (widget_tick < mic_next_probe_tick) {
    return;
  }
  mic_next_probe_tick = widget_tick + 15; /* ~30s at the 2s tick cadence */

  widget_amixer_read("Mic", &mic_percent, &mic_muted);
  if (mic_percent >= 0) {
    snprintf(mic_control, sizeof(mic_control), "Mic");
    mic_control_resolved = 1;
  }
}

static void widget_toggle_mic_mute(void) {
  char cmd[64];

  if (mic_percent < 0) {
    return;
  }
  snprintf(cmd, sizeof(cmd), "amixer -q set %s toggle", mic_control);
  system(cmd);
  widget_update_mic();
  drawbars();
}

static void widget_update_load(void) {
  FILE *fp;
  double l1, l5, l15;

  load_avg1 = -1.0;
  fp = fopen("/proc/loadavg", "r");
  if (!fp) {
    return;
  }
  if (fscanf(fp, "%lf %lf %lf", &l1, &l5, &l15) == 3) {
    load_avg1 = l1;
  }
  fclose(fp);
}

static long widget_ncpu(void) {
  static long n = 0;
  if (n <= 0) {
    n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n <= 0) {
      n = 1;
    }
  }
  return n;
}

/* Git status for the project backing the current workspace (same
 * current-project derivation as the projects bar widget). Leaves git_valid
 * false -- and the widget blank -- when the workspace isn't a saved
 * project's, or that project isn't a git repo. */
/* Returns the integer following the first occurrence of `marker` in `s`, or
 * 0 if `marker` doesn't appear -- used to pull "ahead N"/"behind N" out of
 * git's porcelain branch header. */
static int widget_git_int_after(const char *s, const char *marker) {
  const char *p = strstr(s, marker);
  if (!p) {
    return 0;
  }
  return atoi(p + strlen(marker));
}

static void widget_update_git(void) {
  const char *wsname = NULL;
  int idx;
  char out[512];
  char *nl;
  char *line;
  char header[400];
  const char *argv_status[] = {"git", "status", "--porcelain=v1", "--branch", NULL};

  if (!selmon) {
    git_valid = 0;
    git_project_path[0] = '\0';
    return;
  }
  wsname = workspace_name_from_mask(selmon->tagset[selmon->seltags]);
  idx = wsname ? project_index_for_workspace_name(wsname) : -1;
  if (idx < 0) {
    git_valid = 0;
    git_project_path[0] = '\0';
    git_last_project_path[0] = '\0';
    return;
  }

  /* git status is a fork+exec that scales with repo size and can stall
   * badly on a huge or network-mounted tree; run_capture_argv bounds any
   * single call, but there's no need to pay even the bounded cost every
   * 2s tick. Refresh immediately when the project changes (so switching
   * feels instant) and otherwise only every ~10s. */
  if (strcmp(projects[idx], git_last_project_path) == 0 && widget_tick < git_next_tick) {
    return;
  }
  git_next_tick = widget_tick + 5; /* ~10s at the 2s tick cadence */
  snprintf(git_last_project_path, sizeof(git_last_project_path), "%s", projects[idx]);

  git_valid = 0;
  git_branch[0] = '\0';
  git_dirty = 0;
  git_ahead = 0;
  git_behind = 0;
  git_project_path[0] = '\0';

  if (!run_capture_argv(projects[idx], (char *const *)argv_status, out, sizeof(out), 300)) {
    return; /* not a git repo, git isn't installed, or it timed out */
  }

  nl = strchr(out, '\n');
  if (nl) {
    git_dirty = nl[1] != '\0';
    *nl = '\0';
  }
  line = out;
  if (strncmp(line, "## ", 3) != 0) {
    return;
  }
  line += 3;
  /* branch-name truncation below mutates line in place, so grab the
   * ahead/behind counts from an untouched copy first */
  snprintf(header, sizeof(header), "%s", line);
  git_ahead = widget_git_int_after(header, "ahead ");
  git_behind = widget_git_int_after(header, "behind ");

  if (strncmp(line, "HEAD (no branch)", 16) == 0) {
    snprintf(git_branch, sizeof(git_branch), "detached");
  } else {
    char *end = line;
    while (*end && *end != '.' && *end != ' ') {
      end++;
    }
    *end = '\0';
    snprintf(git_branch, sizeof(git_branch), "%s", line);
  }
  snprintf(git_project_path, sizeof(git_project_path), "%s", projects[idx]);
  git_valid = 1;
}

static void widget_git_open_terminal(void) {
  Arg a = {0};
  const char *argv_term[4];

  if (!git_valid || git_project_path[0] == '\0') {
    return;
  }
  argv_term[0] = terminal_cmd;
  argv_term[1] = "-d";
  argv_term[2] = git_project_path;
  argv_term[3] = NULL;
  a.v = argv_term;
  spawn(&a);
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
static const char icon_launcher[] = "\U0000F135";
static const char icon_projects[] = "\U0000F07B";
static const char icon_desktop_folder[] = "\U0000F07B";
static const char icon_desktop_file[] = "\U0000F016";
static const char icon_desktop_image[] = "\U0000F03E";
static const char icon_desktop_archive[] = "\U0000F1C6";
static const char icon_desktop_audio[] = "\U0000F1C7";
static const char icon_desktop_video[] = "\U0000F1C8";
static const char icon_desktop_pdf[] = "\U0000F1C1";
static const char icon_desktop_text[] = "\U0000F0F6";
static const char icon_trash_empty[] = "\U0000F1F8";
static const char icon_trash_full[] = "\U0000F014";
static const char icon_wifi[] = "\U0000F1EB";
static const char icon_lock[] = "\U0000F023";
static const char icon_bluetooth[] = "\U0000F293";
static const char icon_mic[] = "\U0000F130";
static const char icon_mic_muted[] = "\U0000F131";
static const char icon_load[] = "\U0000F0E4";
static const char icon_git[] = "\U0000F126";
static const char icon_media[] = "\U0000F001";
static const char icon_keyboard[] = "\U0000F11C";
/* }}} widget icons */

/* MPRIS via playerctl -- it already does the DBus property/variant
 * unpacking for us, so there's no player running, or playerctl isn't
 * installed. Both leave media_status empty and blank the widget, matching
 * how the other optional-tool widgets (amixer, bluetoothctl, nmcli) behave
 * when their tool or device is absent.
 *
 * Status and title come out of a single playerctl call (its format string
 * supports {{status}} directly) rather than two -- one fork/tick instead of
 * two for something we already re-poll every 2s. */
static void widget_update_media(void) {
  FILE *fp;
  char buf[256];
  size_t len;
  char *sep;

  media_status[0] = '\0';
  media_title[0] = '\0';

  fp = popen("playerctl metadata --format '{{status}}|{{ artist }} - {{ title }}' 2>/dev/null",
             "r");
  if (!fp) {
    return;
  }
  len = fread(buf, 1, sizeof(buf) - 1, fp);
  pclose(fp);
  buf[len] = '\0';
  buf[strcspn(buf, "\n")] = '\0';
  if (buf[0] == '\0') {
    return; /* no MPRIS player running */
  }

  sep = strchr(buf, '|');
  if (!sep) {
    return;
  }
  *sep = '\0';
  snprintf(media_status, sizeof(media_status), "%s", buf);
  snprintf(media_title, sizeof(media_title), "%s", sep + 1);
}

static void widget_format_media(char *buf, size_t size) {
  if (media_status[0] == '\0') {
    buf[0] = '\0';
    return;
  }
  if (media_title[0] != '\0') {
    snprintf(buf, size, "%s %s", icon_media, media_title);
  } else {
    snprintf(buf, size, "%s", icon_media);
  }
}

static void widget_media_play_pause(void) {
  system("playerctl play-pause >/dev/null 2>&1");
  widget_update_media();
  drawbars();
}

static void widget_media_next(void) {
  system("playerctl next >/dev/null 2>&1");
  widget_update_media();
  drawbars();
}

static void widget_media_previous(void) {
  system("playerctl previous >/dev/null 2>&1");
  widget_update_media();
  drawbars();
}

/* The configured layout list (setxkbmap -query) essentially never changes
 * at runtime -- only re-fetch it occasionally rather than forking
 * setxkbmap every 2s tick just to re-derive the same answer. */
static void widget_refresh_kblayout_list(void) {
  FILE *fp;
  char layouts[256] = "";
  char line[256];
  char *tok;
  char *saveptr = NULL;
  int i;

  kb_layout_count = 0;

  fp = popen("setxkbmap -query 2>/dev/null", "r");
  if (!fp) {
    return;
  }
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "layout:", 7) == 0) {
      char *p = line + 7;
      while (*p == ' ' || *p == '\t') {
        p++;
      }
      p[strcspn(p, "\n")] = '\0';
      snprintf(layouts, sizeof(layouts), "%s", p);
      break;
    }
  }
  pclose(fp);

  if (layouts[0] == '\0') {
    return;
  }

  tok = strtok_r(layouts, ",", &saveptr);
  for (i = 0; tok && i < KB_LAYOUT_MAX; i++) {
    snprintf(kb_layout_names[i], sizeof(kb_layout_names[i]), "%s", tok);
    tok = strtok_r(NULL, ",", &saveptr);
  }
  kb_layout_count = i;
}

/* Active XKB group, mapped to a name via the cached layout list above.
 * XkbGetState is a native X11 call (no fork), so the displayed group stays
 * live every tick even though the list itself is only refreshed
 * occasionally. Blank when there's only one configured layout -- nothing to
 * indicate. */
static void widget_update_kblayout(void) {
  XkbStateRec state;
  int group;

  kb_layout[0] = '\0';

  if (!dpy || XkbGetState(dpy, XkbUseCoreKbd, &state) != Success) {
    return;
  }

  if (widget_tick >= kb_layout_list_next_tick) {
    widget_refresh_kblayout_list();
    kb_layout_list_next_tick = widget_tick + 30; /* ~60s at the 2s tick cadence */
  }

  group = state.group;
  if (group >= 0 && group < kb_layout_count) {
    snprintf(kb_layout, sizeof(kb_layout), "%s", kb_layout_names[group]);
  }
}

static void widget_format_kblayout(char *buf, size_t size) {
  if (kb_layout_count <= 1 || kb_layout[0] == '\0') {
    buf[0] = '\0';
    return;
  }
  snprintf(buf, size, "%s %s", icon_keyboard, kb_layout);
}

static void widget_cycle_kblayout(void) {
  XkbStateRec state;
  int next;

  if (!dpy || kb_layout_count <= 1) {
    return;
  }
  if (XkbGetState(dpy, XkbUseCoreKbd, &state) != Success) {
    return;
  }
  next = (state.group + 1) % kb_layout_count;
  XkbLockGroup(dpy, XkbUseCoreKbd, next);
  widget_update_kblayout();
  drawbars();
}

static void widget_format_backlight(char *buf, size_t size) {
  snprintf(buf, size, "%s", icon_backlight);
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
    icon = icon_volume_high;
  } else if (volume_muted || volume_percent == 0) {
    icon = icon_volume_muted;
  } else if (volume_percent < 50) {
    icon = icon_volume_low;
  } else {
    icon = icon_volume_high;
  }
  snprintf(buf, size, "%s", icon);
}

/* Blank (not "n/a") when there's no capture device at all -- unlike output
 * volume, plenty of desktops legitimately have no mic, and a permanent
 * "n/a" would just be noise. */
static void widget_format_mic(char *buf, size_t size) {
  if (mic_percent < 0) {
    buf[0] = '\0';
    return;
  }
  if (mic_muted) {
    snprintf(buf, size, "%s muted", icon_mic_muted);
    return;
  }
  snprintf(buf, size, "%s %d%%", icon_mic, mic_percent);
}

static void widget_format_load(char *buf, size_t size) {
  if (load_avg1 < 0) {
    snprintf(buf, size, "%s n/a", icon_load);
    return;
  }
  snprintf(buf, size, "%s %.2f", icon_load, load_avg1);
}

/* Blank outside a project workspace or a non-git project, same reasoning as
 * the mic widget -- most workspaces (mail, chat) have nothing to show. */
static void widget_format_git(char *buf, size_t size) {
  char counts[32] = "";

  if (!git_valid) {
    buf[0] = '\0';
    return;
  }
  if (git_ahead > 0 && git_behind > 0) {
    snprintf(counts, sizeof(counts), " %d↑%d↓", git_ahead, git_behind);
  } else if (git_ahead > 0) {
    snprintf(counts, sizeof(counts), " %d↑", git_ahead);
  } else if (git_behind > 0) {
    snprintf(counts, sizeof(counts), " %d↓", git_behind);
  }
  snprintf(buf, size, "%s %s%s%s", icon_git, git_branch, git_dirty ? "*" : "", counts);
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

static void widget_format_net_rate(double kbps, char *buf, size_t size) {
  if (kbps >= 1024.0) {
    snprintf(buf, size, "%.1fM", kbps / 1024.0);
  } else {
    snprintf(buf, size, "%.0fK", kbps);
  }
}

static void widget_format_net(char *buf, size_t size) {
  char rx_buf[16];
  char tx_buf[16];

  if (net_iface[0] == '\0') {
    snprintf(buf, size, "%s down", icon_net);
    return;
  }
  if (!net_up) {
    snprintf(buf, size, "%s %s down", icon_net, net_iface);
    return;
  }
  widget_format_net_rate(net_rx_kbps, rx_buf, sizeof(rx_buf));
  widget_format_net_rate(net_tx_kbps, tx_buf, sizeof(tx_buf));
  snprintf(buf, size, "%s ↓%s ↑%s", icon_net, rx_buf, tx_buf);
}

static void widget_format_wifi(char *buf, size_t size) {
  if (wifi_iface[0] == '\0') {
    snprintf(buf, size, "%s n/a", icon_wifi);
    return;
  }
  if (wifi_blocked) {
    snprintf(buf, size, "%s off", icon_wifi);
    return;
  }
  if (!wifi_up) {
    snprintf(buf, size, "%s down", icon_wifi);
    return;
  }
  if (wifi_ssid[0] != '\0') {
    snprintf(buf, size, "%s %s", icon_wifi, wifi_ssid);
    return;
  }
  snprintf(buf, size, "%s %s", icon_wifi, wifi_iface);
}

static void widget_format_bluetooth(char *buf, size_t size) {
  if (bt_blocked) {
    snprintf(buf, size, "%s off", icon_bluetooth);
    return;
  }
  if (bt_connected_count > 1) {
    snprintf(buf, size, "%s %s +%d", icon_bluetooth, bt_first_device,
             bt_connected_count - 1);
  } else if (bt_connected_count == 1) {
    snprintf(buf, size, "%s %s", icon_bluetooth, bt_first_device);
  } else {
    snprintf(buf, size, "%s", icon_bluetooth);
  }
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
  widget_update_backlight();
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
  widget_update_backlight();
  drawbars();
  draw_slider_popup();
  return 1;
}

/* Fire-and-forget: forks and execs `cmd` via /bin/sh -c, but -- unlike
 * system() -- never waits for it, so the caller isn't blocked on however
 * long the child takes to run. setup() ignores SIGCHLD with SA_NOCLDWAIT,
 * so the kernel reaps the child on its own; nothing here needs to wait()
 * it. Needed for anything that can fire many times a second, like the
 * volume slider while dragging -- system()'s blocking fork+exec+wait per
 * call was slow enough (tens of ms for amixer) that a fast drag queued up
 * a visible backlog of motion/release events, making the slider look like
 * it kept moving on its own for a moment after the button was actually
 * released. */
static void run_detached_shell(const char *cmd) {
  pid_t pid = fork();
  if (pid == 0) {
    if (dpy) {
      close(ConnectionNumber(dpy));
    }
    setsid();
    execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    _exit(127);
  }
}

static void widget_set_volume_percent_absolute(int percent) {
  char cmd[128];

  percent = MAX(0, MIN(100, percent));
  snprintf(cmd, sizeof(cmd), "amixer -q set Master %d%% unmute", percent);
  run_detached_shell(cmd);
  /* Optimistic: the amixer call above is fire-and-forget, so re-querying it
   * with an immediate widget_update_volume() would just race the still-
   * running child and often read back the stale value. We already know
   * what we asked for; the next periodic refresh (~2s) reconciles with
   * whatever amixer actually landed on. */
  volume_percent = percent;
  volume_muted = 0;
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

  if (ev->x >= launcher_btn_x && ev->x < launcher_btn_x + launcher_btn_w) {
    if (ev->button == Button1) {
      project_toggle(m, PickerModeCombined);
    }
    return 1;
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

  if (widget_hit(WidgetMic, ev->x)) {
    if (ev->button == Button1) {
      widget_toggle_mic_mute();
    }
    return 1;
  }

  if (widget_hit(WidgetMedia, ev->x)) {
    if (ev->button == Button1) {
      widget_media_play_pause();
      return 1;
    }
    if (ev->button == Button4) {
      widget_media_next();
      return 1;
    }
    if (ev->button == Button5) {
      widget_media_previous();
      return 1;
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
    if (ev->button == Button2) {
      project_toggle(m, PickerModeTheme);
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

  if (widget_hit(WidgetGit, ev->x)) {
    if (ev->button == Button1) {
      widget_git_open_terminal();
    }
    return 1;
  }

  if (widget_hit(WidgetProjects, ev->x)) {
    if (ev->button == Button1) {
      project_toggle(m, PickerModeProjectOnly);
    }
    return 1;
  }

  if (widget_hit(WidgetWifi, ev->x)) {
    if (ev->button == Button1) {
      wifi_popup_toggle(m);
    }
    return 1;
  }

  if (widget_hit(WidgetBluetooth, ev->x)) {
    if (ev->button == Button1) {
      widget_toggle_bluetooth();
    }
    return 1;
  }

  if (widget_hit(WidgetKbLayout, ev->x)) {
    if (ev->button == Button1) {
      widget_cycle_kblayout();
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

/* Interning this on every save/restore call cost a server round trip per
 * client -- with several clients open, GC'ing an empty workspace on switch
 * (which re-saves every client's tags, see workspace_remove_index) turned
 * into a burst of blocking round trips and made switching feel laggy. */
static Atom workspace_tags_atom(void) {
  static Atom atom = None;
  if (atom == None) {
    atom = XInternAtom(dpy, "_MWM_WORKSPACE_TAGS", False);
  }
  return atom;
}

static void client_save_workspace_tags(const Client *c) {
  Atom property;
  unsigned long data[2];

  if (!c) {
    return;
  }
  property = workspace_tags_atom();
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

  property = workspace_tags_atom();
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
static unsigned int borderpx = 1; /* border pixel of windows; overridable via mwm.set_border_width() */
static unsigned int snap = 32;          /* snap pixel; overridable via mwm.set_snap() */
static int gappx = 0; /* gap around every tiled client, all layouts; overridable via mwm.set_gaps() */
static const int showbar = 1;           /* 0 means no bar */
static const int showsystray = 1;       /* 0 means no system tray */
static const unsigned int systrayspacing = 6;
static const int topbar = 1;            /* 0 means bottom bar */
/* Second entry is a fallback used only for glyphs missing from the primary
 * font -- widget icons live in the Nerd Font Private Use Area ranges, so a
 * Nerd Font must be installed for them to render instead of tofu boxes. */
static const char *fonts[] = {"monospace:size=20", "UbuntuMono Nerd Font Mono:size=20"};
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

/* layout(s) -- overridable via mwm.set_mfact()/mwm.set_nmaster() */
static float mfact = 0.55; /* factor of master area size [0.05..0.95] */
static int nmaster = 1;    /* number of clients in master area */
static const int resizehints =
    1; /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen =
    1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
    /* symbol     arrange function */
    {"[]=", tile, -1}, /* first entry is default */
    {"><>", NULL, -1}, /* no layout function means floating behavior */
    {"[M]", monocle, -1},
};

/* key definitions */
#define MODKEY Mod4Mask
#define WORKSPACEKEYS(KEY, INDEX, LABEL)                                       \
  {MODKEY, KEY, viewnth, {.i = INDEX}, "View workspace " LABEL},               \
      {MODKEY | ControlMask, KEY, toggleviewnth, {.i = INDEX},                 \
       "Toggle workspace " LABEL " in view"},                                  \
      {MODKEY | ShiftMask, KEY, tagnth, {.i = INDEX},                          \
       "Move focused window to workspace " LABEL},                            \
      {MODKEY | ControlMask | ShiftMask, KEY, toggletagnth, {.i = INDEX},      \
       "Toggle focused window on workspace " LABEL},

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
/* termcmd[0] points at the mutable terminal_cmd buffer (declared with the
 * other Lua-config globals above) so mwm.set_terminal() takes effect here
 * too, without needing to rebuild this array. */
static const char *termcmd[] = {terminal_cmd, NULL};

static const Key keys[] = {
    /* modifier                     key        function        argument */
    /* One picker window (see "project picker" further down), four scopes:
     * Mod4+p is a rofi-style app launcher (search $PATH only), Mod4+space
     * jumps straight to a project, Mod4+o is the original combined search
     * across both, and Mod4+Shift+t (further down, by the layout keys)
     * browses color themes. */
    {MODKEY, XK_p, project_toggle_key, {.i = PickerModeAppOnly}, "Open app launcher"},
    {MODKEY, XK_Return, spawn, {.v = termcmd}, "Open a terminal"},
    {MODKEY, XK_o, project_toggle_key, {.i = PickerModeCombined},
     "Open combined project/app picker"},
    {MODKEY, XK_a, agent_jump_next_needs_input, {0}, "Jump to next agent needing input"},
    {MODKEY, XK_b, togglebar, {0}, "Toggle the bars"},
    {MODKEY, XK_r, reloadconfig, {0}, "Reload Lua config"},
    {MODKEY, XK_h, focusdir, {.i = DIR_LEFT}, "Focus window to the left"},
    {MODKEY, XK_j, focusdir, {.i = DIR_DOWN}, "Focus window below"},
    {MODKEY, XK_k, focusdir, {.i = DIR_UP}, "Focus window above"},
    {MODKEY, XK_l, focusdir, {.i = DIR_RIGHT}, "Focus window to the right"},
    {MODKEY | ShiftMask, XK_r, restartwm, {0}, "Restart mwm"},
    {MODKEY | ShiftMask, XK_h, resizebydir, {.i = DIR_LEFT}, "Grow/shrink window to the left"},
    {MODKEY | ShiftMask, XK_j, resizebydir, {.i = DIR_DOWN}, "Grow/shrink window downward"},
    {MODKEY | ShiftMask, XK_k, resizebydir, {.i = DIR_UP}, "Grow/shrink window upward"},
    {MODKEY | ShiftMask, XK_l, resizebydir, {.i = DIR_RIGHT}, "Grow/shrink window to the right"},
    {MODKEY | ControlMask, XK_j, focusstack, {.i = +1}, "Focus next window in stack"},
    {MODKEY | ControlMask, XK_k, focusstack, {.i = -1}, "Focus previous window in stack"},
    {MODKEY, XK_i, incnmaster, {.i = +1}, "Increase number of master windows"},
    {MODKEY, XK_d, incnmaster, {.i = -1}, "Decrease number of master windows"},
    {MODKEY | ControlMask, XK_h, setmfact, {.f = -0.05}, "Shrink master area"},
    {MODKEY | ControlMask, XK_l, setmfact, {.f = +0.05}, "Grow master area"},
    /* zoom (swap the focused client with master) previously had no
     * keyboard binding at all -- only the ClkWinTitle middle-click below,
     * which nobody discovers on their own. */
    {MODKEY, XK_z, zoom, {0}, "Swap focused window with master"},
    /* Scratchpad: designate a window (Shift+minus), then show/hide it from
     * anywhere with plain minus -- it keeps running in the background
     * between toggles, same as i3's scratchpad. */
    {MODKEY, XK_minus, scratchpad_toggle, {0}, "Show/hide scratchpad window"},
    {MODKEY | ShiftMask, XK_minus, scratchpad_set, {0}, "Designate focused window as scratchpad"},
    {MODKEY, XK_Tab, view, {0}, "View previous workspace"},
    {MODKEY | ShiftMask, XK_c, killclient, {0}, "Close focused window"},
    {MODKEY | ShiftMask, XK_colon, promptcommand, {0}, "Open Lua command prompt"},
    {MODKEY, XK_t, setlayout, {.v = &layouts[0]}, "Switch to tiled layout"},
    {MODKEY, XK_f, setlayout, {.v = &layouts[1]}, "Switch to floating layout"},
    {MODKEY, XK_m, setlayout, {.v = &layouts[2]}, "Switch to monocle layout"},
    /* Same picker, fourth scope: browse and apply the named color themes in
     * color_themes[] (also reachable by middle-clicking the theme widget). */
    {MODKEY | ShiftMask, XK_t, project_toggle_key, {.i = PickerModeTheme},
     "Open color theme picker"},
    /* Used to be setlayout({0})'s "toggle to the other layout" -- redundant
     * with Ctrl+Mod+space (cyclelayout) and Mod+t/f/m (pick one directly),
     * so it's free for the project picker instead. */
    {MODKEY, XK_space, project_toggle_key, {.i = PickerModeProjectOnly}, "Open project picker"},
    {MODKEY | ShiftMask, XK_space, togglefloating, {0}, "Toggle focused window floating"},
    {MODKEY | ControlMask, XK_space, cyclelayout, {.i = +1}, "Cycle to next layout"},
    {MODKEY | ControlMask | ShiftMask, XK_space, cyclelayout, {.i = -1},
     "Cycle to previous layout"},
    {MODKEY, XK_0, view, {.ull = ~0ULL}, "View all workspaces"},
    {MODKEY | ShiftMask, XK_0, tag, {.ull = ~0ULL}, "Put focused window on all workspaces"},
    {MODKEY, XK_comma, focusmon, {.i = -1}, "Focus previous monitor"},
    {MODKEY, XK_period, focusmon, {.i = +1}, "Focus next monitor"},
    {MODKEY | ShiftMask, XK_comma, tagmon, {.i = -1}, "Move focused window to previous monitor"},
    {MODKEY | ShiftMask, XK_period, tagmon, {.i = +1}, "Move focused window to next monitor"},
    /* Mod+s, mnemonic "show shortcuts" -- see keybind_help_toggle_key() and
     * the "keybinding help" section further down. */
    {MODKEY, XK_s, keybind_help_toggle_key, {0}, "Show this keybinding help"},
    WORKSPACEKEYS(XK_1, 0, "1") WORKSPACEKEYS(XK_2, 1, "2") WORKSPACEKEYS(XK_3, 2, "3")
        WORKSPACEKEYS(XK_4, 3, "4") WORKSPACEKEYS(XK_5, 4, "5")
            WORKSPACEKEYS(XK_6, 5, "6") WORKSPACEKEYS(XK_7, 6, "7")
                WORKSPACEKEYS(XK_8, 7, "8") WORKSPACEKEYS(XK_9, 8, "9"){
                    MODKEY | ShiftMask, XK_q, quit, {0}, "Quit mwm"},
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
  size_t li;
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

  /* mwm.rule() from Lua -- applied after the compiled-in rules[] above, so
   * a user's Lua config always has the final say for windows it matches. */
  for (li = 0; li < lua_rules_len; li++) {
    LuaRule *lr = &lua_rules[li];
    if ((!lr->class_name[0] || strstr(class_name, lr->class_name)) &&
        (!lr->instance[0] || strstr(instance, lr->instance)) &&
        (!lr->title[0] || strstr(c->name, lr->title))) {
      if (lr->has_isfloating) {
        c->isfloating = lr->isfloating;
      }
      if (lr->workspace[0]) {
        size_t widx = workspace_ensure(lr->workspace);
        if (widx != SIZE_MAX) {
          c->tags = workspace_mask_from_index(widx);
        }
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
  unsigned int statusw, trayw, clockw;

  click = ClkRootWin;
  if (keybind_help_handle_button(ev)) {
    return;
  }
  if (wifi_handle_button(ev)) {
    return;
  }
  if (project_handle_button(ev)) {
    return;
  }
  if (desktop_handle_button(ev)) {
    return;
  }
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
    char project_label[288];
    char base[256];

    i = 0;
    /* Must match drawbar()'s layout exactly: project label, then tags,
     * starting past it rather than at the left edge. */
    project_basename(active_project_path, base, sizeof(base));
    snprintf(project_label, sizeof(project_label), "%s %s", icon_projects, base);
    x = (int)TEXTW(project_label);
    statusw = TEXTW(stext);
    trayw = (showsystray && systray && selmon == m) ? getsystraywidth() : 0;
    clockw = (unsigned int)clock_reserved_w;
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
    } else if (ev->x > selmon->ww - (int)statusw - (int)trayw - (int)clockw) {
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
  for (i = 0; i < lua_buttons_len; i++) {
    if (click == (unsigned int)lua_buttons[i].click && lua_buttons[i].button == ev->button &&
        CLEANMASK(lua_buttons[i].mod) == CLEANMASK(ev->state)) {
      const char *wsname = click == ClkTagBar ? workspace_name_from_mask(arg.ull) : NULL;
      lua_button_invoke(i, wsname);
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

typedef struct {
  const char *name;
  int is_light;
  BarThemeConfig colors; /* {normal{fg,bg,border}, selected{fg,bg,border}} */
} ColorTheme;

/* Picked from the Mod+Shift+t / theme-widget-middle-click picker (see
 * PickerModeTheme in project_filter()/project_launch()) -- selecting one
 * copies .colors into both bar_theme_dark and bar_theme_light via
 * color_theme_apply(), since each entry here is one complete look rather
 * than a dark/light pair. "Default Dark"/"Default Light" are the original
 * hardcoded scheme from set_default_bar_theme(), kept as entries 0 and 1 so
 * they're reachable from the same list. */
static const ColorTheme color_themes[] = {
    {"Default Dark", 0, {{"#bbbbbb", "#222222", "#444444"}, {"#eeeeee", "#005577", "#005577"}}},
    {"Default Light", 1, {{"#24292f", "#f4f5f7", "#d0d5dd"}, {"#ffffff", "#2d6cdf", "#2d6cdf"}}},
    {"Dracula", 0, {{"#6272a4", "#282a36", "#44475a"}, {"#f8f8f2", "#bd93f9", "#bd93f9"}}},
    {"Nord", 0, {{"#4c566a", "#2e3440", "#3b4252"}, {"#2e3440", "#88c0d0", "#88c0d0"}}},
    {"Gruvbox Dark", 0, {{"#928374", "#282828", "#3c3836"}, {"#282828", "#fabd2f", "#fabd2f"}}},
    {"Gruvbox Light", 1, {{"#928374", "#fbf1c7", "#ebdbb2"}, {"#fbf1c7", "#d65d0e", "#d65d0e"}}},
    {"Solarized Dark", 0, {{"#586e75", "#002b36", "#073642"}, {"#002b36", "#268bd2", "#268bd2"}}},
    {"Solarized Light", 1, {{"#93a1a1", "#fdf6e3", "#eee8d5"}, {"#fdf6e3", "#268bd2", "#268bd2"}}},
    {"One Dark", 0, {{"#5c6370", "#282c34", "#3e4451"}, {"#282c34", "#61afef", "#61afef"}}},
    {"One Light", 1, {{"#a0a1a7", "#fafafa", "#e5e5e6"}, {"#fafafa", "#4078f2", "#4078f2"}}},
    {"Tokyo Night", 0, {{"#565f89", "#1a1b26", "#24283b"}, {"#1a1b26", "#7aa2f7", "#7aa2f7"}}},
    {"Tokyo Night Storm", 0, {{"#565f89", "#24283b", "#414868"}, {"#24283b", "#7aa2f7", "#7aa2f7"}}},
    {"Catppuccin Mocha", 0, {{"#6c7086", "#1e1e2e", "#313244"}, {"#1e1e2e", "#cba6f7", "#cba6f7"}}},
    {"Catppuccin Latte", 1, {{"#9ca0b0", "#eff1f5", "#ccd0da"}, {"#eff1f5", "#8839ef", "#8839ef"}}},
    {"Catppuccin Frappe", 0, {{"#737994", "#303446", "#414559"}, {"#303446", "#ca9ee6", "#ca9ee6"}}},
    {"Catppuccin Macchiato", 0, {{"#6e738d", "#24273a", "#363a4f"}, {"#24273a", "#c6a0f6", "#c6a0f6"}}},
    {"Monokai", 0, {{"#75715e", "#272822", "#3e3d32"}, {"#f8f8f2", "#f92672", "#f92672"}}},
    {"Everforest Dark", 0, {{"#859289", "#2d353b", "#343f44"}, {"#2d353b", "#a7c080", "#a7c080"}}},
    {"Everforest Light", 1, {{"#939f91", "#fffbef", "#f4f0d9"}, {"#fffbef", "#8da101", "#8da101"}}},
    {"Rose Pine", 0, {{"#6e6a86", "#191724", "#1f1d2e"}, {"#191724", "#c4a7e7", "#c4a7e7"}}},
    {"Rose Pine Dawn", 1, {{"#797593", "#faf4ed", "#f2e9e1"}, {"#faf4ed", "#907aa9", "#907aa9"}}},
    {"Ayu Dark", 0, {{"#5c6773", "#0a0e14", "#131721"}, {"#0a0e14", "#ffb454", "#ffb454"}}},
    {"Kanagawa", 0, {{"#727169", "#16161d", "#1f1f28"}, {"#16161d", "#7e9cd8", "#7e9cd8"}}},
    {"Github Dark", 0, {{"#8b949e", "#0d1117", "#30363d"}, {"#0d1117", "#58a6ff", "#58a6ff"}}},
    {"Github Light", 1, {{"#57606a", "#ffffff", "#d0d7de"}, {"#ffffff", "#0969da", "#0969da"}}},
    {"Material Palenight", 0, {{"#676e95", "#292d3e", "#32374a"}, {"#292d3e", "#c792ea", "#c792ea"}}},
    {"Nightfox", 0, {{"#738091", "#192330", "#29394f"}, {"#192330", "#719cd6", "#719cd6"}}},
    /* Everything below is generated from the base16 scheme project
     * (github.com/tinted-theming/schemes, formerly base16-schemes-source) --
     * base03/base00/base01 -> normal.fg/bg/border, base0D (the
     * project's conventional "blue"/primary accent) -> selected.bg/border,
     * and selected.fg is base00 for a dark scheme or base07 for a light one
     * (per each file's own "variant" field), so the accent stays legible
     * against the darkest/lightest end of the palette either way. */
    {"Base16: 0x96f", 0, {{"#676567", "#262427", "#3b393c"}, {"#262427", "#49cae4", "#49cae4"}}},
    {"Base16: 3024", 0, {{"#5c5855", "#090300", "#3a3432"}, {"#090300", "#01a0e4", "#01a0e4"}}},
    {"Base16: Apathy", 0, {{"#2b685e", "#031a16", "#0b342d"}, {"#031a16", "#96883e", "#96883e"}}},
    {"Base16: Apprentice", 0, {{"#87875f", "#262626", "#af5f5f"}, {"#262626", "#8787af", "#8787af"}}},
    {"Base16: Ascendancy", 0, {{"#928374", "#282828", "#212f3d"}, {"#282828", "#458588", "#458588"}}},
    {"Base16: Ashes", 0, {{"#747c84", "#1c2023", "#393f45"}, {"#1c2023", "#ae95c7", "#ae95c7"}}},
    {"Base16: Atelier Cave Light", 1, {{"#7e7887", "#efecf4", "#e2dfe7"}, {"#19171c", "#576ddb", "#576ddb"}}},
    {"Base16: Atelier Cave", 0, {{"#655f6d", "#19171c", "#26232a"}, {"#19171c", "#576ddb", "#576ddb"}}},
    {"Base16: Atelier Dune Light", 1, {{"#999580", "#fefbec", "#e8e4cf"}, {"#20201d", "#6684e1", "#6684e1"}}},
    {"Base16: Atelier Dune", 0, {{"#7d7a68", "#20201d", "#292824"}, {"#20201d", "#6684e1", "#6684e1"}}},
    {"Base16: Atelier Estuary Light", 1, {{"#878573", "#f4f3ec", "#e7e6df"}, {"#22221b", "#36a166", "#36a166"}}},
    {"Base16: Atelier Estuary", 0, {{"#6c6b5a", "#22221b", "#302f27"}, {"#22221b", "#36a166", "#36a166"}}},
    {"Base16: Atelier Forest Light", 1, {{"#9c9491", "#f1efee", "#e6e2e0"}, {"#1b1918", "#407ee7", "#407ee7"}}},
    {"Base16: Atelier Forest", 0, {{"#766e6b", "#1b1918", "#2c2421"}, {"#1b1918", "#407ee7", "#407ee7"}}},
    {"Base16: Atelier Heath Light", 1, {{"#9e8f9e", "#f7f3f7", "#d8cad8"}, {"#1b181b", "#516aec", "#516aec"}}},
    {"Base16: Atelier Heath", 0, {{"#776977", "#1b181b", "#292329"}, {"#1b181b", "#516aec", "#516aec"}}},
    {"Base16: Atelier Lakeside Light", 1, {{"#7195a8", "#ebf8ff", "#c1e4f6"}, {"#161b1d", "#257fad", "#257fad"}}},
    {"Base16: Atelier Lakeside", 0, {{"#5a7b8c", "#161b1d", "#1f292e"}, {"#161b1d", "#257fad", "#257fad"}}},
    {"Base16: Atelier Plateau Light", 1, {{"#7e7777", "#f4ecec", "#e7dfdf"}, {"#1b1818", "#7272ca", "#7272ca"}}},
    {"Base16: Atelier Plateau", 0, {{"#655d5d", "#1b1818", "#292424"}, {"#1b1818", "#7272ca", "#7272ca"}}},
    {"Base16: Atelier Savanna Light", 1, {{"#78877d", "#ecf4ee", "#dfe7e2"}, {"#171c19", "#478c90", "#478c90"}}},
    {"Base16: Atelier Savanna", 0, {{"#5f6d64", "#171c19", "#232a25"}, {"#171c19", "#478c90", "#478c90"}}},
    {"Base16: Atelier Seaside Light", 1, {{"#809980", "#f4fbf4", "#cfe8cf"}, {"#131513", "#3d62f5", "#3d62f5"}}},
    {"Base16: Atelier Seaside", 0, {{"#687d68", "#131513", "#242924"}, {"#131513", "#3d62f5", "#3d62f5"}}},
    {"Base16: Atelier Sulphurpool Light", 1, {{"#898ea4", "#f5f7ff", "#dfe2f1"}, {"#202746", "#3d8fd1", "#3d8fd1"}}},
    {"Base16: Atelier Sulphurpool", 0, {{"#6b7394", "#202746", "#293256"}, {"#202746", "#3d8fd1", "#3d8fd1"}}},
    {"Base16: Atlas", 0, {{"#6c8b91", "#002635", "#00384d"}, {"#002635", "#14747e", "#14747e"}}},
    {"Base16: Ayu Dark", 0, {{"#3e4b59", "#0b0e14", "#131721"}, {"#0b0e14", "#59c2ff", "#59c2ff"}}},
    {"Base16: Ayu Light", 1, {{"#a0a6ac", "#f8f9fa", "#edeff1"}, {"#404447", "#399ee6", "#399ee6"}}},
    {"Base16: Ayu Mirage", 0, {{"#4a5059", "#1f2430", "#242936"}, {"#1f2430", "#73d0ff", "#73d0ff"}}},
    {"Base16: Aztec", 0, {{"#2e2e05", "#101600", "#1a1e01"}, {"#101600", "#5b4a9f", "#5b4a9f"}}},
    {"Base16: Bespin", 0, {{"#666666", "#28211c", "#36312e"}, {"#28211c", "#5ea6ea", "#5ea6ea"}}},
    {"Base16: Black Metal (Bathory)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Burzum)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Dark Funeral)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Gorgoroth)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Immortal)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Khold)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Marduk)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Mayhem)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Nile)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal (Venom)", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Black Metal", 0, {{"#333333", "#000000", "#121212"}, {"#000000", "#888888", "#888888"}}},
    {"Base16: Blue Forest", 0, {{"#a0ffa0", "#141f2e", "#1e5c1e"}, {"#141f2e", "#a2cff5", "#a2cff5"}}},
    {"Base16: Blueish", 0, {{"#616d78", "#182430", "#243c54"}, {"#182430", "#82aaff", "#82aaff"}}},
    {"Base16: boo-shnickle", 0, {{"#7c7c63", "#191914", "#3a3a2e"}, {"#191914", "#bfbfd9", "#bfbfd9"}}},
    {"Base16: boo-shnickle-light", 1, {{"#9c9c7d", "#ffffcc", "#dedeb2"}, {"#191914", "#bfbfd9", "#bfbfd9"}}},
    {"Base16: Bosque", 0, {{"#5c6e5e", "#0e1410", "#16211a"}, {"#0e1410", "#6dae9e", "#6dae9e"}}},
    {"Base16: Brasa", 0, {{"#7a6150", "#1a0f0a", "#2b1c14"}, {"#1a0f0a", "#9aa6e0", "#9aa6e0"}}},
    {"Base16: Brewer", 0, {{"#737475", "#0c0d0e", "#2e2f30"}, {"#0c0d0e", "#3182bd", "#3182bd"}}},
    {"Base16: Bright", 0, {{"#b0b0b0", "#000000", "#303030"}, {"#000000", "#6fb3d2", "#6fb3d2"}}},
    {"Base16: Brogrammer", 0, {{"#ecba0f", "#1f1f1f", "#f81118"}, {"#1f1f1f", "#5350b9", "#5350b9"}}},
    {"Base16: Brush Trees Dark", 0, {{"#8299a1", "#485867", "#5a6d7a"}, {"#485867", "#868cb3", "#868cb3"}}},
    {"Base16: Brush Trees", 1, {{"#98afb5", "#e3efef", "#c9dbdc"}, {"#485867", "#868cb3", "#868cb3"}}},
    {"Base16: Cacao", 0, {{"#766052", "#160f0c", "#241913"}, {"#160f0c", "#a89ad0", "#a89ad0"}}},
    {"Base16: caroline", 0, {{"#6d4745", "#1c1213", "#3a2425"}, {"#1c1213", "#684c59", "#684c59"}}},
    {"Base16: Catppuccin Frappe", 0, {{"#51576d", "#303446", "#292c3c"}, {"#303446", "#8caaee", "#8caaee"}}},
    {"Base16: Catppuccin Latte", 1, {{"#bcc0cc", "#eff1f5", "#e6e9ef"}, {"#7287fd", "#1e66f5", "#1e66f5"}}},
    {"Base16: Catppuccin Macchiato", 0, {{"#494d64", "#24273a", "#1e2030"}, {"#24273a", "#8aadf4", "#8aadf4"}}},
    {"Base16: Catppuccin Mocha", 0, {{"#45475a", "#1e1e2e", "#181825"}, {"#1e1e2e", "#89b4fa", "#89b4fa"}}},
    {"Base16: Cerulean Signal Dark", 0, {{"#8fa0b5", "#101722", "#131c29"}, {"#101722", "#7dd3ff", "#7dd3ff"}}},
    {"Base16: Cerulean Signal Light", 1, {{"#637287", "#f7f9fc", "#eef4fb"}, {"#141d2a", "#006fa8", "#006fa8"}}},
    {"Base16: Chalk", 0, {{"#505050", "#151515", "#202020"}, {"#151515", "#6fc2ef", "#6fc2ef"}}},
    {"Base16: Charcoal Dark", 0, {{"#57462c", "#0f0b05", "#231b0e"}, {"#0f0b05", "#c3a983", "#c3a983"}}},
    {"Base16: Charcoal Light", 1, {{"#645538", "#cabda0", "#bcad8c"}, {"#bcad8c", "#251e0f", "#251e0f"}}},
    {"Base16: Chicago Day", 1, {{"#8a9a91", "#e8f0ea", "#d1e0d7"}, {"#1e2a24", "#522398", "#522398"}}},
    {"Base16: Chicago Night", 0, {{"#5f7368", "#1e2a24", "#2a3b32"}, {"#1e2a24", "#522398", "#522398"}}},
    {"Base16: Chinoiserie Midnight", 0, {{"#918072", "#1d1d1d", "#282828"}, {"#1d1d1d", "#81a2a2", "#81a2a2"}}},
    {"Base16: Chinoiserie Morandi", 0, {{"#918072", "#1d1d1d", "#282828"}, {"#1d1d1d", "#839ec9", "#839ec9"}}},
    {"Base16: Chinoiserie Night", 0, {{"#918072", "#1d1d1d", "#282828"}, {"#1d1d1d", "#8fb2c9", "#8fb2c9"}}},
    {"Base16: Chinoiserie", 1, {{"#80766e", "#ffffff", "#e0e0e0"}, {"#131124", "#815c94", "#815c94"}}},
    {"Base16: Circus", 0, {{"#5f5a60", "#191919", "#202020"}, {"#191919", "#639ee4", "#639ee4"}}},
    {"Base16: Classic Dark", 0, {{"#505050", "#151515", "#202020"}, {"#151515", "#6a9fb5", "#6a9fb5"}}},
    {"Base16: Classic Light", 1, {{"#b0b0b0", "#f5f5f5", "#e0e0e0"}, {"#151515", "#6a9fb5", "#6a9fb5"}}},
    {"Base16: Codeschool", 0, {{"#3f4944", "#232c31", "#1c3657"}, {"#232c31", "#484d79", "#484d79"}}},
    {"Base16: Colors", 0, {{"#777777", "#111111", "#333333"}, {"#111111", "#0074d9", "#0074d9"}}},
    {"Base16: Cupcake", 1, {{"#bfb9c6", "#fbf1f2", "#f2f1f4"}, {"#585062", "#7297b9", "#7297b9"}}},
    {"Base16: Cupertino", 1, {{"#808080", "#ffffff", "#c0c0c0"}, {"#5e5e5e", "#0000ff", "#0000ff"}}},
    {"Base16: Da One Black", 0, {{"#888888", "#000000", "#282828"}, {"#000000", "#6bb8ff", "#6bb8ff"}}},
    {"Base16: Da One Gray", 0, {{"#888888", "#181818", "#282828"}, {"#181818", "#6bb8ff", "#6bb8ff"}}},
    {"Base16: Da One Ocean", 0, {{"#878d96", "#171726", "#22273d"}, {"#171726", "#6bb8ff", "#6bb8ff"}}},
    {"Base16: Da One Paper", 1, {{"#585858", "#faf0dc", "#c8c8c8"}, {"#000000", "#5890f8", "#5890f8"}}},
    {"Base16: Da One Sea", 0, {{"#878d96", "#22273d", "#374059"}, {"#22273d", "#6bb8ff", "#6bb8ff"}}},
    {"Base16: Da One White", 1, {{"#585858", "#ffffff", "#c8c8c8"}, {"#000000", "#5890f8", "#5890f8"}}},
    {"Base16: DanQing Light", 1, {{"#cad8d2", "#fcfefd", "#ecf6f2"}, {"#2d302f", "#b0a4e3", "#b0a4e3"}}},
    {"Base16: DanQing", 0, {{"#9da8a3", "#2d302f", "#434846"}, {"#2d302f", "#b0a4e3", "#b0a4e3"}}},
    {"Base16: Darcula", 0, {{"#606366", "#2b2b2b", "#323232"}, {"#2b2b2b", "#9876aa", "#9876aa"}}},
    {"Base16: darkmoss", 0, {{"#555e5f", "#171e1f", "#252c2d"}, {"#171e1f", "#498091", "#498091"}}},
    {"Base16: Darktooth", 0, {{"#665c54", "#1d2021", "#32302f"}, {"#1d2021", "#0d6678", "#0d6678"}}},
    {"Base16: Dark Violet", 0, {{"#593380", "#000000", "#231a40"}, {"#000000", "#4136d9", "#4136d9"}}},
    {"Base16: Decaf", 0, {{"#777777", "#2d2d2d", "#393939"}, {"#2d2d2d", "#90bee1", "#90bee1"}}},
    {"Base16: Deep Oceanic Next", 0, {{"#004852", "#001c1f", "#002931"}, {"#001c1f", "#568ccf", "#568ccf"}}},
    {"Base16: Default Dark", 0, {{"#585858", "#181818", "#282828"}, {"#181818", "#7cafc2", "#7cafc2"}}},
    {"Base16: Default Light", 1, {{"#b8b8b8", "#f8f8f8", "#e8e8e8"}, {"#181818", "#7cafc2", "#7cafc2"}}},
    {"Base16: Digital Rain", 0, {{"#7c8d7c", "#000000", "#4a806c"}, {"#000000", "#5482af", "#5482af"}}},
    {"Base16: dirtysea", 1, {{"#707070", "#e0e0e0", "#d0dad0"}, {"#c4d9c4", "#007300", "#007300"}}},
    {"Base16: Dracula", 0, {{"#6272a4", "#282a36", "#21222c"}, {"#282a36", "#bd93f9", "#bd93f9"}}},
    {"Base16: Edge Dark", 0, {{"#4a4c4f", "#262729", "#313235"}, {"#262729", "#73b3e7", "#73b3e7"}}},
    {"Base16: Edge Light", 1, {{"#9197a1", "#fafafa", "#e3e5e8"}, {"#2e2e38", "#6587bf", "#6587bf"}}},
    {"Base16: Eighties", 0, {{"#747369", "#2d2d2d", "#393939"}, {"#2d2d2d", "#6699cc", "#6699cc"}}},
    {"Base16: Eldritch", 0, {{"#7081d0", "#212337", "#323449"}, {"#212337", "#39ddfd", "#39ddfd"}}},
    {"Base16: Embers Light", 1, {{"#8a8075", "#dbd6d1", "#beb6ae"}, {"#16130f", "#6d5782", "#6d5782"}}},
    {"Base16: Embers", 0, {{"#5a5047", "#16130f", "#2c2620"}, {"#16130f", "#6d5782", "#6d5782"}}},
    {"Base16: emil", 1, {{"#7c7c98", "#efefef", "#bebed2"}, {"#1a1a2f", "#471397", "#471397"}}},
    {"Base16: Equilibrium Dark", 0, {{"#7b776e", "#0c1118", "#181c22"}, {"#0c1118", "#008dd1", "#008dd1"}}},
    {"Base16: Equilibrium Gray Dark", 0, {{"#777777", "#111111", "#1b1b1b"}, {"#111111", "#008dd1", "#008dd1"}}},
    {"Base16: Equilibrium Gray Light", 1, {{"#777777", "#f1f1f1", "#e2e2e2"}, {"#1b1b1b", "#0073b5", "#0073b5"}}},
    {"Base16: Equilibrium Light", 1, {{"#73777f", "#f5f0e7", "#e7e2d9"}, {"#181c22", "#0073b5", "#0073b5"}}},
    {"Base16: eris", 0, {{"#333773", "#0a0920", "#13133a"}, {"#0a0920", "#258fc4", "#258fc4"}}},
    {"Base16: Espresso", 0, {{"#777777", "#2d2d2d", "#393939"}, {"#2d2d2d", "#6c99bb", "#6c99bb"}}},
    {"Base16: Eva Dim", 0, {{"#55799c", "#2a3b4d", "#3d566f"}, {"#2a3b4d", "#1ae1dc", "#1ae1dc"}}},
    {"Base16: Eva", 0, {{"#55799c", "#2a3b4d", "#3d566f"}, {"#2a3b4d", "#15f4ee", "#15f4ee"}}},
    {"Base16: Evenok Dark", 0, {{"#505050", "#000000", "#202020"}, {"#000000", "#00aff2", "#00aff2"}}},
    {"Base16: Everforest Dark Hard", 0, {{"#859289", "#272e33", "#2e383c"}, {"#272e33", "#7fbbb3", "#7fbbb3"}}},
    {"Base16: Everforest Dark Medium", 0, {{"#475258", "#2d353b", "#343f44"}, {"#2d353b", "#7fbbb3", "#7fbbb3"}}},
    {"Base16: Everforest Dark Soft", 0, {{"#859289", "#333c43", "#3a464c"}, {"#333c43", "#7fbbb3", "#7fbbb3"}}},
    {"Base16: Everforest Light (Hard)", 1, {{"#939f91", "#fffbef", "#f8f5e4"}, {"#272e33", "#3a94c5", "#3a94c5"}}},
    {"Base16: Everforest Light (Medium)", 1, {{"#939f91", "#fdf6e3", "#f4f0d9"}, {"#2d353b", "#3a94c5", "#3a94c5"}}},
    {"Base16: Everforest Light (Soft)", 1, {{"#939f91", "#f3ead3", "#eae4ca"}, {"#333c43", "#3a94c5", "#3a94c5"}}},
    {"Base16: Everforest", 0, {{"#859289", "#2d353b", "#343f44"}, {"#2d353b", "#7fbbb3", "#7fbbb3"}}},
    {"Base16: Flat", 0, {{"#95a5a6", "#2c3e50", "#34495e"}, {"#2c3e50", "#3498db", "#3498db"}}},
    {"Base16: Flexoki Dark", 0, {{"#575653", "#100f0f", "#1c1b1a"}, {"#100f0f", "#4385be", "#4385be"}}},
    {"Base16: Flexoki Light", 1, {{"#cecdc3", "#fffcf0", "#f2f0e5"}, {"#100f0f", "#205ea6", "#205ea6"}}},
    {"Base16: Framer", 0, {{"#747474", "#181818", "#151515"}, {"#181818", "#20bcfc", "#20bcfc"}}},
    {"Base16: Fruit Soda", 1, {{"#b5b4b6", "#f1ecf1", "#e0dee0"}, {"#2d2c2c", "#2931df", "#2931df"}}},
    {"Base16: Gigavolt", 0, {{"#a1d2e6", "#202126", "#2d303d"}, {"#202126", "#40bfff", "#40bfff"}}},
    {"Base16: Github Dark Colorblind", 0, {{"#6e7681", "#0d1117", "#161b22"}, {"#0d1117", "#d2a8ff", "#d2a8ff"}}},
    {"Base16: Github Dark Dimmed", 0, {{"#636e7b", "#22272e", "#2d333b"}, {"#22272e", "#dcbdfb", "#dcbdfb"}}},
    {"Base16: Github Dark High Contrast", 0, {{"#9ea7b3", "#0a0c10", "#272b33"}, {"#0a0c10", "#dbb7ff", "#dbb7ff"}}},
    {"Base16: Github Dark", 0, {{"#6e7681", "#0d1117", "#161b22"}, {"#0d1117", "#d2a8ff", "#d2a8ff"}}},
    {"Base16: Github Light Colorblind", 1, {{"#8c959f", "#ffffff", "#f6f8fa"}, {"#24292f", "#8250df", "#8250df"}}},
    {"Base16: Github Light High Contrast", 1, {{"#88929d", "#ffffff", "#e7ecf0"}, {"#0e1116", "#622cbc", "#622cbc"}}},
    {"Base16: Github", 1, {{"#8c959f", "#ffffff", "#f6f8fa"}, {"#1f2328", "#8250df", "#8250df"}}},
    {"Base16: Google Dark", 0, {{"#969896", "#1d1f21", "#282a2e"}, {"#1d1f21", "#3971ed", "#3971ed"}}},
    {"Base16: Google Light", 1, {{"#b4b7b4", "#ffffff", "#e0e0e0"}, {"#1d1f21", "#3971ed", "#3971ed"}}},
    {"Base16: Gotham", 0, {{"#0a3749", "#0c1014", "#11151c"}, {"#0c1014", "#195466", "#195466"}}},
    {"Base16: Grayscale Dark", 0, {{"#525252", "#101010", "#252525"}, {"#101010", "#686868", "#686868"}}},
    {"Base16: Grayscale Light", 1, {{"#ababab", "#f7f7f7", "#e3e3e3"}, {"#101010", "#686868", "#686868"}}},
    {"Base16: Green Screen", 0, {{"#007700", "#001100", "#003300"}, {"#001100", "#009900", "#009900"}}},
    {"Base16: Gruber", 0, {{"#52494e", "#181818", "#453d41"}, {"#181818", "#96a6c8", "#96a6c8"}}},
    {"Base16: Gruvbox dark, hard", 0, {{"#665c54", "#1d2021", "#3c3836"}, {"#1d2021", "#83a598", "#83a598"}}},
    {"Base16: Gruvbox Dark Medium (Forest)", 0, {{"#665c50", "#282828", "#3c3836"}, {"#282828", "#b8bb26", "#b8bb26"}}},
    {"Base16: Gruvbox dark, medium", 0, {{"#665c54", "#282828", "#3c3836"}, {"#282828", "#83a598", "#83a598"}}},
    {"Base16: Gruvbox dark, pale", 0, {{"#8a8a8a", "#262626", "#3a3a3a"}, {"#262626", "#83adad", "#83adad"}}},
    {"Base16: Gruvbox dark, soft", 0, {{"#665c54", "#32302f", "#3c3836"}, {"#32302f", "#83a598", "#83a598"}}},
    {"Base16: Gruvbox dark", 0, {{"#665c54", "#282828", "#3c3836"}, {"#282828", "#458588", "#458588"}}},
    {"Base16: Gruvbox light, hard", 1, {{"#bdae93", "#f9f5d7", "#ebdbb2"}, {"#282828", "#076678", "#076678"}}},
    {"Base16: Gruvbox light, medium", 1, {{"#bdae93", "#fbf1c7", "#ebdbb2"}, {"#282828", "#076678", "#076678"}}},
    {"Base16: Gruvbox light, soft", 1, {{"#bdae93", "#f2e5bc", "#ebdbb2"}, {"#282828", "#076678", "#076678"}}},
    {"Base16: Gruvbox Light", 1, {{"#bdae93", "#fbf1c7", "#ebdbb2"}, {"#1d2021", "#458588", "#458588"}}},
    {"Base16: Gruvbox Material Dark, Hard", 0, {{"#5a524c", "#202020", "#2a2827"}, {"#202020", "#7daea3", "#7daea3"}}},
    {"Base16: Gruvbox Material Dark, Medium", 0, {{"#665c54", "#292828", "#32302f"}, {"#292828", "#7daea3", "#7daea3"}}},
    {"Base16: Gruvbox Material Dark, Soft", 0, {{"#7c6f64", "#32302f", "#3c3836"}, {"#32302f", "#7daea3", "#7daea3"}}},
    {"Base16: Gruvbox Material Light, Hard", 1, {{"#a89984", "#f9f5d7", "#fbf1c7"}, {"#282828", "#45707a", "#45707a"}}},
    {"Base16: Gruvbox Material Light, Medium", 1, {{"#bdae93", "#fbf1c7", "#f2e5bc"}, {"#282828", "#45707a", "#45707a"}}},
    {"Base16: Gruvbox Material Light, Soft", 1, {{"#a89984", "#f2e5bc", "#ebdbb2"}, {"#282828", "#45707a", "#45707a"}}},
    {"Base16: Hardcore", 0, {{"#4a4a4a", "#212121", "#303030"}, {"#212121", "#66d9ef", "#66d9ef"}}},
    {"Base16: Hardhacker", 0, {{"#6e6780", "#211e2a", "#2c2737"}, {"#211e2a", "#95a6f4", "#95a6f4"}}},
    {"Base16: Harmonic16 Dark", 0, {{"#627e99", "#0b1c2c", "#223b54"}, {"#0b1c2c", "#8b56bf", "#8b56bf"}}},
    {"Base16: Harmonic16 Light", 1, {{"#aabcce", "#f7f9fb", "#e5ebf1"}, {"#0b1c2c", "#8b56bf", "#8b56bf"}}},
    {"Base16: Heetch Light", 1, {{"#9c92a8", "#feffff", "#dedae2"}, {"#190134", "#5ba2b6", "#5ba2b6"}}},
    {"Base16: Heetch Dark", 0, {{"#7b6d8b", "#190134", "#392551"}, {"#190134", "#bd0152", "#bd0152"}}},
    {"Base16: Helios", 0, {{"#6f7579", "#1d2021", "#383c3e"}, {"#1d2021", "#1e8bac", "#1e8bac"}}},
    {"Base16: Hopscotch", 0, {{"#797379", "#322931", "#433b42"}, {"#322931", "#1290bf", "#1290bf"}}},
    {"Base16: Horizon Dark", 0, {{"#6f6f70", "#1c1e26", "#232530"}, {"#1c1e26", "#df5273", "#df5273"}}},
    {"Base16: Horizon Light", 1, {{"#bdb3b1", "#fdf0ed", "#fadad1"}, {"#201c1d", "#da103f", "#da103f"}}},
    {"Base16: Horizon Terminal Dark", 0, {{"#6f6f70", "#1c1e26", "#232530"}, {"#1c1e26", "#26bbd9", "#26bbd9"}}},
    {"Base16: Horizon Terminal Light", 1, {{"#bdb3b1", "#fdf0ed", "#fadad1"}, {"#201c1d", "#26bbd9", "#26bbd9"}}},
    {"Base16: Humanoid dark", 0, {{"#60615d", "#232629", "#333b3d"}, {"#232629", "#00a6fb", "#00a6fb"}}},
    {"Base16: Humanoid light", 1, {{"#c0c0bd", "#f8f8f2", "#efefe9"}, {"#070708", "#0082c9", "#0082c9"}}},
    {"Base16: iA Dark", 0, {{"#767676", "#1a1a1a", "#222222"}, {"#1a1a1a", "#8eccdd", "#8eccdd"}}},
    {"Base16: iA Light", 1, {{"#898989", "#f6f6f6", "#dedede"}, {"#f8f8f8", "#48bac2", "#48bac2"}}},
    {"Base16: Icy Dark", 0, {{"#052e34", "#021012", "#031619"}, {"#021012", "#00bcd4", "#00bcd4"}}},
    {"Base16: IR Black", 0, {{"#6c6c66", "#000000", "#242422"}, {"#000000", "#96cbfe", "#96cbfe"}}},
    {"Base16: Isotope", 0, {{"#808080", "#000000", "#404040"}, {"#000000", "#0066ff", "#0066ff"}}},
    {"Base16: Jabuti", 0, {{"#45475d", "#292a37", "#343545"}, {"#292a37", "#3fc6de", "#3fc6de"}}},
    {"Base16: Jellybeans", 0, {{"#c5c5c5", "#121212", "#929292"}, {"#121212", "#b1d8f6", "#b1d8f6"}}},
    {"Base16: Kanagawa Dragon", 0, {{"#625e5a", "#181616", "#282727"}, {"#181616", "#8ba4b0", "#8ba4b0"}}},
    {"Base16: Kanagawa", 0, {{"#54546d", "#1f1f28", "#16161d"}, {"#1f1f28", "#7e9cd8", "#7e9cd8"}}},
    {"Base16: Katy", 0, {{"#676e95", "#292d3e", "#444267"}, {"#292d3e", "#82aaff", "#82aaff"}}},
    {"Base16: Kimber", 0, {{"#644646", "#222222", "#313131"}, {"#222222", "#537c9c", "#537c9c"}}},
    {"Base16: Kissa Latte", 1, {{"#91887d", "#f5f4f0", "#e8e7e3"}, {"#fefcfa", "#3468a8", "#3468a8"}}},
    {"Base16: Kissa Macchiato", 0, {{"#b8a48c", "#1f1c16", "#35322d"}, {"#1f1c16", "#7fa8d4", "#7fa8d4"}}},
    {"Base16: Lichen Chartreuse Dark", 0, {{"#899282", "#151613", "#1c1e1a"}, {"#151613", "#78adc4", "#78adc4"}}},
    {"Base16: Lichen Chartreuse Light", 1, {{"#687161", "#f5f7f2", "#ecefe7"}, {"#151613", "#356e8a", "#356e8a"}}},
    {"Base16: lime", 0, {{"#313140", "#1a1a2f", "#202030"}, {"#1a1a2f", "#2b926f", "#2b926f"}}},
    {"Base16: Linux VT", 0, {{"#555555", "#000000", "#333333"}, {"#000000", "#5555ff", "#5555ff"}}},
    {"Base16: Macintosh", 0, {{"#808080", "#000000", "#404040"}, {"#000000", "#0000d3", "#0000d3"}}},
    {"Base16: Marrakesh", 0, {{"#6c6823", "#201602", "#302e00"}, {"#201602", "#477ca1", "#477ca1"}}},
    {"Base16: Materia", 0, {{"#707880", "#263238", "#2c393f"}, {"#263238", "#89ddff", "#89ddff"}}},
    {"Base16: Material Darker", 0, {{"#4a4a4a", "#212121", "#303030"}, {"#212121", "#82aaff", "#82aaff"}}},
    {"Base16: Material Lighter", 1, {{"#ccd7da", "#fafafa", "#e7eaec"}, {"#000000", "#6182b8", "#6182b8"}}},
    {"Base16: Material Palenight", 0, {{"#676e95", "#292d3e", "#444267"}, {"#292d3e", "#82aaff", "#82aaff"}}},
    {"Base16: Material Vivid", 0, {{"#44464d", "#202124", "#27292c"}, {"#202124", "#2196f3", "#2196f3"}}},
    {"Base16: Material", 0, {{"#546e7a", "#263238", "#2e3c43"}, {"#263238", "#82aaff", "#82aaff"}}},
    {"Base16: Measured Dark", 0, {{"#ababab", "#00211f", "#003a38"}, {"#00211f", "#88b0da", "#88b0da"}}},
    {"Base16: Measured Light", 1, {{"#5a5a5a", "#fdf9f5", "#f9f5f1"}, {"#000000", "#0158ad", "#0158ad"}}},
    {"Base16: Mellow Purple", 0, {{"#320f55", "#1e0528", "#1a092d"}, {"#1e0528", "#550068", "#550068"}}},
    {"Base16: Mexico Light", 1, {{"#b8b8b8", "#f8f8f8", "#e8e8e8"}, {"#181818", "#7cafc2", "#7cafc2"}}},
    {"Base16: Mezcal", 0, {{"#6e6450", "#13110e", "#221f14"}, {"#13110e", "#8aa6c0", "#8aa6c0"}}},
    {"Base16: Mocha", 0, {{"#7e705a", "#3b3228", "#534636"}, {"#3b3228", "#8ab3b5", "#8ab3b5"}}},
    {"Base16: Monokai", 0, {{"#75715e", "#272822", "#383830"}, {"#272822", "#66d9ef", "#66d9ef"}}},
    {"Base16: Moonlight", 0, {{"#748cd6", "#212337", "#403c64"}, {"#212337", "#40ffff", "#40ffff"}}},
    {"Base16: Mountain", 0, {{"#393939", "#0f0f0f", "#191919"}, {"#0f0f0f", "#8f8aac", "#8f8aac"}}},
    {"Base16: Nebula", 0, {{"#6e6f72", "#22273b", "#414f60"}, {"#22273b", "#4d6bb6", "#4d6bb6"}}},
    {"Base16: Noche", 0, {{"#5a6178", "#0c0e16", "#181c2c"}, {"#0c0e16", "#7aa0e8", "#7aa0e8"}}},
    {"Base16: Nord Light", 1, {{"#aebacf", "#e5e9f0", "#c2d0e7"}, {"#29838d", "#3b6ea8", "#3b6ea8"}}},
    {"Base16: Nord", 0, {{"#4c566a", "#2e3440", "#3b4252"}, {"#2e3440", "#81a1c1", "#81a1c1"}}},
    {"Base16: Nova", 0, {{"#899ba6", "#3c4c55", "#556873"}, {"#3c4c55", "#83afe5", "#83afe5"}}},
    {"Base16: Ocean", 0, {{"#65737e", "#2b303b", "#343d46"}, {"#2b303b", "#8fa1b3", "#8fa1b3"}}},
    {"Base16: OceanicNext", 0, {{"#65737e", "#1b2b34", "#343d46"}, {"#1b2b34", "#6699cc", "#6699cc"}}},
    {"Base16: Ocote", 0, {{"#6b6253", "#14100c", "#211b13"}, {"#14100c", "#82a6e0", "#82a6e0"}}},
    {"Base16: One Light", 1, {{"#a0a1a7", "#fafafa", "#f0f0f1"}, {"#090a0b", "#4078f2", "#4078f2"}}},
    {"Base16: OneDark Dark", 0, {{"#434852", "#000000", "#1c1f24"}, {"#000000", "#61afef", "#61afef"}}},
    {"Base16: OneDark", 0, {{"#545862", "#282c34", "#353b45"}, {"#282c34", "#61afef", "#61afef"}}},
    {"Base16: Outrun Dark", 0, {{"#50507a", "#00002a", "#20204a"}, {"#00002a", "#66b0ff", "#66b0ff"}}},
    {"Base16: Oxocarbon Dark", 0, {{"#525252", "#161616", "#262626"}, {"#161616", "#33b1ff", "#33b1ff"}}},
    {"Base16: Oxocarbon Light", 1, {{"#a1acba", "#f2f4f8", "#dde1e6"}, {"#272d35", "#0f62fe", "#0f62fe"}}},
    {"Base16: pandora", 0, {{"#ffbee3", "#131213", "#2f1823"}, {"#131213", "#008080", "#008080"}}},
    {"Base16: Papel", 1, {{"#9a8c76", "#f5efe2", "#8e8576"}, {"#efe8d8", "#2c6ca0", "#2c6ca0"}}},
    {"Base16: PaperColor Dark", 0, {{"#585858", "#1c1c1c", "#363636"}, {"#1c1c1c", "#5fafd7", "#5fafd7"}}},
    {"Base16: PaperColor Light", 1, {{"#858585", "#eeeeee", "#c4c4c4"}, {"#444444", "#005f87", "#005f87"}}},
    {"Base16: Paraiso", 0, {{"#776e71", "#2f1e2e", "#41323f"}, {"#2f1e2e", "#06b6ef", "#06b6ef"}}},
    {"Base16: Pasque", 0, {{"#5d5766", "#271c3a", "#100323"}, {"#271c3a", "#8e7dc6", "#8e7dc6"}}},
    {"Base16: Penumbra Dark Contrast Plus Plus", 0, {{"#636363", "#0d0f13", "#181b1f"}, {"#0d0f13", "#6eb2fd", "#6eb2fd"}}},
    {"Base16: Penumbra Dark Contrast Plus", 0, {{"#636363", "#181b1f", "#24272b"}, {"#181b1f", "#61a3e6", "#61a3e6"}}},
    {"Base16: Penumbra Dark", 0, {{"#636363", "#24272b", "#303338"}, {"#24272b", "#5794d0", "#5794d0"}}},
    {"Base16: Penumbra Light Contrast Plus Plus", 1, {{"#dedede", "#fffdfb", "#fff7ed"}, {"#0d0f13", "#6eb2fd", "#6eb2fd"}}},
    {"Base16: Penumbra Light Contrast Plus", 1, {{"#cecece", "#fffdfb", "#fff7ed"}, {"#181b1f", "#61a3e6", "#61a3e6"}}},
    {"Base16: Penumbra Light", 1, {{"#bebebe", "#fffdfb", "#fff7ed"}, {"#24272b", "#5794d0", "#5794d0"}}},
    {"Base16: PhD", 0, {{"#717885", "#061229", "#2a3448"}, {"#061229", "#5299bf", "#5299bf"}}},
    {"Base16: Pico", 0, {{"#008751", "#000000", "#1d2b53"}, {"#000000", "#83769c", "#83769c"}}},
    {"Base16: pinky", 0, {{"#383338", "#171517", "#1b181b"}, {"#171517", "#00ffff", "#00ffff"}}},
    {"Base16: Pop", 0, {{"#505050", "#000000", "#202020"}, {"#000000", "#0e5a94", "#0e5a94"}}},
    {"Base16: Porple", 0, {{"#65568a", "#292c36", "#333344"}, {"#292c36", "#8485ce", "#8485ce"}}},
    {"Base16: Precious Dark Eleven", 0, {{"#858585", "#1c1e20", "#292b2d"}, {"#1c1e20", "#68b0ee", "#68b0ee"}}},
    {"Base16: Precious Dark Fifteen", 0, {{"#898989", "#23262b", "#303337"}, {"#23262b", "#66b0ef", "#66b0ef"}}},
    {"Base16: Precious Light Warm", 1, {{"#7f8080", "#fff5e5", "#ece4d6"}, {"#4e5359", "#246da5", "#246da5"}}},
    {"Base16: Precious Light White", 1, {{"#848484", "#ffffff", "#ededed"}, {"#555555", "#186daa", "#186daa"}}},
    {"Base16: Primer Dark Dimmed", 0, {{"#545d68", "#1c2128", "#373e47"}, {"#1c2128", "#539bf5", "#539bf5"}}},
    {"Base16: Primer Dark", 0, {{"#484f58", "#010409", "#21262d"}, {"#010409", "#58a6ff", "#58a6ff"}}},
    {"Base16: Primer Light", 1, {{"#959da5", "#fafbfc", "#e1e4e8"}, {"#1b1f23", "#0366d6", "#0366d6"}}},
    {"Base16: Purpledream", 0, {{"#605060", "#100510", "#302030"}, {"#100510", "#00a0f0", "#00a0f0"}}},
    {"Base16: Qualia", 0, {{"#454545", "#101010", "#454545"}, {"#101010", "#50cacd", "#50cacd"}}},
    {"Base16: Railscasts", 0, {{"#5a647e", "#2b2b2b", "#272935"}, {"#2b2b2b", "#6d9cbe", "#6d9cbe"}}},
    {"Base16: Rebecca", 0, {{"#666699", "#292a44", "#663399"}, {"#292a44", "#2de0a7", "#2de0a7"}}},
    {"Base16: Rosé Pine Dawn", 1, {{"#9893a5", "#faf4ed", "#fffaf3"}, {"#cecacd", "#907aa9", "#907aa9"}}},
    {"Base16: Rosé Pine Moon", 0, {{"#6e6a86", "#232136", "#2a273f"}, {"#232136", "#c4a7e7", "#c4a7e7"}}},
    {"Base16: Rosé Pine", 0, {{"#6e6a86", "#191724", "#1f1d2e"}, {"#191724", "#c4a7e7", "#c4a7e7"}}},
    {"Base16: SAGA", 0, {{"#141f27", "#05080a", "#0a1014"}, {"#05080a", "#c9fff7", "#c9fff7"}}},
    {"Base16: Sagelight", 1, {{"#b8b8b8", "#f8f8f8", "#e8e8e8"}, {"#181818", "#a0a7d2", "#a0a7d2"}}},
    {"Base16: Sakura", 1, {{"#755f64", "#feedf3", "#f8e2e7"}, {"#33292b", "#006e93", "#006e93"}}},
    {"Base16: Sandcastle", 0, {{"#665c54", "#282c34", "#2c323b"}, {"#282c34", "#83a598", "#83a598"}}},
    {"Base16: selenized-black", 0, {{"#777777", "#181818", "#252525"}, {"#181818", "#368aeb", "#368aeb"}}},
    {"Base16: selenized-dark", 0, {{"#72898f", "#103c48", "#184956"}, {"#103c48", "#4695f7", "#4695f7"}}},
    {"Base16: selenized-light", 1, {{"#909995", "#fbf3db", "#ece3cc"}, {"#3a4d53", "#006dce", "#006dce"}}},
    {"Base16: selenized-white", 1, {{"#878787", "#ffffff", "#ebebeb"}, {"#282828", "#0054cf", "#0054cf"}}},
    {"Base16: Seti UI", 0, {{"#41535b", "#151718", "#282a2b"}, {"#151718", "#55b5db", "#55b5db"}}},
    {"Base16: Shades of Purple", 0, {{"#808080", "#1e1e3f", "#43d426"}, {"#1e1e3f", "#6943ff", "#6943ff"}}},
    {"Base16: ShadeSmear Dark", 0, {{"#c0c0c0", "#232323", "#1c1c1c"}, {"#232323", "#376388", "#376388"}}},
    {"Base16: ShadeSmear Light", 1, {{"#4e4e4e", "#dbdbdb", "#e4e4e4"}, {"#e4e4e4", "#376388", "#376388"}}},
    {"Base16: Shapeshifter", 1, {{"#555555", "#f9f9f9", "#e0e0e0"}, {"#000000", "#3b48e3", "#3b48e3"}}},
    {"Base16: Silk Dark", 0, {{"#587073", "#0e3c46", "#1d494e"}, {"#0e3c46", "#46bddd", "#46bddd"}}},
    {"Base16: Silk Light", 1, {{"#5c787b", "#e9f1ef", "#ccd4d3"}, {"#d2faff", "#39aac9", "#39aac9"}}},
    {"Base16: Snazzy", 0, {{"#78787e", "#282a36", "#34353e"}, {"#282a36", "#57c7ff", "#57c7ff"}}},
    {"Base16: Soft Server", 0, {{"#6e6780", "#211e2a", "#2c2737"}, {"#211e2a", "#95a6f4", "#95a6f4"}}},
    {"Base16: Solar Flare Light", 1, {{"#85939e", "#f5f7fa", "#e8e9ed"}, {"#18262f", "#33b5e1", "#33b5e1"}}},
    {"Base16: Solar Flare", 0, {{"#667581", "#18262f", "#222e38"}, {"#18262f", "#33b5e1", "#33b5e1"}}},
    {"Base16: Solarized Dark", 0, {{"#657b83", "#002b36", "#073642"}, {"#002b36", "#268bd2", "#268bd2"}}},
    {"Base16: Solarized Light", 1, {{"#839496", "#fdf6e3", "#eee8d5"}, {"#002b36", "#268bd2", "#268bd2"}}},
    {"Base16: Spaceduck", 0, {{"#686f9a", "#16172d", "#1b1c36"}, {"#16172d", "#7a5ccc", "#7a5ccc"}}},
    {"Base16: Spacemacs", 0, {{"#585858", "#1f2022", "#282828"}, {"#1f2022", "#4f97d7", "#4f97d7"}}},
    {"Base16: Sparky", 0, {{"#003b49", "#072b31", "#00313c"}, {"#072b31", "#4698cb", "#4698cb"}}},
    {"Base16: standardized-dark", 0, {{"#898989", "#222222", "#303030"}, {"#222222", "#00a3f2", "#00a3f2"}}},
    {"Base16: standardized-light", 1, {{"#767676", "#ffffff", "#eeeeee"}, {"#222222", "#3173c5", "#3173c5"}}},
    {"Base16: Stella", 0, {{"#655978", "#2b213c", "#362b48"}, {"#2b213c", "#a5aad4", "#a5aad4"}}},
    {"Base16: Still Alive", 1, {{"#a3a3a3", "#f0f0f0", "#d6d6d6"}, {"#140c0d", "#365eff", "#365eff"}}},
    {"Base16: summercamp", 0, {{"#504b38", "#1c1810", "#2a261c"}, {"#1c1810", "#489bf0", "#489bf0"}}},
    {"Base16: Summerfruit Dark", 0, {{"#505050", "#151515", "#202020"}, {"#151515", "#3777e6", "#3777e6"}}},
    {"Base16: Summerfruit Light", 1, {{"#b0b0b0", "#ffffff", "#e0e0e0"}, {"#202020", "#3777e6", "#3777e6"}}},
    {"Base16: Swamp Dark", 0, {{"#5f4e41", "#242015", "#3a3124"}, {"#242015", "#c1666b", "#c1666b"}}},
    {"Base16: Swamp Light", 1, {{"#b5a492", "#f1e3d1", "#ddcebc"}, {"#8c7b68", "#bf7979", "#bf7979"}}},
    {"Base16: Synth Midnight Terminal Dark", 0, {{"#474849", "#050608", "#1a1b1c"}, {"#050608", "#03aeff", "#03aeff"}}},
    {"Base16: Synth Midnight Terminal Light", 1, {{"#a3a5a6", "#dddfe0", "#cfd1d2"}, {"#050608", "#03aeff", "#03aeff"}}},
    {"Base16: Tango", 0, {{"#555753", "#2e3436", "#8ae234"}, {"#2e3436", "#3465a4", "#3465a4"}}},
    {"Base16: tarot", 0, {{"#74316b", "#0e091d", "#2a153c"}, {"#0e091d", "#6e6080", "#6e6080"}}},
    {"Base16: tender", 0, {{"#4c4c4c", "#282828", "#383838"}, {"#282828", "#b3deef", "#b3deef"}}},
    {"Base16: Terracotta Dark", 0, {{"#594740", "#241d1a", "#362b27"}, {"#241d1a", "#b0a4c3", "#b0a4c3"}}},
    {"Base16: Terracotta", 1, {{"#c0aca4", "#efeae8", "#dfd6d1"}, {"#241c19", "#625574", "#625574"}}},
    {"Base16: Tinta", 0, {{"#62626a", "#101012", "#202023"}, {"#101012", "#8a9ab0", "#8a9ab0"}}},
    {"Base16: Tokyo City Dark", 0, {{"#526270", "#171d23", "#1d252c"}, {"#171d23", "#7aa2f7", "#7aa2f7"}}},
    {"Base16: Tokyo City Light", 1, {{"#9699a3", "#fbfbfd", "#f6f6f8"}, {"#171d23", "#34548a", "#34548a"}}},
    {"Base16: Tokyo City Terminal Dark", 0, {{"#526270", "#171d23", "#1d252c"}, {"#171d23", "#539afc", "#539afc"}}},
    {"Base16: Tokyo City Terminal Light", 1, {{"#b7c5d3", "#fbfbfd", "#f6f6f8"}, {"#171d23", "#34548a", "#34548a"}}},
    {"Base16: Tokyo Night Dark", 0, {{"#444b6a", "#1a1b26", "#16161e"}, {"#1a1b26", "#2ac3de", "#2ac3de"}}},
    {"Base16: Tokyo Night Light", 1, {{"#9699a3", "#d5d6db", "#cbccd1"}, {"#1a1b26", "#34548a", "#34548a"}}},
    {"Base16: Tokyo Night Moon", 0, {{"#3b4261", "#222436", "#1e2030"}, {"#222436", "#82aaff", "#82aaff"}}},
    {"Base16: Tokyo Night Storm", 0, {{"#444b6a", "#24283b", "#16161e"}, {"#24283b", "#2ac3de", "#2ac3de"}}},
    {"Base16: Tokyo Night Terminal Dark", 0, {{"#444b6a", "#16161e", "#1a1b26"}, {"#16161e", "#7aa2f7", "#7aa2f7"}}},
    {"Base16: Tokyo Night Terminal Light", 1, {{"#9699a3", "#d5d6db", "#cbccd1"}, {"#1a1b26", "#34548a", "#34548a"}}},
    {"Base16: Tokyo Night Terminal Storm", 0, {{"#444b6a", "#24283b", "#1a1b26"}, {"#24283b", "#7aa2f7", "#7aa2f7"}}},
    {"Base16: Tokyodark Terminal", 0, {{"#282c34", "#11121d", "#1a1b2a"}, {"#11121d", "#7199ee", "#7199ee"}}},
    {"Base16: Tokyodark", 0, {{"#353945", "#11121d", "#212234"}, {"#11121d", "#7199ee", "#7199ee"}}},
    {"Base16: Tomorrow Night Eighties", 0, {{"#999999", "#2d2d2d", "#393939"}, {"#2d2d2d", "#6699cc", "#6699cc"}}},
    {"Base16: Tomorrow Night", 0, {{"#969896", "#1d1f21", "#282a2e"}, {"#1d1f21", "#81a2be", "#81a2be"}}},
    {"Base16: Tomorrow", 1, {{"#b4b7b4", "#ffffff", "#e0e0e0"}, {"#1d1f21", "#4271ae", "#4271ae"}}},
    {"Base16: London Tube", 0, {{"#737171", "#231f20", "#1c3f95"}, {"#231f20", "#009ddc", "#009ddc"}}},
    {"Base16: Twilight", 0, {{"#5f5a60", "#1e1e1e", "#323537"}, {"#1e1e1e", "#7587a6", "#7587a6"}}},
    {"Base16: Unikitty Dark", 0, {{"#838085", "#2e2a31", "#4a464d"}, {"#2e2a31", "#796af5", "#796af5"}}},
    {"Base16: Unikitty Light", 1, {{"#a7a5a8", "#ffffff", "#e1e1e2"}, {"#322d34", "#775dff", "#775dff"}}},
    {"Base16: Unikitty Reversible", 0, {{"#878589", "#2e2a31", "#4b484e"}, {"#2e2a31", "#7864fa", "#7864fa"}}},
    {"Base16: UwUnicorn", 0, {{"#6c3cb2", "#241b26", "#2f2a3f"}, {"#241b26", "#6a9eb5", "#6a9eb5"}}},
    {"Base16: Valua", 0, {{"#3e5c53", "#131f1f", "#213132"}, {"#131f1f", "#4ed2d2", "#4ed2d2"}}},
    {"Base16: Vesper", 0, {{"#333333", "#101010", "#232323"}, {"#101010", "#8eaaaa", "#8eaaaa"}}},
    {"Base16: vice", 0, {{"#383a47", "#17191e", "#22262d"}, {"#17191e", "#00eaff", "#00eaff"}}},
    {"Base16: vulcan", 0, {{"#7a5759", "#041523", "#122339"}, {"#041523", "#977d7c", "#977d7c"}}},
    {"Base16: Windows 10 Light", 1, {{"#cccccc", "#f2f2f2", "#e5e5e5"}, {"#0c0c0c", "#0037da", "#0037da"}}},
    {"Base16: Windows 10", 0, {{"#767676", "#0c0c0c", "#2f2f2f"}, {"#0c0c0c", "#3b78ff", "#3b78ff"}}},
    {"Base16: Windows 95 Light", 1, {{"#a8a8a8", "#fcfcfc", "#e0e0e0"}, {"#000000", "#0000a8", "#0000a8"}}},
    {"Base16: Windows 95", 0, {{"#545454", "#000000", "#1c1c1c"}, {"#000000", "#5454fc", "#5454fc"}}},
    {"Base16: Windows High Contrast Light", 1, {{"#c0c0c0", "#fcfcfc", "#e8e8e8"}, {"#000000", "#000080", "#000080"}}},
    {"Base16: Windows High Contrast", 0, {{"#545454", "#000000", "#1c1c1c"}, {"#000000", "#5454fc", "#5454fc"}}},
    {"Base16: Windows NT Light", 1, {{"#c0c0c0", "#ffffff", "#eaeaea"}, {"#000000", "#000080", "#000080"}}},
    {"Base16: Windows NT", 0, {{"#808080", "#000000", "#2a2a2a"}, {"#000000", "#0000ff", "#0000ff"}}},
    {"Base16: Woodland", 0, {{"#9d8b70", "#231e18", "#302b25"}, {"#231e18", "#88a4d3", "#88a4d3"}}},
    {"Base16: XCode Dusk", 0, {{"#686a71", "#282b35", "#3d4048"}, {"#282b35", "#790ead", "#790ead"}}},
    {"Base16: Yesterday Bright", 0, {{"#a7adba", "#343d46", "#4f5b66"}, {"#343d46", "#7aa6da", "#7aa6da"}}},
    {"Base16: Yesterday Night", 0, {{"#a7adba", "#343d46", "#4f5b66"}, {"#343d46", "#81a2be", "#81a2be"}}},
    {"Base16: Yesterday", 0, {{"#969896", "#1d1f21", "#282a2e"}, {"#1d1f21", "#4271ae", "#4271ae"}}},
    {"Base16: Zenbones", 0, {{"#b77e64", "#191919", "#de6e7c"}, {"#191919", "#cf86c1", "#cf86c1"}}},
    {"Base16: Zenburn", 0, {{"#6f6f6f", "#383838", "#404040"}, {"#383838", "#7cb8bb", "#7cb8bb"}}},
};

static int color_theme_find_by_name(const char *name) {
  size_t i;
  if (!name || !name[0]) {
    return -1;
  }
  for (i = 0; i < LENGTH(color_themes); i++) {
    if (!strcmp(color_themes[i].name, name)) {
      return (int)i;
    }
  }
  return -1;
}

/* Applies one complete look to both bar_theme_dark and bar_theme_light (see
 * the ColorTheme comment above), persists the choice, and repaints. Safe to
 * call during setup() before selmon exists -- widget_apply_theme() already
 * guards that. */
static void color_theme_apply(size_t idx) {
  if (idx >= LENGTH(color_themes)) {
    return;
  }
  bar_theme_dark = color_themes[idx].colors;
  bar_theme_light = color_themes[idx].colors;
  active_color_theme = (int)idx;
  save_theme_mode_state();
  widget_apply_theme();
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
  const char *icon;

  if (active_color_theme >= 0) {
    icon = color_themes[active_color_theme].is_light ? icon_theme_light : icon_theme_dark;
    snprintf(buf, size, "%s", icon);
    return;
  }

  switch (theme_mode) {
  case ThemeModeDark:
    icon = icon_theme_dark;
    break;
  case ThemeModeLight:
    icon = icon_theme_light;
    break;
  case ThemeModeSystem:
  default:
    icon = icon_theme_auto;
    break;
  }
  snprintf(buf, size, "%s", icon);
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

/* Second line, if present, is the last color_themes[] entry picked from the
 * theme picker -- see color_theme_apply(). Absent/unrecognized just means
 * "none", not an error; older state files predate this line entirely. */
static void load_theme_mode_state(void) {
  char path[PATH_MAX];
  char line[64];
  FILE *fp;

  theme_mode = ThemeModeSystem;
  active_color_theme = -1;
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
  if (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    active_color_theme = color_theme_find_by_name(line);
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
  fprintf(fp, "%s\n", active_color_theme >= 0 ? color_themes[active_color_theme].name : "none");
  fclose(fp);
}

/* The classic 3-way toggle always means "plain dark/light/auto" -- so
 * clicking/scrolling it while a named theme from the picker is active
 * clears that override back to the original hardcoded look rather than
 * leaving stale picked colors that this toggle can no longer see. */
static void widget_cycle_theme_mode(int direction) {
  int next = (((int)theme_mode + direction) % ThemeModeCount + ThemeModeCount) %
             ThemeModeCount;

  theme_mode = (enum ThemeMode)next;
  if (active_color_theme >= 0) {
    active_color_theme = -1;
    set_default_bar_theme();
  }
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
    text = luaL_tolstring(L, i, &len);
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

/* Switches m to the named workspace (creating it if needed) and, if the
 * workspace being left is now empty and not visible elsewhere, removes it --
 * this is what keeps ad-hoc workspaces (including per-project ones) from
 * piling up. Shared by the Lua API and the project picker. */
static size_t focus_workspace_by_name(Monitor *m, const char *name) {
  size_t old_index = SIZE_MAX;
  size_t index;
  Monitor *mm;

  if (!m || !name) {
    return SIZE_MAX;
  }

  old_index = workspace_index_from_mask(m->tagset[m->seltags]);
  index = workspace_ensure(name);
  if (index == SIZE_MAX) {
    return SIZE_MAX;
  }

  workspace_view_mask(m, workspace_mask_from_index(index));

  if (old_index != SIZE_MAX && old_index < workspaces_len &&
      old_index != workspace_index_from_mask(m->tagset[m->seltags]) &&
      !workspace_has_clients(old_index) &&
      !workspace_is_visible_on_any_monitor(old_index)) {
    workspace_remove_index(old_index);
    focus(NULL);
    for (mm = mons; mm; mm = mm->next) {
      arrange(mm);
    }
    drawbars();
  }

  return workspace_find(name);
}

static int lua_mwm_focus_workspace(lua_State *L) {
  const char *name = NULL;
  size_t index;

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
  index = focus_workspace_by_name(selmon, name);
  if (index == SIZE_MAX) {
    return luaL_error(L, "unable to focus workspace");
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

/* Path of the project backing the currently focused workspace, or nil if the
 * current workspace isn't a known project. Lets an agent-status hook (or an
 * agent itself, via mwm-cli) ask "what project am I in". */
static int lua_mwm_current_project(lua_State *L) {
  const char *wsname = NULL;
  int idx;

  if (selmon) {
    wsname = workspace_name_from_mask(selmon->tagset[selmon->seltags]);
  }
  idx = wsname ? project_index_for_workspace_name(wsname) : -1;
  if (idx < 0) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushstring(L, projects[idx]);
  return 1;
}

static int lua_mwm_list_projects(lua_State *L) {
  size_t i;
  projects_ensure_loaded();
  lua_createtable(L, (int)projects_len, 0);
  for (i = 0; i < projects_len; i++) {
    lua_pushstring(L, projects[i]);
    lua_rawseti(L, -2, (int)i + 1);
  }
  return 1;
}

/* Switches selmon to a project's workspace by path or name, adding it to the
 * saved project list first if it's an unrecognized directory -- the same
 * behavior as submitting it in the picker. Lets a hook or agent request a
 * project switch over the control socket, e.g.
 * `mwm-cli -m 'mwm.switch_project("~/code/foo")'`. */
static int lua_mwm_switch_project(lua_State *L) {
  const char *arg = luaL_checkstring(L, 1);
  char path_buf[PATH_MAX];
  char wsname[256];
  int idx;
  size_t index;

  if (!selmon) {
    return luaL_error(L, "no monitor");
  }

  projects_ensure_loaded();
  idx = project_find_for_path(arg);
  if (idx < 0) {
    idx = project_index_for_workspace_name(arg);
  }
  if (idx >= 0) {
    snprintf(path_buf, sizeof(path_buf), "%s", projects[idx]);
  } else {
    project_expand_path(arg, path_buf, sizeof(path_buf));
    if (!projects_add(arg)) {
      return luaL_error(L, "not a directory: %s", arg);
    }
  }

  project_basename(path_buf, wsname, sizeof(wsname));
  index = focus_workspace_by_name(selmon, wsname);
  if (index == SIZE_MAX) {
    return luaL_error(L, "unable to switch project");
  }
  project_set_active(path_buf);
  lua_pushinteger(L, (lua_Integer)index + 1);
  return 1;
}

/* Overrides the command the project picker launches with Ctrl+Enter
 * (default "claude"); see mwm.agent_command() in the config. */
static int lua_mwm_agent_command(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  snprintf(agent_launch_command, sizeof(agent_launch_command), "%s", cmd);
  return 0;
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
  char display_name[sizeof(widget->name)];

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
    snprintf(display_name, sizeof(display_name), "%s", lua_tostring(L, -1));
  } else {
    snprintf(display_name, sizeof(display_name), "widget%zu", lua_widgets_len + 1);
  }
  lua_pop(L, 1);
  /* Routed through a local rather than snprintf'd straight from
   * widget->name into widget->text: same-struct field-to-field copies like
   * that trip GCC's _FORTIFY_SOURCE restrict check (it can't always prove
   * non-overlap once inlined with a variable array index), even though
   * name/text are genuinely disjoint fields. */
  snprintf(widget->name, sizeof(widget->name), "%s", display_name);
  snprintf(widget->text, sizeof(widget->text), "%s", display_name);

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

/* Shared by keybind/mousebind spec parsing: turns the modifier tokens
 * preceding the final key/button token into a mask. Recognizes
 * mod4/super/mod, shift, ctrl/control, alt/mod1. */
static int lua_parse_mods(char **tokens, int count, unsigned int *mod_out) {
  int i;
  unsigned int mod = 0;

  for (i = 0; i < count; i++) {
    char *m = tokens[i];
    while (*m == ' ') {
      m++;
    }
    if (!strcasecmp(m, "mod4") || !strcasecmp(m, "super") || !strcasecmp(m, "mod")) {
      mod |= Mod4Mask;
    } else if (!strcasecmp(m, "shift")) {
      mod |= ShiftMask;
    } else if (!strcasecmp(m, "ctrl") || !strcasecmp(m, "control")) {
      mod |= ControlMask;
    } else if (!strcasecmp(m, "alt") || !strcasecmp(m, "mod1")) {
      mod |= Mod1Mask;
    } else {
      return 0; /* unknown modifier name */
    }
  }
  *mod_out = mod;
  return 1;
}

/* Splits "a+b+c" on '+' into up to `max` tokens, returning the count.
 * Shared by the keybind and mousebind spec parsers. */
static int lua_split_spec(char *buf, char **tokens, int max) {
  char *tok;
  char *saveptr = NULL;
  int ntok = 0;

  tok = strtok_r(buf, "+", &saveptr);
  while (tok && ntok < max) {
    tokens[ntok++] = tok;
    tok = strtok_r(NULL, "+", &saveptr);
  }
  return ntok;
}

/* Parses "mod4+shift+q" style specs: zero or more '+'-separated modifier
 * names, then a key name resolved via XStringToKeysym -- so any key X
 * already knows a name for ("Return", "q", "F1", "XF86AudioMute", ...)
 * works without mwm needing its own keysym table. */
static int lua_parse_keybind(const char *spec, unsigned int *mod_out, KeySym *keysym_out) {
  char buf[128];
  char *tokens[8];
  int ntok;
  KeySym keysym;

  snprintf(buf, sizeof(buf), "%s", spec);
  ntok = lua_split_spec(buf, tokens, (int)LENGTH(tokens));
  if (ntok == 0) {
    return 0;
  }
  if (!lua_parse_mods(tokens, ntok - 1, mod_out)) {
    return 0;
  }

  {
    char *keyname = tokens[ntok - 1];
    while (*keyname == ' ') {
      keyname++;
    }
    keysym = XStringToKeysym(keyname);
    if (keysym == NoSymbol) {
      return 0;
    }
  }

  *keysym_out = keysym;
  return 1;
}

/* Maps a mwm.mousebind() context name to the internal Clk* click type.
 * ClkWidgetBar is deliberately not offered here -- widget-bar clicks are
 * fully handled (and consumed) before buttonpress() ever computes a click
 * type, so a binding on it could never fire. */
static int lua_parse_click_context(const char *name) {
  if (!strcasecmp(name, "tagbar")) {
    return ClkTagBar;
  }
  if (!strcasecmp(name, "layout") || !strcasecmp(name, "ltsymbol")) {
    return ClkLtSymbol;
  }
  if (!strcasecmp(name, "status") || !strcasecmp(name, "statustext")) {
    return ClkStatusText;
  }
  if (!strcasecmp(name, "title") || !strcasecmp(name, "wintitle")) {
    return ClkWinTitle;
  }
  if (!strcasecmp(name, "client") || !strcasecmp(name, "clientwin")) {
    return ClkClientWin;
  }
  if (!strcasecmp(name, "root")) {
    return ClkRootWin;
  }
  return -1;
}

/* Parses "mod4+button1" style specs: the same modifier tokens as
 * lua_parse_keybind, then a "button1".."button5" token. */
static int lua_parse_mousebind(const char *spec, unsigned int *mod_out, unsigned int *button_out) {
  char buf[128];
  char *tokens[8];
  int ntok;
  char *btnname;
  int btnnum;

  snprintf(buf, sizeof(buf), "%s", spec);
  ntok = lua_split_spec(buf, tokens, (int)LENGTH(tokens));
  if (ntok == 0) {
    return 0;
  }
  if (!lua_parse_mods(tokens, ntok - 1, mod_out)) {
    return 0;
  }

  btnname = tokens[ntok - 1];
  while (*btnname == ' ') {
    btnname++;
  }
  if (strncasecmp(btnname, "button", 6) != 0) {
    return 0;
  }
  btnnum = atoi(btnname + 6);
  if (btnnum < 1 || btnnum > 5) {
    return 0;
  }
  *button_out = (unsigned int)(Button1 + (btnnum - 1));
  return 1;
}

static void lua_key_invoke(size_t index) {
  int status;

  if (!command_lua || index >= lua_keys_len) {
    return;
  }
  lua_rawgeti(command_lua, LUA_REGISTRYINDEX, lua_keys[index].callback_ref);
  status = lua_pcall(command_lua, 0, 0, 0);
  if (status != LUA_OK) {
    fprintf(stderr, "mwm: keybind error: %s\n", lua_tostring(command_lua, -1));
    lua_pop(command_lua, 1);
  }
}

/* mwm.keybind("mod4+shift+Return", function() ... end, "Open a terminal") --
 * adds a global keybinding without recompiling. The description is optional
 * (defaults to empty) but is what shows up next to this binding in the
 * Mod+s keybinding-help overlay -- see keybind_help_open(). Re-grabs
 * immediately so it (and any removal via a config reload) takes effect
 * right away, including when called live over the control socket. */
static int lua_mwm_keybind(lua_State *L) {
  const char *spec = luaL_checkstring(L, 1);
  const char *desc = lua_isstring(L, 3) ? lua_tostring(L, 3) : "";
  unsigned int mod;
  KeySym keysym;

  luaL_checktype(L, 2, LUA_TFUNCTION);

  if (lua_keys_len >= MAX_LUA_KEYS) {
    return luaL_error(L, "too many keybindings (max %d)", MAX_LUA_KEYS);
  }
  if (!lua_parse_keybind(spec, &mod, &keysym)) {
    return luaL_error(L, "invalid keybind spec '%s'", spec);
  }

  lua_pushvalue(L, 2);
  lua_keys[lua_keys_len].mod = mod;
  lua_keys[lua_keys_len].keysym = keysym;
  lua_keys[lua_keys_len].callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  snprintf(lua_keys[lua_keys_len].desc, sizeof(lua_keys[lua_keys_len].desc), "%s", desc);
  lua_keys_len++;

  grabkeys();
  return 0;
}

static void reset_lua_keys(void) {
  /* Same reasoning as reset_lua_widgets(): the owning Lua state is about to
   * be closed and recreated, invalidating every callback_ref already. */
  lua_keys_len = 0;
}

/* mwm.rule({ class = "firefox", workspace = "web", floating = false }) --
 * applies after the compiled-in rules[], so a Lua rule always has the final
 * say for windows it matches. class/instance/title substring-match like
 * xprop's WM_CLASS/WM_NAME (case-sensitive, same as the compiled rules);
 * at least one of them must be given. */
static int lua_mwm_rule(lua_State *L) {
  LuaRule *rule;

  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_rules_len >= MAX_LUA_RULES) {
    return luaL_error(L, "too many rules (max %d)", MAX_LUA_RULES);
  }

  rule = &lua_rules[lua_rules_len];
  memset(rule, 0, sizeof(*rule));

  lua_getfield(L, 1, "class");
  if (lua_isstring(L, -1)) {
    snprintf(rule->class_name, sizeof(rule->class_name), "%s", lua_tostring(L, -1));
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "instance");
  if (lua_isstring(L, -1)) {
    snprintf(rule->instance, sizeof(rule->instance), "%s", lua_tostring(L, -1));
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "title");
  if (lua_isstring(L, -1)) {
    snprintf(rule->title, sizeof(rule->title), "%s", lua_tostring(L, -1));
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "workspace");
  if (lua_isstring(L, -1)) {
    snprintf(rule->workspace, sizeof(rule->workspace), "%s", lua_tostring(L, -1));
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "floating");
  if (!lua_isnil(L, -1)) {
    rule->has_isfloating = 1;
    rule->isfloating = lua_toboolean(L, -1);
  }
  lua_pop(L, 1);

  if (!rule->class_name[0] && !rule->instance[0] && !rule->title[0]) {
    return luaL_error(L, "rule needs at least one of class/instance/title to match on");
  }

  lua_rules_len++;
  return 0;
}

static void reset_lua_rules(void) { lua_rules_len = 0; }

static int lua_mwm_set_terminal(lua_State *L) {
  const char *cmd = luaL_checkstring(L, 1);
  snprintf(terminal_cmd, sizeof(terminal_cmd), "%s", cmd);
  return 0;
}

static int lua_mwm_set_mfact(lua_State *L) {
  double f = luaL_checknumber(L, 1);
  Monitor *m;

  if (f < 0.05) {
    f = 0.05;
  }
  if (f > 0.95) {
    f = 0.95;
  }
  mfact = (float)f;
  for (m = mons; m; m = m->next) {
    m->mfact = mfact;
  }
  if (selmon) {
    arrange(selmon);
  }
  drawbars();
  return 0;
}

static int lua_mwm_set_nmaster(lua_State *L) {
  int n = (int)luaL_checkinteger(L, 1);
  Monitor *m;

  if (n < 0) {
    n = 0;
  }
  nmaster = n;
  for (m = mons; m; m = m->next) {
    m->nmaster = nmaster;
  }
  if (selmon) {
    arrange(selmon);
  }
  drawbars();
  return 0;
}

static int lua_mwm_set_snap(lua_State *L) {
  int px = (int)luaL_checkinteger(L, 1);
  if (px < 0) {
    px = 0;
  }
  snap = (unsigned int)px;
  return 0;
}

/* Unlike mfact/nmaster, border width has to be pushed to every live client's
 * actual X window, not just recorded for the next one that's created.
 * Fullscreen clients are skipped (they're intentionally at bw=0) but get
 * their stashed oldbw updated, so they pick up the new width whenever they
 * next leave fullscreen instead of the width that was current when they
 * entered it. */
static int lua_mwm_set_border_width(lua_State *L) {
  int px = (int)luaL_checkinteger(L, 1);
  Monitor *m;
  Client *c;

  if (px < 0) {
    px = 0;
  }
  borderpx = (unsigned int)px;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->isfullscreen) {
        c->oldbw = (int)borderpx;
        continue;
      }
      c->bw = (int)borderpx;
      resizeclient(c, c->x, c->y, c->w, c->h);
    }
    arrange(m);
  }
  drawbars();
  return 0;
}

/* Unlike border width, gaps aren't stored per-client -- every layout reads
 * the global at arrange time via resize_cell(), so this just needs to
 * trigger a re-arrange. */
static int lua_mwm_set_gaps(lua_State *L) {
  int px = (int)luaL_checkinteger(L, 1);
  Monitor *m;

  if (px < 0) {
    px = 0;
  }
  gappx = px;
  for (m = mons; m; m = m->next) {
    arrange(m);
  }
  drawbars();
  return 0;
}

/* Same toggle Mod4+minus runs -- exposed so a custom keybind (or another
 * script driving mwm-cli) can trigger it too. */
static int lua_mwm_scratchpad_toggle(lua_State *L) {
  (void)L;
  scratchpad_toggle(NULL);
  return 0;
}

static int lua_mwm_scratchpad_set(lua_State *L) {
  (void)L;
  scratchpad_set(NULL);
  return 0;
}

/* mwm.zoom(), mwm.toggle_floating(), mwm.kill_client(), mwm.toggle_bar() --
 * thin wrappers around the same functions Mod4+z/Shift+space/Shift+c/b
 * already call. None of them read their Arg, so NULL is safe; each guards
 * on selmon existing since, unlike a keybind, these can be reached over the
 * control socket before any monitor is up. */
static int lua_mwm_zoom(lua_State *L) {
  (void)L;
  if (selmon) {
    zoom(NULL);
  }
  return 0;
}

static int lua_mwm_toggle_floating(lua_State *L) {
  (void)L;
  if (selmon) {
    togglefloating(NULL);
  }
  return 0;
}

static int lua_mwm_kill_client(lua_State *L) {
  (void)L;
  if (selmon) {
    killclient(NULL);
  }
  return 0;
}

static int lua_mwm_toggle_bar(lua_State *L) {
  (void)L;
  if (selmon) {
    togglebar(NULL);
  }
  return 0;
}

static void lua_button_invoke(size_t index, const char *wsname) {
  int status;
  int nargs = 0;

  if (!command_lua || index >= lua_buttons_len) {
    return;
  }
  lua_rawgeti(command_lua, LUA_REGISTRYINDEX, lua_buttons[index].callback_ref);
  if (wsname) {
    lua_pushstring(command_lua, wsname);
    nargs = 1;
  }
  status = lua_pcall(command_lua, nargs, 0, 0);
  if (status != LUA_OK) {
    fprintf(stderr, "mwm: mousebind error: %s\n", lua_tostring(command_lua, -1));
    lua_pop(command_lua, 1);
  }
}

/* Re-applies passive button grabs on every live client window. Needed
 * whenever the set of "client"-context Lua mouse bindings changes -- those
 * only actually intercept clicks on windows mwm has explicitly grabbed the
 * relevant button+modifier combo on (see grabbuttons()). */
static void regrab_all_client_buttons(void) {
  Monitor *m;
  Client *c;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      grabbuttons(c, c == m->sel);
    }
  }
}

/* mwm.mousebind("client", "mod4+button1", function() ... end) -- adds a
 * mouse binding without recompiling. `context` is one of tagbar, layout,
 * status, title, client, root (see lua_parse_click_context). Callbacks on
 * "tagbar" receive the clicked workspace's name as their one argument when
 * a workspace was actually clicked. */
static int lua_mwm_mousebind(lua_State *L) {
  const char *ctx_name = luaL_checkstring(L, 1);
  const char *spec = luaL_checkstring(L, 2);
  int click;
  unsigned int mod, button;

  luaL_checktype(L, 3, LUA_TFUNCTION);

  click = lua_parse_click_context(ctx_name);
  if (click < 0) {
    return luaL_error(L, "unknown mouse context '%s'", ctx_name);
  }
  if (!lua_parse_mousebind(spec, &mod, &button)) {
    return luaL_error(L, "invalid mousebind spec '%s'", spec);
  }
  if (lua_buttons_len >= MAX_LUA_BUTTONS) {
    return luaL_error(L, "too many mouse bindings (max %d)", MAX_LUA_BUTTONS);
  }

  lua_pushvalue(L, 3);
  lua_buttons[lua_buttons_len].click = click;
  lua_buttons[lua_buttons_len].mod = mod;
  lua_buttons[lua_buttons_len].button = button;
  lua_buttons[lua_buttons_len].callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_buttons_len++;

  if (click == ClkClientWin) {
    regrab_all_client_buttons();
  }
  return 0;
}

static void reset_lua_buttons(void) { lua_buttons_len = 0; }

/* Matches a layout symbol exactly (as compiled into layouts[]), or one of a
 * few friendly aliases for the three default layouts. Custom Lua layouts
 * (mwm.layout()) are looked up separately, by name, in lua_mwm_set_layout. */
static int lua_layout_index_by_symbol(const char *symbol) {
  static const struct {
    const char *alias;
    const char *symbol;
  } aliases[] = {
      {"tile", "[]="},
      {"floating", "><>"},
      {"float", "><>"},
      {"monocle", "[M]"},
  };
  size_t i, j;

  for (i = 0; i < LENGTH(layouts); i++) {
    if (!strcmp(layouts[i].symbol, symbol)) {
      return (int)i;
    }
  }
  for (i = 0; i < LENGTH(aliases); i++) {
    if (!strcasecmp(symbol, aliases[i].alias)) {
      for (j = 0; j < LENGTH(layouts); j++) {
        if (!strcmp(layouts[j].symbol, aliases[i].symbol)) {
          return (int)j;
        }
      }
    }
  }
  return -1;
}

/* mwm.set_layout(1), mwm.set_layout("monocle"), or mwm.set_layout("spiral")
 * for a name registered via mwm.layout() -- switches selmon's layout.
 * Needs a monitor to exist, so call this from a keybind or other runtime
 * hook rather than unconditionally at the top of config.lua. */
static int lua_mwm_set_layout(lua_State *L) {
  const Layout *target = NULL;
  Arg a = {0};

  if (!selmon) {
    return luaL_error(L, "no monitor yet");
  }

  if (lua_type(L, 1) == LUA_TNUMBER) {
    int idx = (int)luaL_checkinteger(L, 1);
    if (idx < 0 || idx >= (int)LENGTH(layouts)) {
      return luaL_error(L, "unknown layout");
    }
    target = &layouts[idx];
  } else {
    const char *name = luaL_checkstring(L, 1);
    int idx = lua_layout_index_by_symbol(name);
    size_t i;

    if (idx >= 0) {
      target = &layouts[idx];
    } else {
      for (i = 0; i < lua_layouts_len; i++) {
        if (!strcmp(lua_layouts[i].name, name)) {
          target = &lua_layout_entries[i];
          break;
        }
      }
      if (!target) {
        return luaL_error(L, "unknown layout '%s'", name);
      }
    }
  }

  a.v = target;
  setlayout(&a);
  return 0;
}

/* Pulls an integer geometry field out of the table on top of the Lua stack,
 * falling back to `fallback` if it's missing or not a number -- so a
 * layout function that only sets a couple of fields per client doesn't
 * have to fill in the rest. */
static int lua_geom_field(lua_State *L, const char *field, int fallback) {
  int val;
  lua_getfield(L, -1, field);
  val = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : fallback;
  lua_pop(L, 1);
  return val;
}

/* Runs a custom Lua layout's arrange function: builds a `clients` array
 * (one entry per currently-tiled client, same order tile()/monocle() would
 * use) and an `area` table (the monitor's usable rect plus nmaster/mfact,
 * in case the layout wants to honor them), calls the Lua function with
 * those two arguments, and applies the geometry table it returns -- one
 * entry per client, in the same order. Geometry is outer (border-inclusive)
 * to match how a person would describe "put the window here, this size";
 * the border gets subtracted back out before resize() (which wants content
 * dimensions) is called. A client left out of the returned table, or given
 * a non-table entry, simply keeps its current geometry. */
static void lua_layout_arrange(Monitor *m, size_t layout_idx) {
  LuaLayout *lay;
  Client *tiled[MAX_LUA_ARRANGE_CLIENTS];
  Client *c;
  unsigned int n = 0;
  size_t i;
  int status;

  if (!command_lua || layout_idx >= lua_layouts_len) {
    return;
  }
  lay = &lua_layouts[layout_idx];
  if (lay->arrange_ref == LUA_NOREF) {
    return;
  }

  for (c = nexttiled(m->clients); c && n < MAX_LUA_ARRANGE_CLIENTS; c = nexttiled(c->next)) {
    tiled[n++] = c;
  }
  if (n == 0) {
    return;
  }

  lua_rawgeti(command_lua, LUA_REGISTRYINDEX, lay->arrange_ref);

  lua_createtable(command_lua, (int)n, 0);
  for (i = 0; i < n; i++) {
    lua_createtable(command_lua, 0, 2);
    lua_pushstring(command_lua, tiled[i]->name);
    lua_setfield(command_lua, -2, "title");
    lua_pushinteger(command_lua, (lua_Integer)(i + 1));
    lua_setfield(command_lua, -2, "index");
    lua_rawseti(command_lua, -2, (int)(i + 1));
  }

  lua_createtable(command_lua, 0, 6);
  lua_pushinteger(command_lua, m->wx);
  lua_setfield(command_lua, -2, "x");
  lua_pushinteger(command_lua, m->wy);
  lua_setfield(command_lua, -2, "y");
  lua_pushinteger(command_lua, m->ww);
  lua_setfield(command_lua, -2, "w");
  lua_pushinteger(command_lua, m->wh);
  lua_setfield(command_lua, -2, "h");
  lua_pushinteger(command_lua, m->nmaster);
  lua_setfield(command_lua, -2, "nmaster");
  lua_pushnumber(command_lua, (lua_Number)m->mfact);
  lua_setfield(command_lua, -2, "mfact");

  status = lua_pcall(command_lua, 2, 1, 0);
  if (status != LUA_OK) {
    fprintf(stderr, "mwm: layout '%s' arrange error: %s\n", lay->name,
            lua_tostring(command_lua, -1));
    lua_pop(command_lua, 1);
    return;
  }
  if (!lua_istable(command_lua, -1)) {
    fprintf(stderr, "mwm: layout '%s' arrange must return a table of geometries\n", lay->name);
    lua_pop(command_lua, 1);
    return;
  }

  for (i = 0; i < n; i++) {
    Client *c2 = tiled[i];
    int x, y, w, h;

    lua_rawgeti(command_lua, -1, (int)(i + 1));
    if (!lua_istable(command_lua, -1)) {
      lua_pop(command_lua, 1);
      continue; /* not given a geometry -- leave it where it is */
    }
    x = lua_geom_field(command_lua, "x", c2->x);
    y = lua_geom_field(command_lua, "y", c2->y);
    w = lua_geom_field(command_lua, "w", WIDTH(c2));
    h = lua_geom_field(command_lua, "h", HEIGHT(c2));
    lua_pop(command_lua, 1);

    /* resize_cell applies the gap and subtracts the border itself, same as
     * the native layouts -- a Lua layout gets both for free. */
    resize_cell(c2, x, y, w, h, 0);
  }
  lua_pop(command_lua, 1);
}

/* The single arrange() entry point for every Lua layout -- Layout.arrange
 * can't carry the "which one" info itself, so it's recovered from
 * m->lt[m->sellt]->lua_index (see lua_layout_entries[]). */
static void lua_layout_dispatch(Monitor *m) {
  const Layout *lt = m->lt[m->sellt];
  if (lt->lua_index < 0 || (size_t)lt->lua_index >= lua_layouts_len) {
    return;
  }
  lua_layout_arrange(m, (size_t)lt->lua_index);
}

/* mwm.layout({ name = "spiral", symbol = "[@]", arrange = function(clients, area) ... end })
 * registers a custom tiling algorithm. `arrange` receives the tiled client
 * list and usable area described above and must return a table of
 * { x, y, w, h } geometries, one per client, in the same order. Select it
 * with mwm.set_layout("spiral"); it doesn't replace mwm's compiled layouts,
 * it's added alongside them. */
static int lua_mwm_layout(lua_State *L) {
  LuaLayout *lay;
  size_t idx;

  luaL_checktype(L, 1, LUA_TTABLE);
  if (lua_layouts_len >= MAX_LUA_LAYOUTS) {
    return luaL_error(L, "too many layouts (max %d)", MAX_LUA_LAYOUTS);
  }
  idx = lua_layouts_len;
  lay = &lua_layouts[idx];
  memset(lay, 0, sizeof(*lay));
  lay->arrange_ref = LUA_NOREF;

  lua_getfield(L, 1, "name");
  if (lua_isstring(L, -1)) {
    snprintf(lay->name, sizeof(lay->name), "%s", lua_tostring(L, -1));
  } else {
    snprintf(lay->name, sizeof(lay->name), "lua%zu", idx + 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "symbol");
  if (lua_isstring(L, -1)) {
    snprintf(lay->symbol, sizeof(lay->symbol), "%s", lua_tostring(L, -1));
  } else {
    snprintf(lay->symbol, sizeof(lay->symbol), "[?]");
  }
  lua_pop(L, 1);

  lua_getfield(L, 1, "arrange");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 1);
    return luaL_error(L, "layout '%s' needs an arrange function", lay->name);
  }
  lay->arrange_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_layout_entries[idx].symbol = lay->symbol;
  lua_layout_entries[idx].arrange = lua_layout_dispatch;
  lua_layout_entries[idx].lua_index = (int)idx;

  lua_layouts_len++;
  return 0;
}

static void reset_lua_layouts(void) { lua_layouts_len = 0; }

static int layout_is_lua(const Layout *lt) { return lt && lt->arrange == lua_layout_dispatch; }

/* Captures which of each monitor's two layout slots currently hold a Lua
 * layout, by name. Needed before a config reload: reset_lua_layouts() +
 * load_config_from_lua() reuse lua_layout_entries[] for whatever gets
 * registered next, so any m->lt[k] still pointing into it would otherwise
 * end up silently running a different layout (or a no-op) than the one the
 * user actually selected. Returns NULL (with *count_out left at 0) if
 * there are no monitors yet. */
static MonLayoutSnapshot *snapshot_lua_layouts(size_t *count_out) {
  Monitor *m;
  size_t n = 0;
  MonLayoutSnapshot *snap;
  size_t i;

  *count_out = 0;
  for (m = mons; m; m = m->next) {
    n++;
  }
  if (n == 0) {
    return NULL;
  }

  snap = ecalloc_type<MonLayoutSnapshot>(n);
  for (m = mons, i = 0; m; m = m->next, i++) {
    int k;
    for (k = 0; k < 2; k++) {
      if (layout_is_lua(m->lt[k])) {
        LuaLayout *lay = &lua_layouts[m->lt[k]->lua_index];
        snap[i].is_lua[k] = 1;
        snprintf(snap[i].name[k], sizeof(snap[i].name[k]), "%s", lay->name);
      }
    }
  }
  *count_out = n;
  return snap;
}

/* Restores each monitor's layout slots after a reload. A slot that held a
 * Lua layout by name gets re-pointed at whichever lua_layout_entries[]
 * index that name resolves to now -- even if it landed at a different
 * index than before, since the array gets reused across reloads -- or
 * falls back to the first compiled layout if that name wasn't
 * re-registered. Slots that held a compiled layout are left completely
 * alone: those Layout* point into the static const layouts[] array, whose
 * addresses never move. Frees `snap`. */
static void restore_lua_layouts(MonLayoutSnapshot *snap, size_t count) {
  Monitor *m;
  size_t i;

  if (!snap) {
    return;
  }
  for (m = mons, i = 0; m && i < count; m = m->next, i++) {
    int k;
    for (k = 0; k < 2; k++) {
      const Layout *replacement;
      size_t j;

      if (!snap[i].is_lua[k]) {
        continue;
      }
      replacement = &layouts[0];
      for (j = 0; j < lua_layouts_len; j++) {
        if (!strcmp(lua_layouts[j].name, snap[i].name[k])) {
          replacement = &lua_layout_entries[j];
          break;
        }
      }
      m->lt[k] = replacement;
    }
  }
  free(snap);
}

/* Number of switchable layouts: mwm's compiled-in ones plus whatever's been
 * registered with mwm.layout(). */
static size_t layout_total_count(void) { return LENGTH(layouts) + lua_layouts_len; }

/* Indexes into the same unified list layout_total_count() measures:
 * compiled layouts[] first (in their declared order), then Lua layouts (in
 * registration order). Returns NULL if idx is out of range. */
static const Layout *layout_at(size_t idx) {
  if (idx < LENGTH(layouts)) {
    return &layouts[idx];
  }
  idx -= LENGTH(layouts);
  if (idx < lua_layouts_len) {
    return &lua_layout_entries[idx];
  }
  return NULL;
}

/* Backs both the Mod4+Ctrl+space keybind and mwm.cycle_layout(): steps
 * selmon to the next (direction > 0) or previous layout in the unified
 * list, wrapping around either end -- the AwesomeWM-style "just keep
 * pressing the key" way to explore every available layout, as opposed to
 * setlayout()'s dwm-style toggle-between-the-last-two. */
static void cycle_layout(int direction) {
  size_t total;
  size_t i;
  int cur = -1;
  int next;
  Arg a = {0};

  if (!selmon) {
    return;
  }
  total = layout_total_count();
  if (total == 0) {
    return;
  }
  for (i = 0; i < total; i++) {
    if (layout_at(i) == selmon->lt[selmon->sellt]) {
      cur = (int)i;
      break;
    }
  }
  if (cur < 0) {
    cur = 0; /* current layout isn't in the list (shouldn't happen) -- start from the top */
  }

  next = ((cur + direction) % (int)total + (int)total) % (int)total;
  a.v = layout_at((size_t)next);
  setlayout(&a);
}

static void cyclelayout(const Arg *arg) { cycle_layout(arg->i >= 0 ? 1 : -1); }

static int lua_mwm_cycle_layout(lua_State *L) {
  lua_Integer delta = luaL_optinteger(L, 1, 1);
  cycle_layout(delta >= 0 ? 1 : -1);
  return 0;
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

/* Identifies an agent card across redraws by kind+cwd (agents don't have a
 * stable pid available for file-backed status entries). */
static unsigned int agent_row_id(const char *kind, const char *cwd) {
  char buf[16 + 384 + 2];
  snprintf(buf, sizeof(buf), "%s|%s", kind, cwd);
  return agent_hash_id(buf);
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

      {
        AgentStatus *existing = agent_list_find(new_list, comm, cwd_link);
        if (existing) {
          /* A status-file entry already represents this agent; just
           * recover its pid so click-to-jump can find the right window. */
          if (existing->pid <= 0) {
            existing->pid = (pid_t)atol(entry->d_name);
          }
          continue;
        }
      }

      a = ecalloc_type<AgentStatus>();
      snprintf(a->kind, sizeof(a->kind), "%s", comm);
      snprintf(a->status, sizeof(a->status), "running");
      snprintf(a->label, sizeof(a->label), "running");
      snprintf(a->cwd, sizeof(a->cwd), "%s", cwd_link);
      a->updated = time(NULL);
      a->from_file = 0;
      a->file_path[0] = '\0';
      a->pid = (pid_t)atol(entry->d_name);

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

/* Parent pid of `pid` via /proc/<pid>/stat, or 0 if unavailable. The comm
 * field can itself contain spaces or parens, so skip to the last ')' before
 * reading the state char and ppid that follow it. */
static pid_t proc_ppid(pid_t pid) {
  char path[64];
  char buf[512];
  FILE *fp;
  char *p;

  snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
  fp = fopen(path, "r");
  if (!fp) {
    return 0;
  }
  if (!fgets(buf, sizeof(buf), fp)) {
    fclose(fp);
    return 0;
  }
  fclose(fp);

  p = strrchr(buf, ')');
  if (!p) {
    return 0;
  }
  p++;
  while (*p == ' ') {
    p++;
  }
  while (*p && *p != ' ') { /* skip the single-char state field */
    p++;
  }
  return (pid_t)atol(p);
}

/* Reads _NET_WM_PID off a client window, or 0 if unset. */
static pid_t client_get_pid(Window w) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *data = NULL;
  pid_t pid = 0;

  if (XGetWindowProperty(dpy, w, netatom[NetWMPid], 0, 1, False, XA_CARDINAL,
                         &actual_type, &actual_format, &nitems, &bytes_after,
                         &data) == Success &&
      data) {
    if (actual_type == XA_CARDINAL && nitems > 0) {
      pid = (pid_t) * reinterpret_cast<unsigned long *>(data);
    }
    XFree(data);
  }
  return pid;
}

/* Finds the client window that owns `target`, walking up its process
 * ancestry (agent -> shell -> terminal emulator) since a terminal's WM_PID
 * is never the agent process's own pid. Bounded in case /proc is odd. */
static Client *client_for_pid(pid_t target) {
  Monitor *m;
  Client *c;
  int hops;

  if (target <= 0) {
    return NULL;
  }
  for (hops = 0; target > 1 && hops < 32; hops++) {
    for (m = mons; m; m = m->next) {
      for (c = m->clients; c; c = c->next) {
        if (client_get_pid(c->win) == target) {
          return c;
        }
      }
    }
    target = proc_ppid(target);
  }
  return NULL;
}

/* Switches to whichever of c's workspaces sorts lowest and focuses it. */
static void client_reveal(Client *c) {
  Monitor *m;
  WorkspaceMask mask;

  if (!c) {
    return;
  }
  m = c->mon ? c->mon : selmon;
  mask = c->tags & get_full_workspace_mask();
  if (mask) {
    workspace_view_mask(m, mask & (~mask + 1)); /* isolate lowest set bit */
  }
  selmon = m;
  focus(c);
  restack(m);
}

/* Jumps to `a`'s actual window when its pid resolves to one (found by
 * walking up its process ancestry to a client's WM_PID). Falls back to just
 * switching to the project workspace for its cwd -- the project workspace if
 * it falls under a saved project, otherwise the workspace matching its own
 * basename -- when no window is found (agent exited, or a stale/pid-less
 * status-file entry). Never creates a workspace from a click. */
static void agent_jump_to(const AgentStatus *a) {
  Client *c;
  int idx;
  char wsname[256];
  size_t wsidx;

  if (!a || !selmon) {
    return;
  }

  if (a->pid > 0) {
    c = client_for_pid(a->pid);
    if (c) {
      client_reveal(c);
      return;
    }
  }

  idx = project_find_for_path(a->cwd);
  if (idx >= 0) {
    project_basename(projects[idx], wsname, sizeof(wsname));
  } else {
    project_basename(a->cwd, wsname, sizeof(wsname));
  }

  wsidx = workspace_find(wsname);
  if (wsidx == SIZE_MAX) {
    return;
  }
  workspace_view_mask(selmon, workspace_mask_from_index(wsidx));
}

/* Cycles to the next agent that needs input, wrapping around. Lets you clear
 * a queue of blocked agents with one repeated keypress instead of hunting
 * through the sidebar. */
static void agent_jump_next_needs_input(const Arg *arg) {
  AgentStatus *a;
  AgentStatus *first_needs = NULL;
  AgentStatus *after_last = NULL;
  int seen_last = (agent_last_jumped_id == 0);
  (void)arg;

  agents_refresh();

  for (a = agents; a; a = a->next) {
    if (strcmp(a->status, "needs_input") != 0) {
      continue;
    }
    if (!first_needs) {
      first_needs = a;
    }
    if (seen_last && !after_last) {
      after_last = a;
    }
    if (agent_row_id(a->kind, a->cwd) == agent_last_jumped_id) {
      seen_last = 1;
    }
  }

  a = after_last ? after_last : first_needs;
  if (!a) {
    return;
  }
  agent_last_jumped_id = agent_row_id(a->kind, a->cwd);
  agent_jump_to(a);
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

      char cwd_label[384];
      project_label_for_path(a->cwd, cwd_label, sizeof(cwd_label));
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, agent_row_pad, row_y, max_w, bh, 0, cwd_label, 0);
      row_y += bh;

      if (agent_row_layout_len < MAX_AGENTS) {
        AgentRowLayout *row = &agent_row_layout[agent_row_layout_len++];
        row_id = agent_row_id(a->kind, a->cwd);
        row->id = row_id;
        row->card_y = card_y;
        row->card_h = card_h;
        row->dismiss_id = 0;
        row->close_x = row->close_y = row->close_w = row->close_h = 0;
        if (a->from_file) {
          row->dismiss_id = agent_hash_id(a->file_path);
          row->close_w = close_w;
          row->close_h = bh;
          row->close_x = close_x;
          row->close_y = card_y;
          drw_setscheme(drw, scheme[SchemeNorm]);
          drw_text(drw, row->close_x, row->close_y, row->close_w, row->close_h,
                   0, icon_close, 0);
        }
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
    if (row->close_w > 0 && ev->x >= row->close_x &&
        ev->x < row->close_x + row->close_w && ev->y >= row->close_y &&
        ev->y < row->close_y + row->close_h) {
      AgentStatus *a;
      for (a = agents; a; a = a->next) {
        if (a->from_file && agent_hash_id(a->file_path) == row->dismiss_id) {
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

  for (i = 0; i < agent_row_layout_len; i++) {
    AgentRowLayout *row = &agent_row_layout[i];
    if (ev->y >= row->card_y && ev->y < row->card_y + row->card_h) {
      AgentStatus *a;
      for (a = agents; a; a = a->next) {
        if (agent_row_id(a->kind, a->cwd) == row->id) {
          agent_jump_to(a);
          break;
        }
      }
      agent_sidebar_close();
      return 1;
    }
  }

  return 1;
}

/* }}} agent sidebar */

/* {{{ launcher (app index -- searched from within the project picker,
 * see the "project picker" section below for the actual UI) */

static int launcher_app_cmp(const void *a, const void *b) {
  const char *sa = *(const char *const *)a;
  const char *sb = *(const char *const *)b;
  size_t i;
  for (i = 0; sa[i] && sb[i]; i++) {
    int ca = tolower((unsigned char)sa[i]);
    int cb = tolower((unsigned char)sb[i]);
    if (ca != cb) {
      return ca - cb;
    }
  }
  return (unsigned char)sa[i] - (unsigned char)sb[i];
}

static int launcher_app_exists(const char *name) {
  size_t i;
  for (i = 0; i < launcher_apps_len; i++) {
    if (!strcmp(launcher_apps[i], name)) {
      return 1;
    }
  }
  return 0;
}

static void launcher_app_add(const char *name) {
  if (launcher_app_exists(name)) {
    return;
  }
  if (launcher_apps_len == launcher_apps_cap) {
    size_t newcap = launcher_apps_cap ? launcher_apps_cap * 2 : 256;
    char **grown = (char **)realloc(launcher_apps, newcap * sizeof(char *));
    if (!grown) {
      return;
    }
    launcher_apps = grown;
    launcher_apps_cap = newcap;
  }
  launcher_apps[launcher_apps_len++] = xstrdup_local(name);
}

/* Scans every directory on $PATH for executables, the same set dmenu_run
 * would offer, so the launcher works without any external dmenu/rofi. */
static void launcher_scan_apps(void) {
  const char *path_env = getenv("PATH");
  char *path_copy;
  char *saveptr = NULL;
  char *dir;

  if (!path_env || !path_env[0]) {
    return;
  }
  path_copy = xstrdup_local(path_env);

  for (dir = strtok_r(path_copy, ":", &saveptr); dir;
       dir = strtok_r(NULL, ":", &saveptr)) {
    DIR *dp = opendir(dir);
    struct dirent *de;
    if (!dp) {
      continue;
    }
    while ((de = readdir(dp)) != NULL) {
      char fullpath[PATH_MAX];
      struct stat st;
      if (de->d_name[0] == '.') {
        continue;
      }
      snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, de->d_name);
      if (stat(fullpath, &st) != 0 || !S_ISREG(st.st_mode)) {
        continue;
      }
      if (access(fullpath, X_OK) != 0) {
        continue;
      }
      launcher_app_add(de->d_name);
    }
    closedir(dp);
  }
  free(path_copy);

  qsort(launcher_apps, launcher_apps_len, sizeof(char *), launcher_app_cmp);
}

/* }}} launcher */

/* {{{ project picker */

static void get_projects_state_path(char *buf, size_t size) {
  const char *xdg_data_home;
  const char *home;

  if (!buf || size == 0) {
    return;
  }
  xdg_data_home = getenv("XDG_DATA_HOME");
  if (xdg_data_home && xdg_data_home[0] != '\0') {
    snprintf(buf, size, "%s/mwm/projects", xdg_data_home);
    return;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(buf, size, "%s/.local/share/mwm/projects", home);
    return;
  }
  buf[0] = '\0';
}

static void projects_load(void) {
  char path[PATH_MAX];
  char line[PATH_MAX];
  FILE *fp;

  get_projects_state_path(path, sizeof(path));
  if (path[0] == '\0') {
    return;
  }
  fp = fopen(path, "r");
  if (!fp) {
    return;
  }
  while (projects_len < MAX_PROJECTS && fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\n")] = '\0';
    if (line[0] == '\0') {
      continue;
    }
    if (projects_len == projects_cap) {
      size_t newcap = projects_cap ? projects_cap * 2 : 32;
      char **grown = (char **)realloc(projects, newcap * sizeof(char *));
      if (!grown) {
        break;
      }
      projects = grown;
      projects_cap = newcap;
    }
    projects[projects_len++] = xstrdup_local(line);
  }
  fclose(fp);
}

static void projects_save(void) {
  char path[PATH_MAX];
  char dir[PATH_MAX];
  char *slash;
  FILE *fp;
  size_t i;

  get_projects_state_path(path, sizeof(path));
  if (path[0] == '\0') {
    return;
  }
  snprintf(dir, sizeof(dir), "%s", path);
  slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    mkdir(dir, 0755);
  }
  fp = fopen(path, "w");
  if (!fp) {
    return;
  }
  for (i = 0; i < projects_len; i++) {
    fprintf(fp, "%s\n", projects[i]);
  }
  fclose(fp);
}

/* Expands a leading "~" the way a shell would, so users can type "~/code/foo". */
static void project_expand_path(const char *raw, char *out, size_t outsz) {
  const char *home;

  if (raw[0] == '~' && (raw[1] == '/' || raw[1] == '\0')) {
    home = getenv("HOME");
    if (home) {
      snprintf(out, outsz, "%s%s", home, raw + 1);
      return;
    }
  }
  snprintf(out, outsz, "%s", raw);
}

static int projects_add(const char *raw_path) {
  char expanded[PATH_MAX];
  struct stat st;
  size_t i;

  if (!raw_path || raw_path[0] == '\0') {
    return 0;
  }
  project_expand_path(raw_path, expanded, sizeof(expanded));
  if (stat(expanded, &st) != 0 || !S_ISDIR(st.st_mode)) {
    return 0;
  }

  for (i = 0; i < projects_len; i++) {
    if (!strcmp(projects[i], expanded)) {
      return 1; /* already present */
    }
  }

  if (projects_len >= MAX_PROJECTS) {
    return 0;
  }
  if (projects_len == projects_cap) {
    size_t newcap = projects_cap ? projects_cap * 2 : 32;
    char **grown = (char **)realloc(projects, newcap * sizeof(char *));
    if (!grown) {
      return 0;
    }
    projects = grown;
    projects_cap = newcap;
  }
  projects[projects_len++] = xstrdup_local(expanded);
  projects_save();
  return 1;
}

static void projects_remove_index(size_t idx) {
  size_t i;

  if (idx >= projects_len) {
    return;
  }
  free(projects[idx]);
  for (i = idx; i + 1 < projects_len; i++) {
    projects[i] = projects[i + 1];
  }
  projects_len--;
  projects_save();
}

/* Moves projects[idx] to the front, shifting the rest down one -- called on
 * every successful switch, so an empty-query picker naturally lists
 * most-recently-used first (project_filter()'s no-query branch just walks
 * projects[] in array order already). */
static void projects_move_to_front(size_t idx) {
  char *moved;
  size_t i;

  if (idx == 0 || idx >= projects_len) {
    return;
  }
  moved = projects[idx];
  for (i = idx; i > 0; i--) {
    projects[i] = projects[i - 1];
  }
  projects[0] = moved;
  projects_save();
}

/* Collapses a leading $HOME into "~" for display -- the inverse of
 * project_expand_path(). */
static void project_collapse_home(const char *path, char *out, size_t outsz) {
  const char *home = getenv("HOME");
  size_t homelen;

  if (home && home[0] && path) {
    homelen = strlen(home);
    if (!strncmp(path, home, homelen) && (path[homelen] == '/' || path[homelen] == '\0')) {
      snprintf(out, outsz, "~%s", path + homelen);
      return;
    }
  }
  snprintf(out, outsz, "%s", path ? path : "");
}

/* Is any currently-known agent (running() or needs_input()) rooted at
 * `path` or a subdirectory of it? Purely an in-memory scan of the agents
 * list already maintained on the normal widget refresh cadence -- no
 * subprocess, safe to call on every picker redraw. */
static int project_has_running_agent(const char *path) {
  AgentStatus *a;
  size_t plen = strlen(path);

  for (a = agents; a; a = a->next) {
    if (!strncmp(a->cwd, path, plen) && (a->cwd[plen] == '/' || a->cwd[plen] == '\0')) {
      return 1;
    }
  }
  return 0;
}

static void projects_ensure_loaded(void) {
  if (!projects_loaded) {
    projects_load();
    projects_loaded = 1;
  }
}

/* Last path component, with any trailing slashes ignored. This is the
 * workspace name a project switches to: ~/code/foo -> workspace "foo". $HOME
 * itself is special-cased to "default" -- its basename would otherwise just
 * be the username, and the whole point of the default project is that it's
 * called "default". */
static void project_basename(const char *path, char *out, size_t outsz) {
  char tmp[PATH_MAX];
  size_t len;
  const char *base;
  const char *home = getenv("HOME");

  if (path && home && home[0] && !strcmp(path, home)) {
    snprintf(out, outsz, "default");
    return;
  }

  snprintf(tmp, sizeof(tmp), "%s", path ? path : "");
  len = strlen(tmp);
  while (len > 1 && tmp[len - 1] == '/') {
    tmp[--len] = '\0';
  }
  base = strrchr(tmp, '/');
  snprintf(out, outsz, "%s", base ? base + 1 : tmp);
}

/* Longest-prefix match of `path` against the saved project list, e.g. an
 * agent cwd of ~/code/foo/sub matches project ~/code/foo. Returns -1 if no
 * project contains this path. */
static int project_find_for_path(const char *path) {
  size_t i;
  size_t path_len;
  size_t best_len = 0;
  int best = -1;

  if (!path || !path[0]) {
    return -1;
  }
  projects_ensure_loaded();
  path_len = strlen(path);
  for (i = 0; i < projects_len; i++) {
    size_t plen = strlen(projects[i]);
    if (plen == 0 || plen > path_len) {
      continue;
    }
    if (strncmp(projects[i], path, plen) != 0) {
      continue;
    }
    if (path[plen] != '\0' && path[plen] != '/') {
      continue; /* prefix match must land on a path boundary */
    }
    if (plen > best_len) {
      best_len = plen;
      best = (int)i;
    }
  }
  return best;
}

/* Human-friendly label for a path: "myproject" or "myproject/sub/dir" when
 * it belongs to a known project, otherwise the path unchanged. */
static void project_label_for_path(const char *path, char *out, size_t outsz) {
  int idx = project_find_for_path(path);
  char base[256];
  const char *rel;
  size_t plen;

  if (idx < 0) {
    snprintf(out, outsz, "%s", path ? path : "");
    return;
  }
  project_basename(projects[idx], base, sizeof(base));
  plen = strlen(projects[idx]);
  rel = path + plen;
  while (*rel == '/') {
    rel++;
  }
  if (*rel) {
    snprintf(out, outsz, "%s/%s", base, rel);
  } else {
    snprintf(out, outsz, "%s", base);
  }
}

/* Is `wsname` the workspace of one of the saved projects? Returns its index
 * into projects[], or -1. */
static int project_index_for_workspace_name(const char *wsname) {
  size_t i;
  char base[256];

  if (!wsname || !wsname[0]) {
    return -1;
  }
  projects_ensure_loaded();
  for (i = 0; i < projects_len; i++) {
    project_basename(projects[i], base, sizeof(base));
    if (!strcmp(base, wsname)) {
      return (int)i;
    }
  }
  return -1;
}

/* Switches m to `path`'s project workspace (created on first visit, reused
 * after -- same auto-GC as any other workspace). This is what makes
 * "switch to a project" mean "scope to that project's workspace" instead of
 * just opening a terminal in its directory. */
static void project_switch_workspace(Monitor *m, const char *path) {
  char wsname[256];
  project_basename(path, wsname, sizeof(wsname));
  focus_workspace_by_name(m, wsname);
}

/* Records `path` as the active project shown in drawbar()'s top-left corner
 * (see active_project_path) and repaints immediately. Separate from
 * project_switch_workspace() -- the two usually happen together on a real
 * project switch, but the label tracks "which project am I working in"
 * independent of whatever workspace/tag is currently in view. */
static void project_set_active(const char *path) {
  if (!path) {
    return;
  }
  snprintf(active_project_path, sizeof(active_project_path), "%s", path);
  drawbars();
}

/* Called once from setup(): $HOME is always the active project at startup
 * (project_basename() displays it as "default"), and saved to projects[] so
 * it also shows up in the picker. Deliberately doesn't touch workspaces --
 * unlike a real project switch, there's no workspace to jump to yet, and no
 * "default" workspace is pre-created either (only a later, explicit switch
 * to it creates one, same as any other project). */
static void project_init_default(void) {
  const char *home = getenv("HOME");
  if (!home || !home[0]) {
    return;
  }
  projects_add(home);
  snprintf(active_project_path, sizeof(active_project_path), "%s", home);
}

/* Resolves what Enter/Ctrl+Enter in the picker should act on: the selected
 * row, or -- if the query doesn't match anything -- the typed path, added as
 * a new project. */
/* Resolves what Enter/Ctrl+Enter should treat as a project directory: the
 * selected row (if it's a project -- app rows are handled separately by the
 * caller before this is ever reached), or the typed query added as a new
 * project if it's a valid directory. */
static int project_resolve_selected_path(char *out, size_t outsz) {
  if (project_filtered_len > 0 && project_filtered[project_sel].kind == PickResultProject) {
    snprintf(out, outsz, "%s", projects[project_filtered[project_sel].index]);
    return 1;
  }
  /* AppOnly and Theme have no notion of "add as a project" -- an unmatched
   * query there should just fall through to project_launch()'s
   * run-as-shell-command case, the same as rofi's drun mode. */
  if ((project_mode == PickerModeCombined || project_mode == PickerModeProjectOnly) &&
      project_filtered_len == 0 && project_query_len > 0) {
    project_expand_path(project_query, out, outsz);
    return projects_add(project_query);
  }
  return 0;
}

/* Enter/Ctrl+Enter from the picker. In order: the selected row is a theme --
 * apply it and close, with_agent doesn't apply. The selected row is an app
 * -- just launch it. The selected row (or the typed query) is a project
 * directory -- switch to its workspace, opening a terminal with a coding
 * agent running in it only when with_agent is set (Ctrl+Enter); plain Enter
 * just switches. Otherwise -- nothing matched and it's not a directory
 * either -- run the typed query as a shell command, the same fallback
 * dmenu/rofi's "run" mode offers. */
static void project_launch(int with_agent) {
  Arg a = {0};
  const char *argv_term[5];
  char path_buf[PATH_MAX];

  if (project_filtered_len > 0 && project_filtered[project_sel].kind == PickResultTheme) {
    color_theme_apply((size_t)project_filtered[project_sel].index);
    project_close();
    return;
  }

  if (project_filtered_len > 0 && project_filtered[project_sel].kind == PickResultApp) {
    const char *argv_app[2];
    argv_app[0] = launcher_apps[project_filtered[project_sel].index];
    argv_app[1] = NULL;
    a.v = argv_app;
    spawn(&a);
    project_close();
    return;
  }

  if (!project_resolve_selected_path(path_buf, sizeof(path_buf))) {
    if (project_query_len > 0) {
      const char *argv_shell[4];
      argv_shell[0] = "/bin/sh";
      argv_shell[1] = "-c";
      argv_shell[2] = project_query;
      argv_shell[3] = NULL;
      a.v = argv_shell;
      spawn(&a);
      project_close();
    }
    return;
  }

  {
    size_t i;
    for (i = 0; i < projects_len; i++) {
      if (!strcmp(projects[i], path_buf)) {
        projects_move_to_front(i);
        break;
      }
    }
  }

  project_switch_workspace(project_mon ? project_mon : selmon, path_buf);
  project_set_active(path_buf);

  if (with_agent) {
    argv_term[0] = terminal_cmd;
    argv_term[1] = "-d";
    argv_term[2] = path_buf;
    argv_term[3] = agent_launch_command;
    argv_term[4] = NULL;
    a.v = argv_term;
    spawn(&a);
  }
  project_close();
}

static void widget_format_projects(char *buf, size_t size) {
  const char *wsname = NULL;

  if (selmon) {
    wsname = workspace_name_from_mask(selmon->tagset[selmon->seltags]);
  }
  if (wsname && project_index_for_workspace_name(wsname) >= 0) {
    snprintf(buf, size, "%s %s", icon_projects, wsname);
  } else if (projects_len > 0) {
    snprintf(buf, size, "%s %zu", icon_projects, projects_len);
  } else {
    snprintf(buf, size, "%s", icon_projects);
  }
}

/* Subsequence fuzzy match a la fzf: -1 if needle isn't a subsequence of hay,
 * otherwise a score that rewards consecutive runs and word-boundary starts. */
static int project_fuzzy_score(const char *needle, size_t needle_len, const char *hay) {
  size_t hay_len = strlen(hay);
  size_t hi = 0, ni;
  int score = 0;
  int consecutive = 0;
  size_t last_match = (size_t)-1;

  if (needle_len == 0) {
    return 0;
  }

  for (ni = 0; ni < needle_len; ni++) {
    int nc = tolower((unsigned char)needle[ni]);
    int found = 0;
    for (; hi < hay_len; hi++) {
      int hc = tolower((unsigned char)hay[hi]);
      if (hc == nc) {
        found = 1;
        if (last_match != (size_t)-1 && hi == last_match + 1) {
          consecutive++;
          score += 5 + consecutive;
        } else {
          consecutive = 0;
          score += 1;
        }
        if (hi == 0 || hay[hi - 1] == '/' || hay[hi - 1] == '-' || hay[hi - 1] == '_') {
          score += 3;
        }
        last_match = hi;
        hi++;
        break;
      }
    }
    if (!found) {
      return -1;
    }
  }
  score -= (int)((hay_len - needle_len) / 8);
  /* -1 is reserved to mean "no match" (checked via score >= 0 by callers),
   * so a real match's length penalty must never push it below 0. */
  if (score < 0) {
    score = 0;
  }
  return score;
}

static int project_match_cmp(const void *a, const void *b) {
  const ProjectMatch *ma = (const ProjectMatch *)a;
  const ProjectMatch *mb = (const ProjectMatch *)b;
  return mb->score - ma->score;
}

/* An empty query lists only saved projects (most-recently-used first, per
 * projects_move_to_front()) -- that's this picker's home behavior, and
 * dumping every $PATH executable the moment it opens would bury it in
 * noise. Once you start typing, projects and apps are scored with the same
 * fuzzy matcher and merged into one ranked list, so the single most
 * relevant result wins regardless of which kind it is.
 *
 * project_mode narrows all of the above to a single kind: ProjectOnly and
 * AppOnly each pull from just their own loop below (AppOnly's empty-query
 * case lists apps in their scanned/alphabetical order, since there's no
 * saved/MRU app list to fall back to); Theme is exclusive of both and only
 * ever matches color_themes[]. Combined is the only mode that runs both the
 * project and app loops together. */
static void project_filter(void) {
  static ProjectMatch all[MAX_PICK_CANDIDATES];
  size_t all_len = 0;
  size_t i;
  int want_projects = project_mode == PickerModeCombined || project_mode == PickerModeProjectOnly;
  int want_apps = project_mode == PickerModeCombined || project_mode == PickerModeAppOnly;

  project_sel = 0;

  if (project_mode == PickerModeTheme) {
    for (i = 0; i < LENGTH(color_themes) && all_len < MAX_PICK_CANDIDATES; i++) {
      int score = project_query_len > 0
                      ? project_fuzzy_score(project_query, project_query_len, color_themes[i].name)
                      : 0;
      if (score >= 0) {
        all[all_len].index = (int)i;
        all[all_len].score = score;
        all[all_len].kind = PickResultTheme;
        all_len++;
      }
    }
    if (project_query_len > 0) {
      qsort(all, all_len, sizeof(ProjectMatch), project_match_cmp);
    }
  } else if (project_query_len == 0) {
    if (want_projects) {
      for (i = 0; i < projects_len && all_len < MAX_PICK_CANDIDATES; i++) {
        all[all_len].index = (int)i;
        all[all_len].score = 0;
        all[all_len].kind = PickResultProject;
        all_len++;
      }
    } else if (want_apps) {
      for (i = 0; i < launcher_apps_len && all_len < MAX_PICK_CANDIDATES; i++) {
        all[all_len].index = (int)i;
        all[all_len].score = 0;
        all[all_len].kind = PickResultApp;
        all_len++;
      }
    }
  } else {
    if (want_projects) {
      for (i = 0; i < projects_len && all_len < MAX_PICK_CANDIDATES; i++) {
        int score = project_fuzzy_score(project_query, project_query_len, projects[i]);
        if (score >= 0) {
          all[all_len].index = (int)i;
          all[all_len].score = score;
          all[all_len].kind = PickResultProject;
          all_len++;
        }
      }
    }
    if (want_apps) {
      for (i = 0; i < launcher_apps_len && all_len < MAX_PICK_CANDIDATES; i++) {
        int score = project_fuzzy_score(project_query, project_query_len, launcher_apps[i]);
        if (score >= 0) {
          all[all_len].index = (int)i;
          all[all_len].score = score;
          all[all_len].kind = PickResultApp;
          all_len++;
        }
      }
    }
    qsort(all, all_len, sizeof(ProjectMatch), project_match_cmp);
  }

  /* Keep every match, not just the first screenful -- project_scroll picks
   * which PROJECT_MAX_RESULTS-row window of this is on screen. */
  project_filtered_len = all_len;
  for (i = 0; i < project_filtered_len; i++) {
    project_filtered[i] = all[i];
  }
  project_scroll = 0;

  /* The result count just changed, so the popup's height needs to follow --
   * every caller of project_filter() gets this for free instead of having
   * to remember to reposition itself. */
  project_position(project_mon);
}

/* Slides project_scroll so project_sel's row is inside the visible
 * PROJECT_MAX_RESULTS-row window -- called after every selection change.
 * Also re-clamps scroll on its own (a shrinking list, e.g. after Ctrl+D
 * removes a project, can leave it past the new end). */
static void project_ensure_sel_visible(void) {
  if (project_filtered_len == 0) {
    project_scroll = 0;
    return;
  }
  if ((size_t)project_sel < project_scroll) {
    project_scroll = (size_t)project_sel;
  } else if ((size_t)project_sel >= project_scroll + PROJECT_MAX_RESULTS) {
    project_scroll = (size_t)project_sel - PROJECT_MAX_RESULTS + 1;
  }
  if (project_filtered_len <= PROJECT_MAX_RESULTS) {
    project_scroll = 0;
  } else if (project_scroll > project_filtered_len - PROJECT_MAX_RESULTS) {
    project_scroll = project_filtered_len - PROJECT_MAX_RESULTS;
  }
}

/* Moves the selection by `delta` (+1/-1 for arrow keys or Ctrl+N/P),
 * wrapping around the *entire* filtered list rather than just the visible
 * window -- that's what lets Ctrl+N walk past the first screenful instead
 * of cycling the same 8 rows forever. */
static void project_move_sel(int delta) {
  int len;

  if (project_filtered_len == 0) {
    return;
  }
  len = (int)project_filtered_len;
  project_sel = ((project_sel + delta) % len + len) % len;
  project_ensure_sel_visible();
}

/* Sized to fit the actual result count (at least one row, for the hint
 * text when there are none) rather than always reserving the full
 * PROJECT_MAX_RESULTS -- a one-match query shouldn't leave a tall empty
 * popup hanging below it. */
static int project_total_h(void) {
  size_t rows = project_filtered_len > 0 ? project_filtered_len : 1;
  if (rows > PROJECT_MAX_RESULTS) {
    rows = PROJECT_MAX_RESULTS;
  }
  return bh + 1 + (int)rows * bh;
}

static void project_position(Monitor *m) {
  int w = project_w;
  int h = project_total_h();
  int x, y;

  if (!m || project_win == None) {
    return;
  }
  x = m->wx + (m->ww - w) / 2;
  y = m->wy + (m->wh - h) / 3;
  if (x < m->wx) {
    x = m->wx;
  }
  if (y < m->wy) {
    y = m->wy;
  }
  XMoveResizeWindow(dpy, project_win, x, y, w, h);
}

static void project_draw(void) {
  int total_h = project_total_h();
  int y;
  size_t i;
  char display[PICKER_QUERY_MAX + 32];
  const char *prefix;

  if (project_win == None) {
    return;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, project_w, total_h, 1, 1);

  /* Combined keeps the plain "> " prompt it's always had; the single-scope
   * modes get their icon as a prefix so it's visually obvious which picker
   * came up, since Mod+space/Mod+p/Mod+o/Mod+Shift+t now open four
   * different ones. */
  prefix = project_mode == PickerModeProjectOnly ? icon_projects
           : project_mode == PickerModeAppOnly  ? icon_launcher
           : project_mode == PickerModeTheme    ? icon_theme_auto
                                                 : ">";
  /* "(sel/total)" only shows up once there's more than one screenful --
   * it's the only hint that Ctrl+N/P keep going past row 8 instead of
   * wrapping, so it matters most exactly when the list is long. */
  if (project_filtered_len > PROJECT_MAX_RESULTS) {
    snprintf(display, sizeof(display), "%s %s (%d/%zu)", prefix, project_query,
             project_sel + 1, project_filtered_len);
  } else {
    snprintf(display, sizeof(display), "%s %s", prefix, project_query);
  }
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, 0, 0, project_w, bh, lrpad / 2, display, 0);

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, bh, project_w, 1, 1, 0);

  y = bh + 1;
  if (project_filtered_len == 0) {
    const char *hint;
    if (project_mode == PickerModeAppOnly) {
      hint = project_query_len > 0 ? "No matching apps -- Enter runs this as a command"
                                    : "No apps found";
    } else if (project_mode == PickerModeTheme) {
      hint = "No matching themes";
    } else if (project_mode == PickerModeProjectOnly) {
      hint = project_query_len > 0 ? "No matching projects -- Enter adds this path as one"
             : projects_len == 0   ? "No projects yet -- type a path to add one"
                                   : "No projects";
    } else if (projects_len == 0 && project_query_len == 0) {
      hint = "No projects yet -- type a path to add one, or search for an app";
    } else if (project_query_len > 0) {
      hint = "No matches -- Enter runs this as a command, or adds it as a project";
    } else {
      hint = "No projects";
    }
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, 0, y, project_w, bh, lrpad / 2, hint, 0);
  } else {
    const char *cur_wsname =
        selmon ? workspace_name_from_mask(selmon->tagset[selmon->seltags]) : NULL;
    size_t shown = project_filtered_len - project_scroll;
    if (shown > PROJECT_MAX_RESULTS) {
      shown = PROJECT_MAX_RESULTS;
    }

    for (i = 0; i < shown; i++) {
      size_t idx = project_scroll + i;
      const ProjectMatch *match = &project_filtered[idx];
      char row_text[PATH_MAX + 384];
      /* Only saved projects are removable via the close button -- apps come
       * from $PATH and themes from the built-in table, neither of which is
       * "yours" to delete. */
      int close_w = match->kind == PickResultProject ? bh : 0;
      int close_x = project_w - close_w;

      if (match->kind == PickResultApp) {
        snprintf(row_text, sizeof(row_text), "%s %s", icon_launcher,
                 launcher_apps[match->index]);
      } else if (match->kind == PickResultTheme) {
        const ColorTheme *ct = &color_themes[match->index];
        snprintf(row_text, sizeof(row_text), "%s %s%s",
                 ct->is_light ? icon_theme_light : icon_theme_dark, ct->name,
                 match->index == active_color_theme ? " (active)" : "");
      } else {
        const char *path = projects[match->index];
        char base[256];
        char collapsed[PATH_MAX];
        char prefix[64] = "";

        project_basename(path, base, sizeof(base));
        project_collapse_home(path, collapsed, sizeof(collapsed));

        if (project_has_running_agent(path)) {
          snprintf(prefix, sizeof(prefix), "%s ", icon_agents);
        }
        if (cur_wsname && !strcmp(cur_wsname, base)) {
          size_t plen = strlen(prefix);
          snprintf(prefix + plen, sizeof(prefix) - plen, "• ");
        }
        snprintf(row_text, sizeof(row_text), "%s%s  %s", prefix, base, collapsed);
      }

      drw_setscheme(drw, scheme[idx == (size_t)project_sel ? SchemeSel : SchemeNorm]);
      drw_text(drw, 0, y, close_x, bh, lrpad / 2, row_text, 0);

      project_row_layout[i].close_x = close_x;
      project_row_layout[i].close_y = y;
      project_row_layout[i].close_w = close_w;
      project_row_layout[i].close_h = bh;
      if (match->kind == PickResultProject) {
        drw_setscheme(drw, scheme[SchemeNorm]);
        drw_text(drw, close_x, y, bh, bh, 0, icon_close, 0);
      }
      y += bh;
    }
  }

  drw_map(drw, project_win, 0, 0, project_w, total_h);
}

static void project_open(Monitor *m, enum PickerMode mode) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }
  projects_ensure_loaded();
  if (launcher_apps_len == 0) {
    launcher_scan_apps();
  }
  if (project_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask;
    project_win = XCreateWindow(
        dpy, root, 0, 0, project_w, project_total_h(), 1,
        DefaultDepth(dpy, screen), CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, project_win, cursor[CurNormal]->cursor);
  }
  if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) !=
      GrabSuccess) {
    return;
  }

  project_mon = m;
  project_mode = mode;
  project_visible = 1;
  project_query[0] = '\0';
  project_query_len = 0;
  project_filter(); /* also positions/sizes the popup for the initial result count */
  project_draw();
  XMapRaised(dpy, project_win);
  drawbars();
}

static void project_close(void) {
  if (!project_visible) {
    return;
  }
  project_visible = 0;
  XUngrabKeyboard(dpy, CurrentTime);
  if (project_win != None) {
    XUnmapWindow(dpy, project_win);
  }
  drawbars();
}

/* Same mode pressed again closes it; a different mode's key while it's
 * already open switches the picker to that mode instead of stacking or
 * requiring a close first -- e.g. Mod+p while the project picker is up
 * jumps straight to the app launcher. */
static void project_toggle(Monitor *m, enum PickerMode mode) {
  if (project_visible && project_mode == mode) {
    project_close();
  } else {
    project_open(m ? m : selmon, mode);
  }
}

static void project_toggle_key(const Arg *arg) {
  project_toggle(selmon, (enum PickerMode)arg->i);
}

static void project_submit(void) { project_launch(0); }

static void project_submit_agent(void) { project_launch(1); }

static void project_remove_selected(void) {
  if (project_filtered_len == 0 || project_filtered[project_sel].kind != PickResultProject) {
    return; /* apps aren't a saved list -- nothing to remove */
  }
  projects_remove_index((size_t)project_filtered[project_sel].index);
  project_filter();
  project_draw();
}

static int project_handle_keypress(XKeyEvent *ev) {
  char buf[64];
  KeySym keysym = NoSymbol;
  int len = XLookupString(ev, buf, sizeof(buf), &keysym, NULL);
  unsigned int state = CLEANMASK(ev->state);
  int i;

  switch (keysym) {
  case XK_Escape:
    project_close();
    return 1;
  case XK_Return:
  case XK_KP_Enter:
    if (state == ControlMask) {
      project_submit_agent();
    } else {
      project_submit();
    }
    return 1;
  case XK_BackSpace:
    if (project_query_len > 0) {
      project_query[--project_query_len] = '\0';
      project_filter();
      project_draw();
    }
    return 1;
  case XK_Up:
    project_move_sel(-1);
    project_draw();
    return 1;
  case XK_Down:
  case XK_Tab:
    project_move_sel(+1);
    project_draw();
    return 1;
  default:
    break;
  }

  if (state == ControlMask && keysym == XK_u) {
    project_query_len = 0;
    project_query[0] = '\0';
    project_filter();
    project_draw();
    return 1;
  }
  if (state == ControlMask && (keysym == XK_d || keysym == XK_x)) {
    project_remove_selected();
    return 1;
  }
  if (state == ControlMask && keysym == XK_p) {
    project_move_sel(-1);
    project_draw();
    return 1;
  }
  if (state == ControlMask && keysym == XK_n) {
    project_move_sel(+1);
    project_draw();
    return 1;
  }

  for (i = 0; i < len; i++) {
    if (!isprint((unsigned char)buf[i])) {
      continue;
    }
    if (project_query_len + 1 >= sizeof(project_query)) {
      break;
    }
    project_query[project_query_len++] = buf[i];
    project_query[project_query_len] = '\0';
  }
  if (len > 0) {
    project_filter();
    project_draw();
    return 1;
  }
  return 0;
}

static int project_handle_button(XButtonPressedEvent *ev) {
  size_t i;

  if (!project_visible) {
    return 0;
  }
  if (ev->window != project_win) {
    project_close();
    return 0;
  }
  if (ev->button == Button4) {
    project_move_sel(-1);
    project_draw();
    return 1;
  }
  if (ev->button == Button5) {
    project_move_sel(+1);
    project_draw();
    return 1;
  }
  if (ev->button != Button1) {
    return 1;
  }
  /* project_row_layout[] only holds the currently-visible rows (indices
   * 0..shown-1, see project_draw()) -- project_filtered_len can be much
   * larger now that it's the whole match list, so this must walk the same
   * visible window rather than the full list. */
  {
    size_t shown = project_filtered_len - project_scroll;
    if (shown > PROJECT_MAX_RESULTS) {
      shown = PROJECT_MAX_RESULTS;
    }
    for (i = 0; i < shown; i++) {
      size_t idx = project_scroll + i;
      ProjectRowLayout *row = &project_row_layout[i];
      if (ev->x >= row->close_x && ev->x < row->close_x + row->close_w &&
          ev->y >= row->close_y && ev->y < row->close_y + row->close_h) {
        projects_remove_index((size_t)project_filtered[idx].index);
        project_filter();
        project_draw();
        return 1;
      }
      if (ev->y >= row->close_y && ev->y < row->close_y + row->close_h) {
        project_sel = (int)idx;
        project_submit();
        return 1;
      }
    }
  }
  return 1;
}

/* }}} project picker */

/* {{{ keybinding help (Mod+s) */

/* "Super+Shift+" style prefix for a modifier mask -- mirrors the modifier
 * names lua_parse_mods() accepts, so a binding's on-screen label matches
 * what you'd type in mwm.keybind(). */
static void keybind_help_mod_string(unsigned int mod, char *out, size_t outsz) {
  size_t len = 0;

  out[0] = '\0';
  if (mod & Mod4Mask) {
    len += (size_t)snprintf(out + len, outsz - len, "Super+");
  }
  if (mod & ControlMask) {
    len += (size_t)snprintf(out + len, outsz - len, "Ctrl+");
  }
  if (mod & ShiftMask) {
    len += (size_t)snprintf(out + len, outsz - len, "Shift+");
  }
  if (mod & Mod1Mask) {
    len += (size_t)snprintf(out + len, outsz - len, "Alt+");
  }
}

/* XKeysymToString() gives X's internal names ("space", "Return", "minus")
 * -- translate the common ones to what a person would actually call the
 * key, and capitalize bare lowercase letters ("p" -> "P"). */
static void keybind_help_key_string(KeySym keysym, char *out, size_t outsz) {
  const char *raw = XKeysymToString(keysym);

  if (!raw) {
    snprintf(out, outsz, "?");
    return;
  }
  if (!strcmp(raw, "space")) {
    raw = "Space";
  } else if (!strcmp(raw, "Return")) {
    raw = "Enter";
  } else if (!strcmp(raw, "minus")) {
    raw = "-";
  } else if (!strcmp(raw, "comma")) {
    raw = ",";
  } else if (!strcmp(raw, "period")) {
    raw = ".";
  } else if (!strcmp(raw, "colon")) {
    raw = ":";
  } else if (!strcmp(raw, "Escape")) {
    raw = "Esc";
  }
  snprintf(out, outsz, "%s", raw);
  if (out[0] != '\0' && out[1] == '\0' && out[0] >= 'a' && out[0] <= 'z') {
    out[0] = (char)(out[0] - 'a' + 'A');
  }
}

/* Builds the flat list of every keybinding to show: the compiled-in keys[]
 * (all of which carry a real description) plus any live mwm.keybind()
 * registrations -- a blank description there gets a placeholder rather
 * than being hidden, since an undocumented binding is still worth knowing
 * exists. Returns the count written to `out`. */
static size_t keybind_help_collect(KeybindHelpEntry *out, size_t cap) {
  size_t n = 0;
  size_t i;

  for (i = 0; i < LENGTH(keys) && n < cap; i++) {
    out[n].mod = keys[i].mod;
    out[n].keysym = keys[i].keysym;
    out[n].desc = keys[i].desc && keys[i].desc[0] ? keys[i].desc : "(no description)";
    n++;
  }
  for (i = 0; i < lua_keys_len && n < cap; i++) {
    out[n].mod = lua_keys[i].mod;
    out[n].keysym = lua_keys[i].keysym;
    out[n].desc = lua_keys[i].desc[0] ? lua_keys[i].desc : "(no description)";
    n++;
  }
  return n;
}

/* Lays every entry out column-major (fills column 0 top-to-bottom before
 * starting column 1) so related bindings declared near each other in
 * keys[] end up in the same or adjacent column instead of scattered by
 * row-major wraparound. Column count follows from how many rows fit in the
 * monitor's height; key-combo width is one shared column across the whole
 * table so every description lines up, but each column gets its own
 * description width so a handful of long entries in one column don't
 * stretch every other column too. */
static void keybind_help_draw(void) {
  static KeybindHelpEntry entries[MAX_KEYBIND_HELP_ENTRIES];
  size_t total;
  size_t idx;
  int keycombo_w = 0;
  int gap;
  int rows_per_col;
  int columns;
  int col_desc_w[32] = {0};
  int col_x[32];
  int win_w, win_h;
  int win_x, win_y;
  int cx;
  Monitor *m = keybind_help_mon ? keybind_help_mon : selmon;

  if (keybind_help_win == None || !m) {
    return;
  }

  total = keybind_help_collect(entries, LENGTH(entries));
  gap = lrpad;

  for (idx = 0; idx < total; idx++) {
    char modstr[32], keystr[32], combo[64];
    int w;

    keybind_help_mod_string(entries[idx].mod, modstr, sizeof(modstr));
    keybind_help_key_string(entries[idx].keysym, keystr, sizeof(keystr));
    snprintf(combo, sizeof(combo), "%s%s", modstr, keystr);
    w = (int)TEXTW(combo);
    if (w > keycombo_w) {
      keycombo_w = w;
    }
  }

  /* rows_per_col is derived straight from the height budget the window is
   * later clamped to (m->wh - 2*gap), so win_h never has to be shrunk out
   * from under a row layout that already assumed more space -- that
   * mismatch would otherwise clip the bottom rows of the last column. */
  rows_per_col = (m->wh - 2 * gap - bh) / bh;
  if (rows_per_col < 8) {
    rows_per_col = 8;
  }

  /* Column count follows from rows_per_col, but a table with many columns
   * of short rows can end up wider than the screen; if so, trade columns
   * for rows (up to a generous cap) until the natural width fits, rather
   * than clamping win_w afterward and clipping the rightmost column(s). */
  for (;;) {
    columns = total == 0 ? 1 : (int)((total + (size_t)rows_per_col - 1) / (size_t)rows_per_col);
    if (columns < 1) {
      columns = 1;
    }
    if (columns > (int)LENGTH(col_desc_w)) {
      columns = (int)LENGTH(col_desc_w);
      rows_per_col = (int)((total + (size_t)columns - 1) / (size_t)columns);
    }

    for (idx = 0; idx < (size_t)columns; idx++) {
      col_desc_w[idx] = 0;
    }
    for (idx = 0; idx < total; idx++) {
      int col = (int)(idx / (size_t)rows_per_col);
      int w = (int)TEXTW(entries[idx].desc);
      if (col >= columns) {
        col = columns - 1;
      }
      if (w > col_desc_w[col]) {
        col_desc_w[col] = w;
      }
    }

    cx = gap;
    for (idx = 0; idx < (size_t)columns; idx++) {
      col_x[idx] = cx;
      cx += keycombo_w + gap + col_desc_w[idx] + gap;
    }

    if (cx <= m->ww - 2 * gap || columns <= 1 || rows_per_col >= (int)total) {
      break;
    }
    rows_per_col += 4;
  }

  win_w = cx;
  win_h = bh + 2 * gap + rows_per_col * bh;
  if (win_w > m->ww - 2 * gap) {
    win_w = m->ww - 2 * gap;
  }
  win_x = m->wx + (m->ww - win_w) / 2;
  win_y = m->wy + (m->wh - win_h) / 2;
  if (win_y < m->wy) {
    win_y = m->wy; /* very many entries on a short monitor -- better clipped at the bottom than off the top */
  }
  XMoveResizeWindow(dpy, keybind_help_win, win_x, win_y, win_w, win_h);

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, win_w, win_h, 1, 1);
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, 0, 0, win_w, bh, lrpad / 2, "Keybindings -- press any key to close", 0);
  draw_hdivider(0, bh, win_w);

  for (idx = 0; idx < total; idx++) {
    int col = (int)(idx / (size_t)rows_per_col);
    int row;
    int cy;
    char modstr[32], keystr[32], combo[64];

    if (col >= columns) {
      col = columns - 1;
    }
    row = (int)(idx - (size_t)col * (size_t)rows_per_col);
    cy = bh + gap + row * bh;

    keybind_help_mod_string(entries[idx].mod, modstr, sizeof(modstr));
    keybind_help_key_string(entries[idx].keysym, keystr, sizeof(keystr));
    snprintf(combo, sizeof(combo), "%s%s", modstr, keystr);

    drw_setscheme(drw, scheme[SchemeSel]);
    drw_text(drw, col_x[col], cy, keycombo_w, bh, 0, combo, 0);
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, col_x[col] + keycombo_w + gap, cy, col_desc_w[col], bh, 0,
             entries[idx].desc, 0);
  }

  drw_map(drw, keybind_help_win, 0, 0, win_w, win_h);
}

static void keybind_help_open(Monitor *m) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }
  if (keybind_help_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    keybind_help_win = XCreateWindow(
        dpy, root, 0, 0, 1, 1, 1, DefaultDepth(dpy, screen), CopyFromParent,
        DefaultVisual(dpy, screen),
        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, keybind_help_win, cursor[CurNormal]->cursor);
  }
  if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) !=
      GrabSuccess) {
    return;
  }
  keybind_help_mon = m;
  keybind_help_visible = 1;
  keybind_help_draw();
  XMapRaised(dpy, keybind_help_win);
}

static void keybind_help_close(void) {
  if (!keybind_help_visible) {
    return;
  }
  keybind_help_visible = 0;
  XUngrabKeyboard(dpy, CurrentTime);
  if (keybind_help_win != None) {
    XUnmapWindow(dpy, keybind_help_win);
  }
}

static void keybind_help_toggle(Monitor *m) {
  if (keybind_help_visible) {
    keybind_help_close();
  } else {
    keybind_help_open(m ? m : selmon);
  }
}

static void keybind_help_toggle_key(const Arg *arg) {
  (void)arg;
  keybind_help_toggle(selmon);
}

/* Read-only overlay -- any key or click just dismisses it rather than
 * needing Escape or Mod+s specifically. */
static int keybind_help_handle_keypress(XKeyEvent *ev) {
  (void)ev;
  if (!keybind_help_visible) {
    return 0;
  }
  keybind_help_close();
  return 1;
}

static int keybind_help_handle_button(XButtonPressedEvent *ev) {
  (void)ev;
  if (!keybind_help_visible) {
    return 0;
  }
  keybind_help_close();
  return 1;
}

/* }}} keybinding help */

/* {{{ wifi popup */

/* Splits one nmcli -t (terse, colon-separated) line into up to `max`
 * fields in place, undoing nmcli's own escaping of literal ':' and '\\'
 * within a field value ("\:" / "\\\\"). Returns the field count found. */
static int nmcli_split_line(char *line, char *fields[], int max) {
  int n = 0;
  char *out = line;
  char *read = line;

  if (max <= 0) {
    return 0;
  }
  fields[n++] = out;
  while (*read) {
    if (read[0] == '\\' && (read[1] == ':' || read[1] == '\\')) {
      *out++ = read[1];
      read += 2;
      continue;
    }
    if (*read == ':') {
      *out++ = '\0';
      read++;
      if (n < max) {
        fields[n++] = out;
      }
      continue;
    }
    *out++ = *read++;
  }
  *out = '\0';
  return n;
}

/* Strongest signal first; whichever network is currently active always
 * sorts to the top regardless of signal, since that's the one answer to
 * "what am I on right now" someone opening this popup usually wants first. */
static int wifi_network_cmp(const void *a, const void *b) {
  const WifiNetwork *wa = (const WifiNetwork *)a;
  const WifiNetwork *wb = (const WifiNetwork *)b;
  if (wa->active != wb->active) {
    return wb->active - wa->active;
  }
  return wb->signal - wa->signal;
}

/* Parses `nmcli -t -f SSID,SIGNAL,SECURITY,ACTIVE device wifi list` output.
 * nmcli lists one row per BSSID, so the same SSID can appear more than once
 * (dual-band APs, mesh nodes) -- collapsed here into one entry per SSID,
 * keeping the strongest signal seen and OR-ing "active" across its rows. */
static void wifi_parse_scan_output(char *buf) {
  char *saveptr = NULL;
  char *line;

  wifi_networks_len = 0;
  line = strtok_r(buf, "\n", &saveptr);
  while (line && wifi_networks_len < MAX_WIFI_NETWORKS) {
    char *fields[4];
    int n = nmcli_split_line(line, fields, 4);

    if (n >= 1 && fields[0][0] != '\0') {
      size_t i;
      int is_dup = 0;
      int signal = n >= 2 ? atoi(fields[1]) : -1;
      int active = n >= 4 && !strcmp(fields[3], "yes");

      for (i = 0; i < wifi_networks_len; i++) {
        if (!strcmp(wifi_networks[i].ssid, fields[0])) {
          is_dup = 1;
          if (signal > wifi_networks[i].signal) {
            wifi_networks[i].signal = signal;
          }
          if (active) {
            wifi_networks[i].active = 1;
          }
          break;
        }
      }
      if (!is_dup) {
        WifiNetwork *w = &wifi_networks[wifi_networks_len++];
        snprintf(w->ssid, sizeof(w->ssid), "%s", fields[0]);
        w->signal = signal;
        w->secured = n >= 3 && fields[2][0] != '\0';
        w->active = active;
      }
    }
    line = strtok_r(NULL, "\n", &saveptr);
  }
  qsort(wifi_networks, wifi_networks_len, sizeof(WifiNetwork), wifi_network_cmp);
  if (wifi_sel >= (int)wifi_networks_len) {
    wifi_sel = wifi_networks_len > 0 ? (int)wifi_networks_len - 1 : 0;
  }
}

static void wifi_job_reset(void) {
  if (wifi_job_fd >= 0) {
    close(wifi_job_fd);
    wifi_job_fd = -1;
  }
  wifi_job_pid = -1;
  wifi_job_kind = WifiJobNone;
  wifi_job_buf_len = 0;
}

/* Forks and execs argv (no shell -- an SSID or password is passed through
 * as its own argv entry, nothing to escape) with stdout (and, if
 * capture_stderr, stderr too) captured through a non-blocking pipe. Never
 * waits: the child is reaped automatically (setup() sets SA_NOCLDWAIT on
 * SIGCHLD), and wifi_job_poll(), called every spin of the main loop,
 * notices when the pipe hits EOF. This is what keeps a multi-second scan or
 * connect attempt from freezing the whole window manager the way a plain
 * system()/popen() call would (see the volume slider's own history of
 * exactly this bug). */
static int wifi_job_start(enum WifiJobKind kind, char *const argv[], int capture_stderr) {
  int pipefd[2];
  pid_t pid;

  if (wifi_job_kind != WifiJobNone) {
    return 0;
  }
  if (pipe(pipefd) != 0) {
    return 0;
  }
  pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
  }
  if (pid == 0) {
    if (dpy) {
      close(ConnectionNumber(dpy));
    }
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    if (capture_stderr) {
      dup2(pipefd[1], STDERR_FILENO);
    } else {
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
      }
    }
    close(pipefd[1]);
    execvp(argv[0], argv);
    _exit(127);
  }
  close(pipefd[1]);
  fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
  wifi_job_kind = kind;
  wifi_job_pid = pid;
  wifi_job_fd = pipefd[0];
  wifi_job_buf_len = 0;
  return 1;
}

static void wifi_start_scan(void) {
  char *argv[] = {(char *)"nmcli",  (char *)"-t",     (char *)"-f", (char *)"SSID,SIGNAL,SECURITY,ACTIVE",
                  (char *)"device", (char *)"wifi",   (char *)"list", NULL};
  if (wifi_job_start(WifiJobScan, argv, 0)) {
    wifi_scanning = 1;
  }
}

static void wifi_start_connect(const char *ssid, const char *password) {
  char *argv[8];
  int i = 0;

  argv[i++] = (char *)"nmcli";
  argv[i++] = (char *)"device";
  argv[i++] = (char *)"wifi";
  argv[i++] = (char *)"connect";
  argv[i++] = (char *)ssid;
  if (password && password[0]) {
    argv[i++] = (char *)"password";
    argv[i++] = (char *)password;
  }
  argv[i] = NULL;

  if (wifi_job_start(WifiJobConnect, argv, 1)) {
    wifi_connecting = 1;
    snprintf(wifi_connecting_ssid, sizeof(wifi_connecting_ssid), "%s", ssid);
    wifi_status_msg[0] = '\0';
  }
}

/* Drains whatever's ready on the pending job's pipe without blocking;
 * called from run()'s main loop on every pass (its select() also waits on
 * this fd, so a result shows up as soon as it's ready rather than on the
 * next 1s poll). EOF means the child is done producing output. A connect
 * failure isn't detectable via exit status (SA_NOCLDWAIT means it's not
 * retrievable at all), so it's detected the same way run_capture_argv's
 * callers already have to work around system()'s untrustworthy return
 * value elsewhere in this file: nmcli connect failures print "Error: ..."
 * to stderr, which wifi_start_connect() captures into the same buffer as
 * stdout specifically so this string check works. */
static void wifi_job_poll(void) {
  enum WifiJobKind finished_kind;

  if (wifi_job_kind == WifiJobNone) {
    return;
  }

  for (;;) {
    ssize_t n = read(wifi_job_fd, wifi_job_buf + wifi_job_buf_len,
                      sizeof(wifi_job_buf) - 1 - wifi_job_buf_len);
    if (n > 0) {
      wifi_job_buf_len += (size_t)n;
      if (wifi_job_buf_len >= sizeof(wifi_job_buf) - 1) {
        break;
      }
      continue;
    }
    if (n < 0 && errno == EAGAIN) {
      return; /* still running, nothing new yet */
    }
    break; /* EOF or a real read error -- either way the job is done */
  }

  wifi_job_buf[wifi_job_buf_len] = '\0';
  finished_kind = wifi_job_kind;
  wifi_job_reset();

  if (finished_kind == WifiJobScan) {
    wifi_scanning = 0;
    wifi_parse_scan_output(wifi_job_buf);
  } else if (finished_kind == WifiJobConnect) {
    wifi_connecting = 0;
    /* Empty output counts as failure too -- a real success always prints
     * some confirmation ("Device '...' successfully activated..."); empty
     * means nmcli is missing, execvp failed, or it was killed, none of
     * which should be reported as a successful connection. */
    if (wifi_job_buf[0] == '\0' || strstr(wifi_job_buf, "Error")) {
      snprintf(wifi_status_msg, sizeof(wifi_status_msg), "Couldn't connect to %s",
               wifi_connecting_ssid);
      wifi_password_mode = 1;
      snprintf(wifi_password_ssid, sizeof(wifi_password_ssid), "%s", wifi_connecting_ssid);
      wifi_password_query[0] = '\0';
      wifi_password_query_len = 0;
    } else {
      snprintf(wifi_status_msg, sizeof(wifi_status_msg), "Connected to %s", wifi_connecting_ssid);
      wifi_password_mode = 0;
      wifi_start_scan(); /* refresh so the list reflects the new active network */
    }
  }

  if (wifi_visible) {
    wifi_popup_position(wifi_mon);
    wifi_popup_draw();
  }
}

static int wifi_popup_total_h(void) {
  int rows;

  if (wifi_blocked) {
    return bh + 1 + bh;
  }
  if (wifi_password_mode) {
    return bh + 1 + bh;
  }
  rows = (int)wifi_networks_len;
  if (rows > MAX_WIFI_LIST_ROWS) {
    rows = MAX_WIFI_LIST_ROWS;
  }
  if (rows < 1) {
    rows = 1; /* "Scanning..." / "No networks found" */
  }
  return bh + 1 + rows * bh + (wifi_status_msg[0] != '\0' ? bh : 0);
}

static void wifi_popup_position(Monitor *m) {
  int h = wifi_popup_total_h();
  int anchor_x, anchor_w;
  int popup_x, popup_y;

  if (!m || wifi_win == None) {
    return;
  }
  anchor_x = widget_layout[WidgetWifi].x;
  anchor_w = widget_layout[WidgetWifi].w;

  popup_x = m->wx + anchor_x + (anchor_w / 2) - (wifi_popup_w / 2);
  if (popup_x < m->wx) {
    popup_x = m->wx;
  }
  if (popup_x + wifi_popup_w > m->wx + m->ww) {
    popup_x = m->wx + m->ww - wifi_popup_w;
  }
  popup_y = m->bby - h - 8;
  if (popup_y < m->my) {
    popup_y = m->my;
  }
  XMoveResizeWindow(dpy, wifi_win, popup_x, popup_y, wifi_popup_w, h);
}

static void wifi_popup_draw(void) {
  int total_h = wifi_popup_total_h();
  int y;
  char toggle_text[64];

  if (wifi_win == None) {
    return;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, wifi_popup_w, total_h, 1, 1);

  /* Row 0 is the toggle button -- the whole row is the click target, see
   * wifi_handle_button(). */
  snprintf(toggle_text, sizeof(toggle_text), "%s Wi-Fi: %s", icon_wifi,
           wifi_blocked ? "Off (click to turn on)" : "On (click to turn off)");
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, 0, 0, wifi_popup_w, bh, lrpad / 2, toggle_text, 0);

  drw_setscheme(drw, scheme[SchemeNorm]);
  draw_hdivider(0, bh, wifi_popup_w);
  y = bh + 1;

  if (wifi_blocked) {
    drw_text(drw, 0, y, wifi_popup_w, bh, lrpad / 2, "Wi-Fi is off", 0);
    y += bh;
  } else if (wifi_password_mode) {
    char display[192];
    char masked[128];
    size_t i;

    for (i = 0; i < wifi_password_query_len && i + 1 < sizeof(masked); i++) {
      masked[i] = '*';
    }
    masked[i] = '\0';
    snprintf(display, sizeof(display), "Password for %s: %s", wifi_password_ssid, masked);
    drw_setscheme(drw, scheme[SchemeSel]);
    drw_text(drw, 0, y, wifi_popup_w, bh, lrpad / 2, display, 0);
    y += bh;
  } else {
    size_t shown = wifi_networks_len < MAX_WIFI_LIST_ROWS ? wifi_networks_len : (size_t)MAX_WIFI_LIST_ROWS;

    if (shown == 0) {
      const char *hint = wifi_scanning ? "Scanning for networks..." : "No networks found";
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, 0, y, wifi_popup_w, bh, lrpad / 2, hint, 0);
      y += bh;
    } else {
      size_t i;
      for (i = 0; i < shown; i++) {
        const WifiNetwork *w = &wifi_networks[i];
        char row_text[256];
        char status[32] = "";

        if (w->active) {
          snprintf(status, sizeof(status), " (connected)");
        } else if (wifi_connecting && !strcmp(wifi_connecting_ssid, w->ssid)) {
          snprintf(status, sizeof(status), " (connecting...)");
        }
        snprintf(row_text, sizeof(row_text), "%s%s%s%s", w->secured ? icon_lock : "",
                 w->secured ? " " : "", w->ssid, status);

        drw_setscheme(drw, scheme[(int)i == wifi_sel ? SchemeSel : SchemeNorm]);
        drw_text(drw, 0, y, wifi_popup_w, bh, lrpad / 2, row_text, 0);
        y += bh;
      }
    }
  }

  if (!wifi_blocked && !wifi_password_mode && wifi_status_msg[0] != '\0') {
    drw_setscheme(drw, scheme[SchemeNorm]);
    drw_text(drw, 0, y, wifi_popup_w, bh, lrpad / 2, wifi_status_msg, 0);
  }

  drw_map(drw, wifi_win, 0, 0, wifi_popup_w, total_h);
}

static void wifi_popup_open(Monitor *m) {
  XSetWindowAttributes wa = {};

  if (!m) {
    return;
  }
  if (wifi_win == None) {
    wa.override_redirect = True;
    wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
    wa.border_pixel = scheme[SchemeSel][ColBorder].pixel;
    wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    wifi_win = XCreateWindow(dpy, root, 0, 0, wifi_popup_w, bh, 1, DefaultDepth(dpy, screen),
                             CopyFromParent, DefaultVisual(dpy, screen),
                             CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &wa);
    XDefineCursor(dpy, wifi_win, cursor[CurNormal]->cursor);
  }
  if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) !=
      GrabSuccess) {
    return;
  }

  wifi_mon = m;
  wifi_visible = 1;
  wifi_sel = 0;
  wifi_password_mode = 0;
  wifi_status_msg[0] = '\0';
  widget_update_wifi(); /* fresh blocked/up state before deciding whether to scan */
  if (!wifi_blocked) {
    wifi_start_scan();
  }
  wifi_popup_position(m);
  wifi_popup_draw();
  XMapRaised(dpy, wifi_win);
  drawbars();
}

static void wifi_popup_close(void) {
  if (!wifi_visible) {
    return;
  }
  wifi_visible = 0;
  wifi_password_mode = 0;
  XUngrabKeyboard(dpy, CurrentTime);
  if (wifi_win != None) {
    XUnmapWindow(dpy, wifi_win);
  }
  drawbars();
}

static void wifi_popup_toggle(Monitor *m) {
  if (wifi_visible) {
    wifi_popup_close();
  } else {
    wifi_popup_open(m ? m : selmon);
  }
}

static int wifi_handle_keypress(XKeyEvent *ev) {
  char buf[64];
  KeySym keysym = NoSymbol;
  int len;

  if (!wifi_visible) {
    return 0;
  }

  len = XLookupString(ev, buf, sizeof(buf), &keysym, NULL);

  if (keysym == XK_Escape) {
    if (wifi_password_mode) {
      wifi_password_mode = 0;
      wifi_popup_position(wifi_mon);
      wifi_popup_draw();
    } else {
      wifi_popup_close();
    }
    return 1;
  }

  if (wifi_password_mode) {
    if (keysym == XK_Return || keysym == XK_KP_Enter) {
      wifi_start_connect(wifi_password_ssid, wifi_password_query);
      wifi_popup_draw();
      return 1;
    }
    if (keysym == XK_BackSpace) {
      if (wifi_password_query_len > 0) {
        wifi_password_query[--wifi_password_query_len] = '\0';
        wifi_popup_draw();
      }
      return 1;
    }
    {
      int i;
      int changed = 0;
      for (i = 0; i < len; i++) {
        if (!isprint((unsigned char)buf[i])) {
          continue;
        }
        if (wifi_password_query_len + 1 >= sizeof(wifi_password_query)) {
          break;
        }
        wifi_password_query[wifi_password_query_len++] = buf[i];
        wifi_password_query[wifi_password_query_len] = '\0';
        changed = 1;
      }
      if (changed) {
        wifi_popup_draw();
      }
    }
    return 1;
  }

  if (!wifi_blocked && wifi_networks_len > 0) {
    size_t shown = wifi_networks_len < MAX_WIFI_LIST_ROWS ? wifi_networks_len : (size_t)MAX_WIFI_LIST_ROWS;

    if (keysym == XK_Up) {
      wifi_sel = (int)((wifi_sel - 1 + (int)shown) % (int)shown);
      wifi_popup_draw();
      return 1;
    }
    if (keysym == XK_Down) {
      wifi_sel = (int)((wifi_sel + 1) % (int)shown);
      wifi_popup_draw();
      return 1;
    }
    if ((keysym == XK_Return || keysym == XK_KP_Enter) && !wifi_connecting) {
      if (wifi_sel >= 0 && (size_t)wifi_sel < shown && !wifi_networks[wifi_sel].active) {
        wifi_start_connect(wifi_networks[wifi_sel].ssid, NULL);
        wifi_popup_position(wifi_mon);
        wifi_popup_draw();
      }
      return 1;
    }
  }

  return 1; /* swallow everything else while the popup is up */
}

static int wifi_handle_button(XButtonPressedEvent *ev) {
  int row_y;

  if (!wifi_visible) {
    return 0;
  }
  if (ev->window != wifi_win) {
    wifi_popup_close();
    return 0;
  }
  if (ev->button != Button1) {
    return 1;
  }

  if (ev->y < bh) {
    /* Toggle row. Optimistic like the volume slider's fix: flip the local
     * flag immediately for a snappy UI, and let the real state (read back
     * via widget_update_wifi() on the normal periodic tick, or right here
     * when turning back on) catch up. */
    run_detached_shell(wifi_blocked ? "rfkill unblock wifi" : "rfkill block wifi");
    wifi_blocked = !wifi_blocked;
    wifi_status_msg[0] = '\0';
    wifi_password_mode = 0;
    if (!wifi_blocked) {
      wifi_networks_len = 0;
      wifi_start_scan();
    }
    wifi_popup_position(wifi_mon);
    wifi_popup_draw();
    drawbars();
    return 1;
  }

  if (wifi_blocked || wifi_password_mode) {
    return 1;
  }

  row_y = bh + 1;
  {
    size_t shown = wifi_networks_len < MAX_WIFI_LIST_ROWS ? wifi_networks_len : (size_t)MAX_WIFI_LIST_ROWS;
    size_t i;
    for (i = 0; i < shown; i++) {
      if (ev->y >= row_y && ev->y < row_y + bh) {
        wifi_sel = (int)i;
        if (!wifi_networks[i].active && !wifi_connecting) {
          wifi_start_connect(wifi_networks[i].ssid, NULL);
          wifi_popup_position(wifi_mon);
        }
        wifi_popup_draw();
        return 1;
      }
      row_y += bh;
    }
  }
  return 1;
}

/* }}} wifi popup */

/* {{{ desktop icons */

static void get_desktop_dir(char *buf, size_t size) {
  const char *home = getenv("HOME");
  if (!home || home[0] == '\0') {
    buf[0] = '\0';
    return;
  }
  snprintf(buf, size, "%s/Desktop", home);
  mkdir(buf, 0755);
}

static void get_trash_base_dir(char *buf, size_t size) {
  const char *xdg_data_home = getenv("XDG_DATA_HOME");
  const char *home;

  if (xdg_data_home && xdg_data_home[0] != '\0') {
    snprintf(buf, size, "%s/Trash", xdg_data_home);
    return;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(buf, size, "%s/.local/share/Trash", home);
    return;
  }
  buf[0] = '\0';
}

static void get_trash_files_dir(char *buf, size_t size) {
  char base[PATH_MAX];
  get_trash_base_dir(base, sizeof(base));
  if (base[0] == '\0') {
    buf[0] = '\0';
    return;
  }
  mkdir(base, 0700);
  snprintf(buf, size, "%s/files", base);
  mkdir(buf, 0700);
}

static void get_trash_info_dir(char *buf, size_t size) {
  char base[PATH_MAX];
  get_trash_base_dir(base, sizeof(base));
  if (base[0] == '\0') {
    buf[0] = '\0';
    return;
  }
  mkdir(base, 0700);
  snprintf(buf, size, "%s/info", base);
  mkdir(buf, 0700);
}

static void get_desktop_layout_path(char *buf, size_t size) {
  const char *xdg_data_home = getenv("XDG_DATA_HOME");
  const char *home;

  if (xdg_data_home && xdg_data_home[0] != '\0') {
    snprintf(buf, size, "%s/mwm/desktop_layout", xdg_data_home);
    return;
  }
  home = getenv("HOME");
  if (home && home[0] != '\0') {
    snprintf(buf, size, "%s/.local/share/mwm/desktop_layout", home);
    return;
  }
  buf[0] = '\0';
}

static void desktop_layout_save(void) {
  char path[PATH_MAX];
  char dir[PATH_MAX];
  char *slash;
  FILE *fp;
  size_t i;

  get_desktop_layout_path(path, sizeof(path));
  if (path[0] == '\0') {
    return;
  }
  snprintf(dir, sizeof(dir), "%s", path);
  slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    mkdir(dir, 0755);
  }
  fp = fopen(path, "w");
  if (!fp) {
    return;
  }
  for (i = 0; i < desktop_icons_len; i++) {
    DesktopIcon *icon = &desktop_icons[i];
    if (icon->is_trash) {
      fprintf(fp, "T\t%d\t%d\n", icon->x, icon->y);
    } else {
      fprintf(fp, "F\t%s\t%d\t%d\n", icon->name, icon->x, icon->y);
    }
  }
  fclose(fp);
}

static int desktop_ext_matches(const char *ext, const char *want_lower) {
  size_t i;
  for (i = 0; ext[i] && want_lower[i]; i++) {
    if (tolower((unsigned char)ext[i]) != want_lower[i]) {
      return 0;
    }
  }
  return ext[i] == '\0' && want_lower[i] == '\0';
}

static const char *desktop_icon_glyph(const DesktopIcon *icon) {
  static const struct {
    const char *ext;
    const char *glyph;
  } table[] = {
      {"png", icon_desktop_image},   {"jpg", icon_desktop_image},
      {"jpeg", icon_desktop_image},  {"gif", icon_desktop_image},
      {"bmp", icon_desktop_image},   {"svg", icon_desktop_image},
      {"webp", icon_desktop_image},  {"zip", icon_desktop_archive},
      {"tar", icon_desktop_archive}, {"gz", icon_desktop_archive},
      {"xz", icon_desktop_archive},  {"7z", icon_desktop_archive},
      {"bz2", icon_desktop_archive}, {"mp3", icon_desktop_audio},
      {"wav", icon_desktop_audio},   {"flac", icon_desktop_audio},
      {"ogg", icon_desktop_audio},   {"mp4", icon_desktop_video},
      {"mkv", icon_desktop_video},   {"webm", icon_desktop_video},
      {"avi", icon_desktop_video},   {"pdf", icon_desktop_pdf},
      {"txt", icon_desktop_text},    {"md", icon_desktop_text},
      {"log", icon_desktop_text},
  };
  const char *ext;
  size_t i;

  if (icon->is_trash) {
    char files_dir[PATH_MAX];
    DIR *dp;
    int has_entries = 0;
    get_trash_files_dir(files_dir, sizeof(files_dir));
    dp = files_dir[0] ? opendir(files_dir) : NULL;
    if (dp) {
      struct dirent *de;
      while ((de = readdir(dp)) != NULL) {
        if (de->d_name[0] == '.') {
          continue;
        }
        has_entries = 1;
        break;
      }
      closedir(dp);
    }
    return has_entries ? icon_trash_full : icon_trash_empty;
  }
  if (icon->is_dir) {
    return icon_desktop_folder;
  }
  ext = strrchr(icon->name, '.');
  if (!ext || !ext[1]) {
    return icon_desktop_file;
  }
  ext++;
  for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
    if (desktop_ext_matches(ext, table[i].ext)) {
      return table[i].glyph;
    }
  }
  return icon_desktop_file;
}

static void desktop_next_free_cell(const DesktopIcon *icons, size_t icons_len, int *out_x,
                                   int *out_y) {
  int top = bh + desktop_margin;
  int bottom = sh - bh - desktop_margin;
  int max_cols = (sw - 2 * desktop_margin) / desktop_cell_w;
  int col;

  if (max_cols < 1) {
    max_cols = 1;
  }
  for (col = 0; col < max_cols * 4; col++) {
    int row;
    for (row = 0;; row++) {
      int cx = desktop_margin + col * desktop_cell_w;
      int cy = top + row * desktop_cell_h;
      size_t i;
      int occupied = 0;
      if (cy + desktop_cell_h > bottom) {
        break;
      }
      for (i = 0; i < icons_len; i++) {
        int dx = icons[i].x - cx;
        int dy = icons[i].y - cy;
        if (dx < 0) {
          dx = -dx;
        }
        if (dy < 0) {
          dy = -dy;
        }
        if (dx < desktop_cell_w / 2 && dy < desktop_cell_h / 2) {
          occupied = 1;
          break;
        }
      }
      if (!occupied) {
        *out_x = cx;
        *out_y = cy;
        return;
      }
    }
  }
  *out_x = desktop_margin;
  *out_y = top;
}

static void desktop_rescan(void) {
  static DesktopIcon saved[MAX_DESKTOP_ICONS];
  static DesktopIcon new_icons[MAX_DESKTOP_ICONS];
  size_t saved_len = 0;
  size_t new_len = 0;
  char path[PATH_MAX];
  char desktop_dir[PATH_MAX];
  FILE *fp;
  char line[600];
  DIR *dp;
  struct dirent *de;
  int trash_x = -1, trash_y = -1;

  get_desktop_layout_path(path, sizeof(path));
  if (path[0] != '\0' && (fp = fopen(path, "r")) != NULL) {
    while (saved_len < MAX_DESKTOP_ICONS && fgets(line, sizeof(line), fp)) {
      line[strcspn(line, "\n")] = '\0';
      if (line[0] == 'T' && line[1] == '\t') {
        sscanf(line + 2, "%d\t%d", &trash_x, &trash_y);
      } else if (line[0] == 'F' && line[1] == '\t') {
        char *name_start = line + 2;
        char *tab1 = strchr(name_start, '\t');
        if (tab1) {
          int px = 0, py = 0;
          *tab1 = '\0';
          if (sscanf(tab1 + 1, "%d\t%d", &px, &py) == 2) {
            snprintf(saved[saved_len].name, sizeof(saved[saved_len].name), "%s",
                     name_start);
            saved[saved_len].x = px;
            saved[saved_len].y = py;
            saved_len++;
          }
        }
      }
    }
    fclose(fp);
  }

  new_icons[0].name[0] = '\0';
  new_icons[0].is_dir = 0;
  new_icons[0].is_trash = 1;
  new_icons[0].x = trash_x >= 0 ? trash_x : desktop_margin;
  new_icons[0].y = trash_y >= 0 ? trash_y : (bh + desktop_margin);
  new_icons[0].w = desktop_cell_w;
  new_icons[0].h = desktop_cell_h;
  new_len = 1;

  get_desktop_dir(desktop_dir, sizeof(desktop_dir));
  dp = desktop_dir[0] ? opendir(desktop_dir) : NULL;
  if (dp) {
    while (new_len < MAX_DESKTOP_ICONS && (de = readdir(dp)) != NULL) {
      char full[PATH_MAX];
      struct stat st;
      DesktopIcon *icon;
      int found = 0;
      size_t si;

      if (de->d_name[0] == '.') {
        continue;
      }
      snprintf(full, sizeof(full), "%s/%s", desktop_dir, de->d_name);
      if (stat(full, &st) != 0) {
        continue;
      }

      icon = &new_icons[new_len];
      snprintf(icon->name, sizeof(icon->name), "%s", de->d_name);
      icon->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
      icon->is_trash = 0;
      icon->w = desktop_cell_w;
      icon->h = desktop_cell_h;

      for (si = 0; si < saved_len; si++) {
        if (!strcmp(saved[si].name, icon->name)) {
          icon->x = saved[si].x;
          icon->y = saved[si].y;
          found = 1;
          break;
        }
      }
      if (!found) {
        int nx, ny;
        desktop_next_free_cell(new_icons, new_len, &nx, &ny);
        icon->x = nx;
        icon->y = ny;
      }
      new_len++;
    }
    closedir(dp);
  }

  memcpy(desktop_icons, new_icons, new_len * sizeof(DesktopIcon));
  desktop_icons_len = new_len;
  desktop_layout_save();
}

static int desktop_copy_regular_file(const char *src, const char *dst, mode_t mode) {
  int in_fd, out_fd;
  char buf[65536];
  ssize_t n;
  int ok = 1;

  in_fd = open(src, O_RDONLY);
  if (in_fd < 0) {
    return 0;
  }
  out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, (mode & 0777) ? (mode & 0777) : 0644);
  if (out_fd < 0) {
    close(in_fd);
    return 0;
  }
  while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
    ssize_t off = 0;
    while (off < n) {
      ssize_t w = write(out_fd, buf + off, (size_t)(n - off));
      if (w <= 0) {
        ok = 0;
        break;
      }
      off += w;
    }
    if (!ok) {
      break;
    }
  }
  if (n < 0) {
    ok = 0;
  }
  close(in_fd);
  close(out_fd);
  return ok;
}

static int desktop_copy_path_recursive(const char *src, const char *dst) {
  struct stat st;

  if (lstat(src, &st) != 0) {
    return 0;
  }
  if (S_ISLNK(st.st_mode)) {
    char target[PATH_MAX];
    ssize_t n = readlink(src, target, sizeof(target) - 1);
    if (n < 0) {
      return 0;
    }
    target[n] = '\0';
    return symlink(target, dst) == 0;
  }
  if (S_ISDIR(st.st_mode)) {
    DIR *dp = opendir(src);
    struct dirent *de;
    int ok = 1;
    if (!dp) {
      return 0;
    }
    if (mkdir(dst, st.st_mode & 0777) != 0 && errno != EEXIST) {
      closedir(dp);
      return 0;
    }
    while ((de = readdir(dp)) != NULL) {
      char child_src[PATH_MAX], child_dst[PATH_MAX];
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) {
        continue;
      }
      snprintf(child_src, sizeof(child_src), "%s/%s", src, de->d_name);
      snprintf(child_dst, sizeof(child_dst), "%s/%s", dst, de->d_name);
      if (!desktop_copy_path_recursive(child_src, child_dst)) {
        ok = 0;
      }
    }
    closedir(dp);
    return ok;
  }
  if (S_ISREG(st.st_mode)) {
    return desktop_copy_regular_file(src, dst, st.st_mode);
  }
  return 0; /* skip devices, sockets, fifos */
}

static void desktop_trash_icon(size_t index) {
  DesktopIcon *icon;
  char desktop_dir[PATH_MAX];
  char files_dir[PATH_MAX];
  char info_dir[PATH_MAX];
  char src[PATH_MAX];
  char dst_name[300];
  char dst[PATH_MAX];
  char info_path[PATH_MAX];
  char encoded_path[PATH_MAX * 3];
  struct stat st;
  FILE *fp;
  time_t now;
  struct tm tmv;
  char timebuf[32];
  int n;
  size_t ei;

  if (index >= desktop_icons_len) {
    return;
  }
  icon = &desktop_icons[index];
  if (icon->is_trash) {
    return;
  }

  get_desktop_dir(desktop_dir, sizeof(desktop_dir));
  get_trash_files_dir(files_dir, sizeof(files_dir));
  get_trash_info_dir(info_dir, sizeof(info_dir));
  if (files_dir[0] == '\0' || info_dir[0] == '\0') {
    return;
  }

  snprintf(src, sizeof(src), "%s/%s", desktop_dir, icon->name);
  snprintf(dst_name, sizeof(dst_name), "%s", icon->name);
  snprintf(dst, sizeof(dst), "%s/%s", files_dir, dst_name);
  for (n = 1; stat(dst, &st) == 0; n++) {
    char *dot = strrchr(icon->name, '.');
    if (dot && dot != icon->name) {
      snprintf(dst_name, sizeof(dst_name), "%.*s-%d%s", (int)(dot - icon->name),
               icon->name, n, dot);
    } else {
      snprintf(dst_name, sizeof(dst_name), "%s-%d", icon->name, n);
    }
    snprintf(dst, sizeof(dst), "%s/%s", files_dir, dst_name);
  }

  if (rename(src, dst) != 0) {
    return; /* same-filesystem move failed; leave the icon where it is */
  }

  {
    size_t oi = 0;
    size_t ci;
    for (ci = 0; src[ci] && oi + 4 < sizeof(encoded_path); ci++) {
      unsigned char c = (unsigned char)src[ci];
      if (isalnum(c) || strchr("/-_.~", c)) {
        encoded_path[oi++] = (char)c;
      } else {
        snprintf(encoded_path + oi, 4, "%%%02X", c);
        oi += 3;
      }
    }
    encoded_path[oi] = '\0';
  }

  now = time(NULL);
  localtime_r(&now, &tmv);
  strftime(timebuf, sizeof(timebuf), "%Y-%m-%dT%H:%M:%S", &tmv);

  snprintf(info_path, sizeof(info_path), "%s/%s.trashinfo", info_dir, dst_name);
  fp = fopen(info_path, "w");
  if (fp) {
    fprintf(fp, "[Trash Info]\nPath=%s\nDeletionDate=%s\n", encoded_path, timebuf);
    fclose(fp);
  }

  for (ei = index; ei + 1 < desktop_icons_len; ei++) {
    desktop_icons[ei] = desktop_icons[ei + 1];
  }
  desktop_icons_len--;
  desktop_layout_save();
}

static void desktop_draw(void) {
  size_t i;

  if (deskwin == None) {
    return;
  }

  drw_setscheme(drw, scheme[SchemeNorm]);
  drw_rect(drw, 0, 0, sw, sh, 1, 1);

  for (i = 0; i < desktop_icons_len; i++) {
    DesktopIcon *icon = &desktop_icons[i];
    const char *glyph = desktop_icon_glyph(icon);
    char lines[2][256];
    size_t nlines;
    int is_dragged = desktop_drag_active && i == desktop_drag_index;
    int x = is_dragged ? desktop_drag_x : icon->x;
    int y = is_dragged ? desktop_drag_y : icon->y;
    int highlight = 0;
    int li;

    icon->w = desktop_cell_w;
    icon->h = desktop_cell_h;

    if (desktop_drag_active && icon->is_trash && !is_dragged) {
      DesktopIcon *dragged = &desktop_icons[desktop_drag_index];
      if (!dragged->is_trash && desktop_drag_x < icon->x + desktop_cell_w &&
          desktop_drag_x + desktop_cell_w > icon->x &&
          desktop_drag_y < icon->y + desktop_cell_h &&
          desktop_drag_y + desktop_cell_h > icon->y) {
        highlight = 1;
      }
    }

    drw_setscheme(drw, scheme[highlight ? SchemeSel : SchemeNorm]);
    drw_text(drw, x, y, desktop_cell_w, desktop_glyph_h, lrpad / 2, glyph, 0);

    nlines = notif_wrap_body(icon->is_trash ? "Trash" : icon->name, desktop_cell_w - lrpad,
                             lines, 2);
    for (li = 0; li < (int)nlines; li++) {
      drw_text(drw, x, y + desktop_glyph_h + li * bh, desktop_cell_w, bh, lrpad / 2,
               lines[li], 0);
    }
  }

  drw_map(drw, deskwin, 0, 0, sw, sh);
}

static int desktop_hit_test(int x, int y) {
  size_t i;
  for (i = 0; i < desktop_icons_len; i++) {
    DesktopIcon *icon = &desktop_icons[i];
    if (icon->w <= 0 || icon->h <= 0) {
      continue;
    }
    if (x >= icon->x && x < icon->x + icon->w && y >= icon->y && y < icon->y + icon->h) {
      return (int)i;
    }
  }
  return -1;
}

static void desktop_open_icon(const DesktopIcon *icon) {
  Arg a = {0};
  const char *argv_term[4];
  const char *argv_open[3];
  char path[PATH_MAX];
  char trash_dir[PATH_MAX];
  char desktop_dir[PATH_MAX];

  if (icon->is_trash) {
    get_trash_files_dir(trash_dir, sizeof(trash_dir));
    argv_term[0] = terminal_cmd;
    argv_term[1] = "-d";
    argv_term[2] = trash_dir;
    argv_term[3] = NULL;
    a.v = argv_term;
    spawn(&a);
    return;
  }

  get_desktop_dir(desktop_dir, sizeof(desktop_dir));
  snprintf(path, sizeof(path), "%s/%s", desktop_dir, icon->name);
  argv_open[0] = "xdg-open";
  argv_open[1] = path;
  argv_open[2] = NULL;
  a.v = argv_open;
  spawn(&a);
}

static void desktop_dragmouse(size_t index) {
  DesktopIcon *icon;
  int ox, oy;
  int start_x, start_y;
  XEvent ev;
  Time lasttime = 0;

  if (index >= desktop_icons_len) {
    return;
  }
  icon = &desktop_icons[index];
  start_x = icon->x;
  start_y = icon->y;

  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync, None,
                   cursor[CurMove]->cursor, CurrentTime) != GrabSuccess) {
    return;
  }
  if (!getrootptr(&ox, &oy)) {
    XUngrabPointer(dpy, CurrentTime);
    return;
  }

  desktop_drag_active = 1;
  desktop_drag_index = index;
  desktop_drag_x = start_x;
  desktop_drag_y = start_y;
  desktop_draw();

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
      desktop_drag_x = start_x + (ev.xmotion.x - ox);
      desktop_drag_y = start_y + (ev.xmotion.y - oy);
      desktop_draw();
      break;
    }
  } while (ev.type != ButtonRelease);

  XUngrabPointer(dpy, CurrentTime);
  desktop_drag_active = 0;

  if (!icon->is_trash && desktop_icons_len > 0 && desktop_icons[0].is_trash) {
    DesktopIcon *trash = &desktop_icons[0];
    if (desktop_drag_x < trash->x + desktop_cell_w &&
        desktop_drag_x + desktop_cell_w > trash->x &&
        desktop_drag_y < trash->y + desktop_cell_h &&
        desktop_drag_y + desktop_cell_h > trash->y) {
      desktop_trash_icon(index);
      desktop_draw();
      return;
    }
  }

  icon->x = desktop_drag_x;
  icon->y = desktop_drag_y;
  desktop_layout_save();
  desktop_draw();
}

static int desktop_handle_button(XButtonPressedEvent *ev) {
  int idx;
  struct timeval now;
  long elapsed_ms;

  if (ev->window != deskwin) {
    return 0;
  }
  if (ev->button != Button1) {
    return 1;
  }

  idx = desktop_hit_test(ev->x, ev->y);
  if (idx < 0) {
    desktop_last_click_index = -1;
    focus(NULL);
    drawbars();
    return 1;
  }

  gettimeofday(&now, NULL);
  elapsed_ms = (now.tv_sec - desktop_last_click_time.tv_sec) * 1000 +
              (now.tv_usec - desktop_last_click_time.tv_usec) / 1000;
  if (desktop_last_click_index == idx && elapsed_ms >= 0 && elapsed_ms < 400) {
    desktop_last_click_index = -1;
    desktop_open_icon(&desktop_icons[idx]);
    return 1;
  }

  desktop_last_click_index = idx;
  desktop_last_click_time = now;
  desktop_dragmouse((size_t)idx);
  return 1;
}

static void desktop_setup(void) {
  XSetWindowAttributes wa = {};
  Atom version_atom = 5;
  char tmp[PATH_MAX];

  wa.override_redirect = True;
  wa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
  wa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask;
  deskwin = XCreateWindow(dpy, root, 0, 0, (unsigned int)sw, (unsigned int)sh, 0,
                          DefaultDepth(dpy, screen), CopyFromParent,
                          DefaultVisual(dpy, screen),
                          CWOverrideRedirect | CWBackPixel | CWEventMask, &wa);
  XDefineCursor(dpy, deskwin, cursor[CurNormal]->cursor);
  XChangeProperty(dpy, deskwin, xdndatom[XdndAwareAtom], XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&version_atom, 1);

  get_desktop_dir(tmp, sizeof(tmp));
  get_trash_files_dir(tmp, sizeof(tmp));
  get_trash_info_dir(tmp, sizeof(tmp));

  desktop_rescan();

  XMapWindow(dpy, deskwin);
  XLowerWindow(dpy, deskwin);
  desktop_draw();
}

/* {{{ xdnd (drag files in from another application onto the desktop) */

static void desktop_xdnd_enter(XClientMessageEvent *cme) {
  int more_than_3 = (int)(cme->data.l[1] & 1);

  xdnd_source = (Window)cme->data.l[0];
  xdnd_version = (int)((cme->data.l[1] >> 24) & 0xFF);
  xdnd_desired_type = None;

  if (more_than_3) {
    Atom actual;
    int format;
    unsigned long nitems, after;
    unsigned char *data = NULL;
    if (XGetWindowProperty(dpy, xdnd_source, xdndatom[XdndTypeListAtom], 0, 256, False,
                           XA_ATOM, &actual, &format, &nitems, &after, &data) == Success &&
        data) {
      Atom *atoms = (Atom *)data;
      unsigned long ai;
      for (ai = 0; ai < nitems; ai++) {
        if (atoms[ai] == xdndatom[UriListAtom]) {
          xdnd_desired_type = atoms[ai];
          break;
        }
      }
      XFree(data);
    }
  } else {
    Atom candidates[3];
    int ti;
    candidates[0] = (Atom)cme->data.l[2];
    candidates[1] = (Atom)cme->data.l[3];
    candidates[2] = (Atom)cme->data.l[4];
    for (ti = 0; ti < 3; ti++) {
      if (candidates[ti] == xdndatom[UriListAtom]) {
        xdnd_desired_type = candidates[ti];
        break;
      }
    }
  }
}

static void desktop_xdnd_position(XClientMessageEvent *cme) {
  Window source = (Window)cme->data.l[0];
  XEvent reply;

  xdnd_last_x = (int)(((unsigned long)cme->data.l[2]) >> 16);
  xdnd_last_y = (int)(((unsigned long)cme->data.l[2]) & 0xFFFF);

  memset(&reply, 0, sizeof(reply));
  reply.xclient.type = ClientMessage;
  reply.xclient.display = dpy;
  reply.xclient.window = source;
  reply.xclient.message_type = xdndatom[XdndStatusAtom];
  reply.xclient.format = 32;
  reply.xclient.data.l[0] = (long)deskwin;
  reply.xclient.data.l[1] = xdnd_desired_type != None ? 1 : 0;
  reply.xclient.data.l[2] = 0;
  reply.xclient.data.l[3] = 0;
  reply.xclient.data.l[4] =
      xdnd_desired_type != None ? (long)xdndatom[XdndActionCopyAtom] : None;
  XSendEvent(dpy, source, False, NoEventMask, &reply);
}

static void desktop_xdnd_leave(XClientMessageEvent *cme) {
  (void)cme;
  xdnd_source = None;
  xdnd_desired_type = None;
  xdnd_version = 0;
}

static void desktop_xdnd_drop(XClientMessageEvent *cme) {
  Window source = (Window)cme->data.l[0];
  Time timestamp = xdnd_version >= 1 ? (Time)cme->data.l[2] : CurrentTime;

  if (xdnd_desired_type == None) {
    XEvent reply;
    memset(&reply, 0, sizeof(reply));
    reply.xclient.type = ClientMessage;
    reply.xclient.display = dpy;
    reply.xclient.window = source;
    reply.xclient.message_type = xdndatom[XdndFinishedAtom];
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = (long)deskwin;
    reply.xclient.data.l[1] = 0;
    reply.xclient.data.l[2] = None;
    XSendEvent(dpy, source, False, NoEventMask, &reply);
    return;
  }

  xdnd_pending_source = source;
  XConvertSelection(dpy, xdndatom[XdndSelectionAtom], xdnd_desired_type,
                    xdndatom[XdndSelectionAtom], deskwin, timestamp);
}

static void desktop_url_decode(const char *in, char *out, size_t outsz) {
  size_t oi = 0;
  size_t i;
  for (i = 0; in[i] && oi + 1 < outsz; i++) {
    if (in[i] == '%' && isxdigit((unsigned char)in[i + 1]) &&
        isxdigit((unsigned char)in[i + 2])) {
      char hex[3] = {in[i + 1], in[i + 2], '\0'};
      out[oi++] = (char)strtol(hex, NULL, 16);
      i += 2;
    } else {
      out[oi++] = in[i];
    }
  }
  out[oi] = '\0';
}

static int desktop_xdnd_import_uri_list(const char *data, int drop_x, int drop_y) {
  char desktop_dir[PATH_MAX];
  char *copy;
  char *line;
  char *saveptr = NULL;
  int imported = 0;
  int stagger = 0;

  get_desktop_dir(desktop_dir, sizeof(desktop_dir));
  if (desktop_dir[0] == '\0') {
    return 0;
  }

  copy = xstrdup_local(data);
  for (line = strtok_r(copy, "\r\n", &saveptr); line;
       line = strtok_r(NULL, "\r\n", &saveptr)) {
    char decoded[PATH_MAX];
    const char *path_start;
    const char *base;
    char dst_name[300];
    char dst[PATH_MAX];
    struct stat st;
    int n;
    DesktopIcon *icon;

    if (line[0] == '#' || line[0] == '\0') {
      continue;
    }
    if (strncmp(line, "file://", 7) != 0) {
      continue;
    }

    path_start = line + 7;
    if (*path_start != '/') {
      path_start = strchr(path_start, '/'); /* skip an embedded hostname */
      if (!path_start) {
        continue;
      }
    }
    desktop_url_decode(path_start, decoded, sizeof(decoded));
    if (decoded[0] == '\0') {
      continue;
    }

    base = strrchr(decoded, '/');
    base = base ? base + 1 : decoded;
    if (base[0] == '\0') {
      continue;
    }

    snprintf(dst_name, sizeof(dst_name), "%s", base);
    snprintf(dst, sizeof(dst), "%s/%s", desktop_dir, dst_name);
    for (n = 1; strcmp(decoded, dst) != 0 && stat(dst, &st) == 0; n++) {
      const char *dot = strrchr(base, '.');
      if (dot && dot != base) {
        snprintf(dst_name, sizeof(dst_name), "%.*s-%d%s", (int)(dot - base), base, n, dot);
      } else {
        snprintf(dst_name, sizeof(dst_name), "%s-%d", base, n);
      }
      snprintf(dst, sizeof(dst), "%s/%s", desktop_dir, dst_name);
    }

    if (strcmp(decoded, dst) != 0 && !desktop_copy_path_recursive(decoded, dst)) {
      continue;
    }

    if (desktop_icons_len < MAX_DESKTOP_ICONS) {
      struct stat dst_st;
      icon = &desktop_icons[desktop_icons_len];
      snprintf(icon->name, sizeof(icon->name), "%s", dst_name);
      icon->is_dir = (stat(dst, &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) ? 1 : 0;
      icon->is_trash = 0;
      icon->x = drop_x + stagger;
      icon->y = drop_y + stagger;
      if (icon->x < desktop_margin) {
        icon->x = desktop_margin;
      }
      if (icon->y < bh + desktop_margin) {
        icon->y = bh + desktop_margin;
      }
      if (icon->x > sw - desktop_cell_w - desktop_margin) {
        icon->x = sw - desktop_cell_w - desktop_margin;
      }
      if (icon->y > sh - bh - desktop_cell_h - desktop_margin) {
        icon->y = sh - bh - desktop_cell_h - desktop_margin;
      }
      icon->w = desktop_cell_w;
      icon->h = desktop_cell_h;
      desktop_icons_len++;
      stagger += 20;
      imported++;
    }
  }
  free(copy);

  if (imported > 0) {
    desktop_layout_save();
  }
  return imported;
}

static void desktop_xdnd_selection_notify(XSelectionEvent *sev) {
  Atom actual;
  int format;
  unsigned long nitems, after;
  unsigned char *data = NULL;
  int imported = 0;
  XEvent reply;

  if (sev->property != None &&
      XGetWindowProperty(dpy, deskwin, sev->property, 0, 262144, True, AnyPropertyType,
                         &actual, &format, &nitems, &after, &data) == Success &&
      data) {
    imported = desktop_xdnd_import_uri_list((const char *)data, xdnd_last_x, xdnd_last_y);
    XFree(data);
  }

  memset(&reply, 0, sizeof(reply));
  reply.xclient.type = ClientMessage;
  reply.xclient.display = dpy;
  reply.xclient.window = xdnd_pending_source;
  reply.xclient.message_type = xdndatom[XdndFinishedAtom];
  reply.xclient.format = 32;
  reply.xclient.data.l[0] = (long)deskwin;
  reply.xclient.data.l[1] = imported > 0 ? 1 : 0;
  reply.xclient.data.l[2] = imported > 0 ? (long)xdndatom[XdndActionCopyAtom] : None;
  if (xdnd_pending_source != None) {
    XSendEvent(dpy, xdnd_pending_source, False, NoEventMask, &reply);
  }
  xdnd_pending_source = None;

  desktop_draw();
  drawbars();
}

static void selectionnotify(XEvent *e) {
  XSelectionEvent *sev = &e->xselection;
  if (sev->requestor == deskwin && sev->selection == xdndatom[XdndSelectionAtom]) {
    desktop_xdnd_selection_notify(sev);
  }
}

/* }}} xdnd */

/* }}} desktop icons */

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
  lua_pushcfunction(L, lua_mwm_current_project);
  lua_setfield(L, -2, "current_project");
  lua_pushcfunction(L, lua_mwm_list_projects);
  lua_setfield(L, -2, "list_projects");
  lua_pushcfunction(L, lua_mwm_switch_project);
  lua_setfield(L, -2, "switch_project");
  lua_pushcfunction(L, lua_mwm_agent_command);
  lua_setfield(L, -2, "agent_command");
  lua_pushcfunction(L, lua_mwm_keybind);
  lua_setfield(L, -2, "keybind");
  lua_pushcfunction(L, lua_mwm_rule);
  lua_setfield(L, -2, "rule");
  lua_pushcfunction(L, lua_mwm_set_terminal);
  lua_setfield(L, -2, "set_terminal");
  lua_pushcfunction(L, lua_mwm_set_mfact);
  lua_setfield(L, -2, "set_mfact");
  lua_pushcfunction(L, lua_mwm_set_nmaster);
  lua_setfield(L, -2, "set_nmaster");
  lua_pushcfunction(L, lua_mwm_set_snap);
  lua_setfield(L, -2, "set_snap");
  lua_pushcfunction(L, lua_mwm_set_border_width);
  lua_setfield(L, -2, "set_border_width");
  lua_pushcfunction(L, lua_mwm_set_gaps);
  lua_setfield(L, -2, "set_gaps");
  lua_pushcfunction(L, lua_mwm_scratchpad_toggle);
  lua_setfield(L, -2, "scratchpad_toggle");
  lua_pushcfunction(L, lua_mwm_scratchpad_set);
  lua_setfield(L, -2, "scratchpad_set");
  lua_pushcfunction(L, lua_mwm_zoom);
  lua_setfield(L, -2, "zoom");
  lua_pushcfunction(L, lua_mwm_toggle_floating);
  lua_setfield(L, -2, "toggle_floating");
  lua_pushcfunction(L, lua_mwm_kill_client);
  lua_setfield(L, -2, "kill_client");
  lua_pushcfunction(L, lua_mwm_toggle_bar);
  lua_setfield(L, -2, "toggle_bar");
  lua_pushcfunction(L, lua_mwm_mousebind);
  lua_setfield(L, -2, "mousebind");
  lua_pushcfunction(L, lua_mwm_set_layout);
  lua_setfield(L, -2, "set_layout");
  lua_pushcfunction(L, lua_mwm_layout);
  lua_setfield(L, -2, "layout");
  lua_pushcfunction(L, lua_mwm_cycle_layout);
  lua_setfield(L, -2, "cycle_layout");
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
  Layout foo = {"", NULL, -1};
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
  for (i = 0; i < launcher_apps_len; i++) {
    free(launcher_apps[i]);
  }
  free(launcher_apps);
  launcher_apps = NULL;
  launcher_apps_len = 0;
  launcher_apps_cap = 0;
  if (project_win != None) {
    XDestroyWindow(dpy, project_win);
    project_win = None;
  }
  if (keybind_help_win != None) {
    XDestroyWindow(dpy, keybind_help_win);
    keybind_help_win = None;
  }
  if (wifi_win != None) {
    XDestroyWindow(dpy, wifi_win);
    wifi_win = None;
  }
  wifi_job_reset();
  for (i = 0; i < projects_len; i++) {
    free(projects[i]);
  }
  free(projects);
  projects = NULL;
  projects_len = 0;
  projects_cap = 0;
  if (deskwin != None) {
    XDestroyWindow(dpy, deskwin);
    deskwin = None;
  }
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

  if (cme->window == deskwin) {
    if (cme->message_type == xdndatom[XdndEnterAtom]) {
      desktop_xdnd_enter(cme);
    } else if (cme->message_type == xdndatom[XdndPositionAtom]) {
      desktop_xdnd_position(cme);
    } else if (cme->message_type == xdndatom[XdndLeaveAtom]) {
      desktop_xdnd_leave(cme);
    } else if (cme->message_type == xdndatom[XdndDropAtom]) {
      desktop_xdnd_drop(cme);
    }
    return;
  }

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

// {{{ bar dividers
/* Subtle 1px dividers in the theme's border color -- independent of
 * whichever SchemeNorm/SchemeSel is currently set for surrounding text/fill,
 * so a divider's color doesn't flicker depending on a neighboring widget's
 * highlight state. Used to segment the widget bar's dense row of icons
 * (twenty-odd of them, by now) and to give both bars a defined edge against
 * the desktop instead of just ending in flat color. */
void draw_vdivider(int x, int y, int h) {
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBorder].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, 1, h);
}

void draw_hdivider(int x, int y, int w) {
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBorder].pixel);
  XFillRectangle(dpy, drw->drawable, drw->gc, x, y, w, 1);
}
// }}} bar dividers

// {{{ void drawbar(Monitor *m)
void drawbar(Monitor *m) {
  int x, w, tw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  char prompt_display[sizeof(command_prompt) + 16];
  char project_label[288];
  char clock_text[64];
  int clockw = 0;
  int project_labelw = 0;
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
    widget_format_clock(clock_text, sizeof(clock_text));
    clockw = (int)TEXTW(clock_text);
    clock_reserved_w = clockw;
    tw = TEXTW(stext) - lrpad + 2; /* 2px right padding */
    drw_text(drw, m->ww - tw - trayw - clockw, 0, tw, bh, 0, stext, 0);
    drw_text(drw, m->ww - clockw, 0, clockw, bh, lrpad / 2, clock_text, 0);
  }
  /* Not reset on the else path -- clock_reserved_w only means anything for
   * selmon's bar (where the clock/tray actually live), so a non-selmon
   * monitor's redraw should just leave the last real value alone rather
   * than transiently zeroing it out from under a same-tick updatesystray(). */

  /* Active project in the far left corner -- independent of whichever
   * workspace/tag is currently viewed (see active_project_path); there's
   * always one, defaulting to "default" (tied to $HOME) at startup. */
  {
    char base[256];
    project_basename(active_project_path, base, sizeof(base));
    snprintf(project_label, sizeof(project_label), "%s %s", icon_projects, base);
  }
  project_labelw = (int)TEXTW(project_label);
  drw_setscheme(drw, scheme[SchemeSel]);
  drw_text(drw, 0, 0, project_labelw, bh, lrpad / 2, project_label, 0);
  draw_vdivider(project_labelw, bh / 4, bh - bh / 2);
  for (c = m->clients; c; c = c->next) {
    occ |= c->tags;
    if (c->isurgent) {
      urg |= c->tags;
    }
  }
  x = project_labelw;
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
  if (workspace_count_visible() > 0) {
    draw_vdivider(x, bh / 4, bh - bh / 2);
  }
  w = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);
  if ((w = m->ww - tw - trayw - clockw - x) > bh) {
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
  /* Both edges, not just whichever touches the desktop -- m->topbar can
   * flip which end of the screen this bar (and the widget bar) sits on. */
  draw_hdivider(0, 0, m->ww);
  draw_hdivider(0, bh - 1, m->ww);
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

  launcher_btn_x = 0;
  launcher_btn_w = (int)TEXTW(icon_launcher);
  drw_setscheme(drw, scheme[project_visible ? SchemeSel : SchemeNorm]);
  drw_text(drw, launcher_btn_x, 0, launcher_btn_w, bh, lrpad / 2, icon_launcher, 0);
  draw_vdivider(launcher_btn_w, bh / 4, bh - bh / 2);

  widget_format_battery(text[WidgetBattery], sizeof(text[WidgetBattery]));
  widget_format_backlight(text[WidgetBacklight], sizeof(text[WidgetBacklight]));
  widget_format_volume(text[WidgetVolume], sizeof(text[WidgetVolume]));
  widget_format_mic(text[WidgetMic], sizeof(text[WidgetMic]));
  widget_format_media(text[WidgetMedia], sizeof(text[WidgetMedia]));
  widget_format_theme(text[WidgetTheme], sizeof(text[WidgetTheme]));
  widget_format_notifications(text[WidgetNotif], sizeof(text[WidgetNotif]));
  widget_format_todos(text[WidgetTodo], sizeof(text[WidgetTodo]));
  widget_format_agents(text[WidgetAgents], sizeof(text[WidgetAgents]));
  widget_format_git(text[WidgetGit], sizeof(text[WidgetGit]));
  widget_format_projects(text[WidgetProjects], sizeof(text[WidgetProjects]));
  widget_format_load(text[WidgetLoad], sizeof(text[WidgetLoad]));
  widget_format_cpu(text[WidgetCpu], sizeof(text[WidgetCpu]));
  widget_format_mem(text[WidgetMem], sizeof(text[WidgetMem]));
  widget_format_disk(text[WidgetDisk], sizeof(text[WidgetDisk]));
  widget_format_net(text[WidgetNet], sizeof(text[WidgetNet]));
  widget_format_wifi(text[WidgetWifi], sizeof(text[WidgetWifi]));
  widget_format_bluetooth(text[WidgetBluetooth], sizeof(text[WidgetBluetooth]));
  widget_format_kblayout(text[WidgetKbLayout], sizeof(text[WidgetKbLayout]));

  x = m->ww;
  {
    int first = 1;

    for (i = 0; i < n; i++) {
      enum WidgetId id = widget_draw_order[i];
      int w;
      int highlighted;

      if (text[id][0] == '\0') {
        /* blank widget (e.g. git/mic with nothing to show) -- no width,
         * no gap, so it doesn't leave an empty hole in the bar */
        widget_layout[id].x = x;
        widget_layout[id].w = 0;
        continue;
      }

      w = (int)TEXTW(text[id]);
      highlighted = id == WidgetBacklight || id == WidgetVolume ||
                    id == WidgetTheme ||
                    (id == WidgetMic && mic_muted) ||
                    (id == WidgetMedia && strcmp(media_status, "Playing") == 0) ||
                    (id == WidgetNotif && notification_unread > 0) ||
                    (id == WidgetTodo && todo_incomplete > 0) ||
                    (id == WidgetAgents && agent_needs_input > 0) ||
                    (id == WidgetGit && git_dirty) ||
                    (id == WidgetProjects && project_visible) ||
                    (id == WidgetLoad && load_avg1 >= (double)widget_ncpu()) ||
                    (id == WidgetWifi && wifi_blocked) ||
                    (id == WidgetBluetooth && bt_blocked);

      if (!first) {
        draw_vdivider(x - lrpad / 2, bh / 4, bh - bh / 2);
        x -= lrpad;
      }
      first = 0;
      x -= w;
      widget_layout[id].x = x;
      widget_layout[id].w = w;

      drw_setscheme(drw, scheme[highlighted ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, text[id], 0);
    }

    for (i = 0; i < lua_widgets_len; i++) {
      LuaWidget *widget = &lua_widgets[i];
      int w;

      if (widget->text[0] == '\0') {
        widget->x = x;
        widget->w = 0;
        continue;
      }

      w = (int)TEXTW(widget->text);
      if (!first) {
        draw_vdivider(x - lrpad / 2, bh / 4, bh - bh / 2);
        x -= lrpad;
      }
      first = 0;
      x -= w;
      widget->x = x;
      widget->w = w;

      drw_setscheme(drw, scheme[widget->highlight ? SchemeSel : SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, widget->text, 0);
    }
  }

  draw_hdivider(0, 0, m->ww);
  draw_hdivider(0, bh - 1, m->ww);
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
  if (project_visible && ev->window == project_win) {
    project_draw();
    return;
  }
  if (keybind_help_visible && ev->window == keybind_help_win) {
    keybind_help_draw();
    return;
  }
  if (wifi_visible && ev->window == wifi_win) {
    wifi_popup_draw();
    return;
  }
  if (ev->window == deskwin) {
    desktop_draw();
    return;
  }
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
    for (i = 0; i < lua_buttons_len; i++) {
      if (lua_buttons[i].click == ClkClientWin) {
        for (j = 0; j < LENGTH(modifiers); j++) {
          XGrabButton(dpy, lua_buttons[i].button, lua_buttons[i].mod | modifiers[j],
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
      /* skip modifier codes, we do that ourselves */
      for (i = 0; i < LENGTH(keys); i++) {
        if (keys[i].keysym == syms[(k - start) * skip]) {
          for (j = 0; j < LENGTH(modifiers); j++) {
            XGrabKey(dpy, k, keys[i].mod | modifiers[j], root, True,
                     GrabModeAsync, GrabModeAsync);
          }
        }
      }
      for (i = 0; i < lua_keys_len; i++) {
        if (lua_keys[i].keysym == syms[(k - start) * skip]) {
          for (j = 0; j < LENGTH(modifiers); j++) {
            XGrabKey(dpy, k, lua_keys[i].mod | modifiers[j], root, True,
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
  if (keybind_help_visible && keybind_help_handle_keypress(ev)) {
    return;
  }
  if (wifi_visible && wifi_handle_keypress(ev)) {
    return;
  }
  if (project_visible && project_handle_keypress(ev)) {
    return;
  }
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
  for (i = 0; i < lua_keys_len; i++) {
    if (keysym == lua_keys[i].keysym &&
        CLEANMASK(lua_keys[i].mod) == CLEANMASK(ev->state)) {
      lua_key_invoke(i);
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
    resize_cell(c, m->wx, m->wy, m->ww, m->wh, 0);
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

// {{{ void resize_cell(Client *c, int x, int y, int w, int h, int interact)
/* What every layout (native tile()/monocle(), and the Lua arrange()
 * dispatch) should call instead of resize() directly: takes the outer cell
 * boundary the layout computed -- border-inclusive, gap-inclusive -- and
 * insets it by the configured gap before subtracting the border and
 * handing off to resize(). Centralizing this here is what makes gaps apply
 * uniformly to every layout, including custom Lua ones, without each one
 * needing to know gaps exist.
 *
 * The accumulator math in tile() deliberately advances by the *intended*
 * cell size rather than the client's actual post-resize HEIGHT()/WIDTH():
 * gap-shrinking a client here would otherwise make consecutive cells in
 * the same column/row eat each other's gap, since HEIGHT(c) would already
 * reflect the smaller, gapped size. */
void resize_cell(Client *c, int x, int y, int w, int h, int interact) {
  int inset = gappx;
  int cw, ch;

  if (2 * inset < w) {
    x += inset;
    w -= 2 * inset;
  }
  if (2 * inset < h) {
    y += inset;
    h -= 2 * inset;
  }

  cw = w - 2 * c->bw;
  ch = h - 2 * c->bw;
  if (cw < 1) {
    cw = 1;
  }
  if (ch < 1) {
    ch = 1;
  }
  resize(c, x, y, cw, ch, interact);
}
// }}} void resize_cell(Client *c, int x, int y, int w, int h, int interact)

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
  MonLayoutSnapshot *layout_snap;
  size_t layout_snap_count;

  /* Must happen before reset_lua_layouts()/load_config_from_lua() reuse
   * lua_layout_entries[] out from under any monitor still pointing into it. */
  layout_snap = snapshot_lua_layouts(&layout_snap_count);

  if (command_lua) {
    lua_close(command_lua);
    command_lua = NULL;
  }
  reset_lua_widgets();
  reset_lua_keys();
  reset_lua_rules();
  reset_lua_buttons();
  reset_lua_layouts();

  set_default_bar_theme();
  load_config_from_lua();
  /* A theme picked from the picker takes precedence over both the
   * hardcoded default just restored above and whatever Lua just set --
   * same "last explicit choice wins" rule setup() follows at startup. */
  if (active_color_theme >= 0) {
    bar_theme_dark = color_themes[active_color_theme].colors;
    bar_theme_light = color_themes[active_color_theme].colors;
  }
  setcolorscheme();
  /* Unconditional even though mwm.keybind()/mwm.mousebind() already
   * re-grab on each call: a reload that registers fewer (or zero) Lua
   * bindings than before still needs the stale ones actually forgotten by
   * X, not just dropped from mwm's own arrays. */
  grabkeys();
  regrab_all_client_buttons();
  restore_lua_layouts(layout_snap, layout_snap_count);
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
      desktop_rescan();
      desktop_draw();
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
    if (wifi_job_fd >= 0) {
      FD_SET(wifi_job_fd, &readfds);
      if (wifi_job_fd > maxfd) {
        maxfd = wifi_job_fd;
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
    wifi_job_poll();
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

// {{{ scratchpad
/* Is `target` still a live, managed client? scratchpad_client is only ever
 * cleared by unmanage() -- this is belt-and-suspenders against any path
 * that might miss that, since a dangling Client* used here would be a
 * use-after-free. */
int client_is_managed(Client *target) {
  Monitor *m;
  Client *c;

  if (!target) {
    return 0;
  }
  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c == target) {
        return 1;
      }
    }
  }
  return 0;
}

/* Tags c onto the (never-viewed-by-default) scratchpad workspace, hiding
 * it without killing it. */
void scratchpad_hide(Client *c) {
  size_t idx = workspace_ensure(scratchpad_workspace);
  if (idx == SIZE_MAX) {
    return;
  }
  c->tags = workspace_mask_from_index(idx);
  client_save_workspace_tags(c);
  focus(NULL);
  arrange(c->mon);
}

/* Brings c onto m's current workspace and focuses it. Deliberately doesn't
 * reuse sendmon() -- that no-ops when c->mon already equals m, which is
 * exactly the common single-monitor case here (the client needs its tags
 * updated even when its monitor doesn't change). */
void scratchpad_show(Client *c, Monitor *m) {
  if (c->mon != m) {
    detach(c);
    detachstack(c);
    c->mon = m;
    attach(c);
    attachstack(c);
  }
  c->tags = m->tagset[m->seltags];
  client_save_workspace_tags(c);
  focus(c);
  arrange(m);
}

/* Mod4+Shift+minus: designates the focused client as *the* scratchpad
 * window (replacing whatever was there before) and hides it immediately.
 * Floats it and gives it a reasonable default size/position the first
 * time, so it reappears as a centered overlay rather than joining
 * whatever tiling layout is active. */
void scratchpad_set(const Arg *arg) {
  Client *c;
  int w, h;
  (void)arg;

  if (!selmon || !selmon->sel) {
    return;
  }
  c = selmon->sel;
  scratchpad_client = c;
  c->isfloating = 1;
  w = selmon->ww * 6 / 10;
  h = selmon->wh * 6 / 10;
  resizeclient(c, selmon->wx + (selmon->ww - w) / 2, selmon->wy + (selmon->wh - h) / 2, w, h);
  scratchpad_hide(c);
  drawbars();
}

/* Mod4+minus: shows the scratchpad client if it's hidden or on another
 * workspace, hides it if it's already the one visible here. A no-op if
 * nothing's been designated yet, or the designated client has since been
 * closed. */
void scratchpad_toggle(const Arg *arg) {
  (void)arg;
  if (!selmon) {
    return;
  }
  if (!client_is_managed(scratchpad_client)) {
    scratchpad_client = NULL;
    return;
  }
  if (scratchpad_client->mon == selmon &&
      scratchpad_client->tags == selmon->tagset[selmon->seltags]) {
    scratchpad_hide(scratchpad_client);
  } else {
    scratchpad_show(scratchpad_client, selmon);
  }
  drawbars();
}
// }}} scratchpad

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
  /* A previously picked named theme wins over both of the above -- it's
   * the user's last explicit choice via the picker, not a fallback. */
  if (active_color_theme >= 0) {
    bar_theme_dark = color_themes[active_color_theme].colors;
    bar_theme_light = color_themes[active_color_theme].colors;
  }
  notif_dbus_init();
  todo_seed_fake();
  projects_load();
  projects_loaded = 1;
  project_init_default();
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
  xdndatom[XdndAwareAtom] = XInternAtom(dpy, "XdndAware", False);
  xdndatom[XdndEnterAtom] = XInternAtom(dpy, "XdndEnter", False);
  xdndatom[XdndPositionAtom] = XInternAtom(dpy, "XdndPosition", False);
  xdndatom[XdndStatusAtom] = XInternAtom(dpy, "XdndStatus", False);
  xdndatom[XdndLeaveAtom] = XInternAtom(dpy, "XdndLeave", False);
  xdndatom[XdndDropAtom] = XInternAtom(dpy, "XdndDrop", False);
  xdndatom[XdndFinishedAtom] = XInternAtom(dpy, "XdndFinished", False);
  xdndatom[XdndSelectionAtom] = XInternAtom(dpy, "XdndSelection", False);
  xdndatom[XdndTypeListAtom] = XInternAtom(dpy, "XdndTypeList", False);
  xdndatom[XdndActionCopyAtom] = XInternAtom(dpy, "XdndActionCopy", False);
  xdndatom[UriListAtom] = XInternAtom(dpy, "text/uri-list", False);
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
  netatom[NetWMPid] = XInternAtom(dpy, "_NET_WM_PID", False);
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
  desktop_setup();
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
      resize_cell(c, m->wx, m->wy + my, mw, h, 0);
      /* Advance by the intended cell height h, not the client's actual
       * post-resize HEIGHT() -- see resize_cell()'s comment. */
      if (my + h < m->wh) {
        my += h;
      }
      mfacts -= c->cfact;
    } else {
      fact = sfacts > 0.0f ? c->cfact / sfacts : 1.0f;
      h = (unsigned int)((m->wh - ty) * fact);
      resize_cell(c, m->wx + mw, m->wy + ty, m->ww - mw, h, 0);
      if (ty + h < m->wh) {
        ty += h;
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
  if (c == scratchpad_client) {
    scratchpad_client = NULL;
  }
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

  /* Shifted left by clock_reserved_w so the tray never overlaps the clock
   * drawbar() just drew past it, at the true right edge of the bar. */
  wc.x = selmon->mx + selmon->ww - (int)width - clock_reserved_w;
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
