/* objects.cpp - 10 May 11
   Hosts3D - 3D Real-Time Network Monitor
   Copyright (c) 2006-2010  Del Castle
   Continuation note: This repository is an independent community-maintained
   continuation of the original Hosts3D upstream.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details. */

#include <GL/glfw.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>  //atoi()
#include <string.h>  //strcat(), strcpy()
#include <sys/stat.h>  //mkdir() (linux), struct stat, stat()
#ifndef __MINGW32__
#include <arpa/inet.h>  //inet_addr()
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "objects.h"
#include "llist.h"

#ifdef __MINGW32__
const char HSDDATA[] = "hsd-data/";  //hosts3d data directory (win)
#else
const char HSDDATA[] = ".hosts3d/";  //hosts3d data directory (linux)
#endif

char fullpath[256];
MyLL ntpsLL;  //dynamic data struct for net positions file entries

static char netpsColorToken(const char *tok)
{
  if (!tok || !*tok) return '\0';
  if (!strcmp(tok, "default")) return 'd';
  if (!strcmp(tok, "orange")) return 'o';
  if (!strcmp(tok, "yellow")) return 'y';
  if (!strcmp(tok, "fluro")) return 'f';
  if (!strcmp(tok, "green")) return 'g';
  if (!strcmp(tok, "mint")) return 'm';
  if (!strcmp(tok, "aqua")) return 'a';
  if (!strcmp(tok, "blue")) return 'b';
  if (!strcmp(tok, "purple")) return 'p';
  if (!strcmp(tok, "violet")) return 'v';
  return '\0';
}

static bool netpsIsHoldToken(const char *tok)
{
  return (tok && !strcmp(tok, "hold"));
}

static bool netpsParseIntToken(const char *tok, int *out)
{
  char *end;
  long val;
  if (!tok || !*tok || !out) return false;
  val = strtol(tok, &end, 10);
  if (*end) return false;
  *out = (int) val;
  return true;
}

static bool netpsEqNoCase(const char *left, const char *right)
{
  if (!left || !right) return false;
  while (*left && *right)
  {
    if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) return false;
    left++;
    right++;
  }
  return (!*left && !*right);
}

static void netpsNormalizeFqdn(const char *src, char *dst, size_t dstsz)
{
  size_t pos = 0, end;
  if (!dst || !dstsz)
    return;
  dst[0] = '\0';
  if (!src || !*src)
    return;
  while (*src && isspace((unsigned char)*src)) src++;
  while (*src && (pos + 1 < dstsz))
  {
    if (!isspace((unsigned char)*src)) dst[pos++] = (char) tolower((unsigned char)*src);
    src++;
  }
  dst[pos] = '\0';
  end = strlen(dst);
  while (end && (dst[end - 1] == '.'))
  {
    dst[end - 1] = '\0';
    end--;
  }
}

static bool netpsNormalizeMacToken(const char *src, char *dst, size_t dstsz)
{
  unsigned int values[6];
  if (!src || !*src || !dst || (dstsz < 18)) return false;
  if ((sscanf(src, "%2x:%2x:%2x:%2x:%2x:%2x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) != 6)
      && (sscanf(src, "%2x-%2x-%2x-%2x-%2x-%2x", &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]) != 6))
    return false;
  snprintf(dst, dstsz, "%02X:%02X:%02X:%02X:%02X:%02X",
           values[0], values[1], values[2], values[3], values[4], values[5]);
  return true;
}

static bool netpsParseIpValue(const char *src, char *dst, size_t dstsz)
{
  unsigned long addr;
  if (!src || !*src || !dst || !dstsz) return false;
  addr = inet_addr(src);
  if ((addr == INADDR_NONE) && strcmp(src, "255.255.255.255")) return false;
  strncpy(dst, src, dstsz - 1);
  dst[dstsz - 1] = '\0';
  return true;
}

static char *netpsNextToken(char **cursor)
{
  char *start, *end;
  if (!cursor || !*cursor) return 0;
  start = *cursor;
  while (*start && isspace((unsigned char)*start)) start++;
  if (!*start || (*start == '#'))
  {
    *cursor = start;
    return 0;
  }
  end = start;
  while (*end && !isspace((unsigned char)*end)) end++;
  if (*end)
  {
    *end = '\0';
    end++;
  }
  *cursor = end;
  return start;
}

static bool netpsParsePosRule(char *cursor, netp_type *out)
{
  char *net, *x, *y, *z, *tok1 = 0, *tok2 = 0;
  if (!cursor || !out) return false;
  net = netpsNextToken(&cursor);
  x = netpsNextToken(&cursor);
  y = netpsNextToken(&cursor);
  z = netpsNextToken(&cursor);
  tok1 = netpsNextToken(&cursor);
  tok2 = netpsNextToken(&cursor);
  if (!net || !x || !y || !z || !tok1 || netpsNextToken(&cursor)) return false;
  if (strlen(net) >= sizeof(out->nt)) return false;
  strcpy(out->nt, net);
  if (!netpsParseIntToken(x, &out->px) || !netpsParseIntToken(y, &out->py) || !netpsParseIntToken(z, &out->pz)) return false;
  out->rtp = 'p';
  if (netpsIsHoldToken(tok1)) out->hld = 1;
  else if ((out->clr = netpsColorToken(tok1)) == '\0') return false;
  if (tok2)
  {
    if (netpsIsHoldToken(tok2)) out->hld = 1;
    else if (!out->clr && (out->clr = netpsColorToken(tok2))) {}
    else return false;
  }
  return (out->clr || out->hld);
}

static bool netpsParseHostRule(char *cursor, netp_type *out)
{
  char *tok;
  bool gotx = false, goty = false, gotz = false;
  if (!cursor || !out) return false;
  out->rtp = 'h';
  while ((tok = netpsNextToken(&cursor)))
  {
    char *eq;
    if (netpsIsHoldToken(tok))
    {
      out->hld = 1;
      continue;
    }
    eq = strchr(tok, '=');
    if (!eq) return false;
    *eq = '\0';
    eq++;
    if (!*eq) return false;
    if (netpsEqNoCase(tok, "ip"))
    {
      if (out->hip || !netpsParseIpValue(eq, out->ip, sizeof(out->ip))) return false;
      out->hip = true;
    }
    else if (netpsEqNoCase(tok, "mac"))
    {
      if (out->hmc || !netpsNormalizeMacToken(eq, out->mac, sizeof(out->mac))) return false;
      out->hmc = true;
    }
    else if (netpsEqNoCase(tok, "fqdn"))
    {
      if (out->hfq) return false;
      netpsNormalizeFqdn(eq, out->fqdn, sizeof(out->fqdn));
      if (!*out->fqdn) return false;
      out->hfq = true;
    }
    else if (netpsEqNoCase(tok, "x"))
    {
      if (gotx || !netpsParseIntToken(eq, &out->px)) return false;
      gotx = true;
    }
    else if (netpsEqNoCase(tok, "y"))
    {
      if (goty || !netpsParseIntToken(eq, &out->py)) return false;
      goty = true;
    }
    else if (netpsEqNoCase(tok, "z"))
    {
      if (gotz || !netpsParseIntToken(eq, &out->pz)) return false;
      gotz = true;
    }
    else if (netpsEqNoCase(tok, "color"))
    {
      if (out->clr || ((out->clr = netpsColorToken(eq)) == '\0')) return false;
    }
    else return false;
  }
  if (!(out->hip || out->hmc || out->hfq)) return false;
  if (!gotx || !goty || !gotz) return false;
  return (out->clr || out->hld);
}

static bool netpsParseLine(const char *line, netp_type *out)
{
  char buf[512], *cursor, *kw;
  if (!line || !out) return false;
  memset(out, 0, sizeof(*out));
  strncpy(buf, line, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  cursor = buf;
  kw = netpsNextToken(&cursor);
  if (!kw) return false;
  if (!strcmp(kw, "pos")) return netpsParsePosRule(cursor, out);
  if (!strcmp(kw, "host")) return netpsParseHostRule(cursor, out);
  return false;
}

static bool netpExactIp(const netp_type *ne, in_addr_t *ip)
{
  char ntmp[19], *cd;
  if (!ne) return false;
  if (ne->rtp == 'h')
  {
    if (!ne->hip) return false;
    if (ip) *ip = inet_addr(ne->ip);
    return true;
  }
  strcpy(ntmp, ne->nt);
  cd = strchr(ntmp, '/');
  if (!cd || strcmp(cd, "/32")) return false;
  *cd = '\0';
  if (!*ntmp) return false;
  if (ip) *ip = inet_addr(ntmp);
  return true;
}

static void netpApplyVisual(host_type *ht, const netp_type *ne)
{
  if (!ht || !ne) return;
  switch (ne->clr)
  {
    case 'd': ht->clr = 0; break;
    case 'o': ht->clr = 1; break;
    case 'y': ht->clr = 2; break;
    case 'f': ht->clr = 3; break;
    case 'g': ht->clr = 4; break;
    case 'm': ht->clr = 5; break;
    case 'a': ht->clr = 6; break;
    case 'b': ht->clr = 7; break;
    case 'p': ht->clr = 8; break;
    case 'v': ht->clr = 9; break;
  }
  ht->px = ne->px * SPC;
  ht->py = ne->py * SPC;
  ht->pz = ne->pz * SPC;
}

static bool netpMatchHostRule(const netp_type *ne, const host_type *ht)
{
  char buf[256], mac[18];
  if (!ne || !ht || (ne->rtp != 'h')) return false;
  if (ne->hip && strcmp(ne->ip, ht->htip)) return false;
  if (ne->hmc)
  {
    if (!*ht->htmc || !netpsNormalizeMacToken(ht->htmc, mac, sizeof(mac)) || strcmp(ne->mac, mac)) return false;
  }
  if (ne->hfq)
  {
    if (!*ht->htnm) return false;
    netpsNormalizeFqdn(ht->htnm, buf, sizeof(buf));
    if (!*buf || strcmp(ne->fqdn, buf)) return false;
  }
  return true;
}

//cross object vertexs, squares
cobj_type cobj =
{
  {
    { 640.0, 0.0,    4.0},
    { 640.0, 0.0,    8.0},
    {   8.0, 0.0,    8.0},
    {   8.0, 0.0,  640.0},
    {   4.0, 0.0,  640.0},
    {   4.0, 0.0,    4.0},
    {   8.0, 0.0,    4.0},

    {  -4.0, 0.0,    4.0},
    {  -4.0, 0.0,  640.0},
    {  -8.0, 0.0,  640.0},
    {  -8.0, 0.0,    8.0},
    {-640.0, 0.0,    8.0},
    {-640.0, 0.0,    4.0},
    {  -8.0, 0.0,    4.0},

    {  -4.0, 0.0, -640.0},
    {  -4.0, 0.0,   -4.0},
    {  -8.0, 0.0,   -4.0},
    {-640.0, 0.0,   -4.0},
    {-640.0, 0.0,   -8.0},
    {  -8.0, 0.0,   -8.0},
    {  -8.0, 0.0, -640.0},

    {   8.0, 0.0, -640.0},
    {   8.0, 0.0,   -8.0},
    { 640.0, 0.0,   -8.0},
    { 640.0, 0.0,   -4.0},
    {   8.0, 0.0,   -4.0},
    {   4.0, 0.0,   -4.0},
    {   4.0, 0.0, -640.0}
  },
  {
    { 0,  1,  2,  6},
    { 6,  3,  4,  5},
    { 7,  8,  9, 13},
    {13, 10, 11, 12},
    {14, 15, 16, 20},
    {19, 16, 17, 18},
    {21, 25, 26, 27},
    {23, 24, 25, 22}
  }
};

//host object vertexs, triangles
hobj_type hobj =
{
  {
    { 0.0, 10.0,  0.0},
    {-3.0,  8.0,  3.0},
    {-3.0,  8.0, -3.0},
    { 3.0,  8.0, -3.0},
    { 3.0,  8.0,  3.0},
    {-3.0,  2.0,  3.0},
    {-3.0,  2.0, -3.0},
    { 3.0,  2.0, -3.0},
    { 3.0,  2.0,  3.0},
    { 0.0,  0.0,  0.0}
  },
  {
    {1, 0, 4},
    {4, 0, 3},
    {3, 0, 2},
    {2, 0, 1},
    {1, 4, 8},
    {8, 5, 1},
    {4, 3, 7},
    {7, 8, 4},
    {3, 2, 6},
    {6, 7, 3},
    {2, 1, 5},
    {5, 6, 2},
    {5, 8, 9},
    {8, 7, 9},
    {7, 6, 9},
    {6, 5, 9}
  }
};

//multiple-hosts object vertexs, triangles
mobj_type mobj =
{
  {
    { 0.0, 10.0,  0.0},
    {-3.0,  8.0,  3.0},
    {-3.0,  8.0, -3.0},
    { 3.0,  8.0, -3.0},
    { 3.0,  8.0,  3.0},
    { 0.0,  5.0,  0.0},
    {-3.0,  2.0,  3.0},
    {-3.0,  2.0, -3.0},
    { 3.0,  2.0, -3.0},
    { 3.0,  2.0,  3.0},
    { 0.0,  0.0,  0.0}
  },
  {
    {1, 0,  4},
    {4, 0,  3},
    {3, 0,  2},
    {2, 0,  1},
    {1, 4,  5},
    {4, 3,  5},
    {3, 2,  5},
    {2, 1,  5},
    {6, 5,  9},
    {9, 5,  8},
    {8, 5,  7},
    {7, 5,  6},
    {6, 9, 10},
    {9, 8, 10},
    {8, 7, 10},
    {7, 6, 10}
  }
};

//packet object vertexs, triangles
pobj_type pobj =
{
  {
    {-1.0, 6.0,  1.0},
    {-1.0, 6.0, -1.0},
    { 1.0, 6.0, -1.0},
    { 1.0, 6.0,  1.0},
    {-1.0, 4.0,  1.0},
    {-1.0, 4.0, -1.0},
    { 1.0, 4.0, -1.0},
    { 1.0, 4.0,  1.0}
  },
  {
    {0, 1, 2},
    {2, 3, 0},
    {0, 3, 7},
    {7, 4, 0},
    {3, 2, 6},
    {6, 7, 3},
    {2, 1, 5},
    {5, 6, 2},
    {1, 0, 4},
    {4, 5, 1},
    {4, 5, 6},
    {6, 7, 4}
  }
};

//draw cross object
void cobjDraw()
{
  glBegin(GL_QUADS);
  for (unsigned char obj = 0; obj < 8; obj++)
  {
    switch (obj)
    {
      case 0: case 1: glColor3ub(grey[0], grey[1], grey[2]); break;
      case 2: case 3: glColor3ub(blue[0], blue[1], blue[2] - 50); break;
      case 4: case 5: glColor3ub(green[0], green[1] - 50, green[2]); break;
      case 6: case 7: glColor3ub(red[0] - 50, red[1], red[2]); break;
    }
    glVertex3f(cobj.vtx[cobj.sqr[obj].a].x, cobj.vtx[cobj.sqr[obj].a].y, cobj.vtx[cobj.sqr[obj].a].z);
    glVertex3f(cobj.vtx[cobj.sqr[obj].b].x, cobj.vtx[cobj.sqr[obj].b].y, cobj.vtx[cobj.sqr[obj].b].z);
    glVertex3f(cobj.vtx[cobj.sqr[obj].c].x, cobj.vtx[cobj.sqr[obj].c].y, cobj.vtx[cobj.sqr[obj].c].z);
    glVertex3f(cobj.vtx[cobj.sqr[obj].d].x, cobj.vtx[cobj.sqr[obj].d].y, cobj.vtx[cobj.sqr[obj].d].z);
  }
  glEnd();
}

//draw host object
void hobjDraw(int r, int g, int b)
{
  glBegin(GL_TRIANGLES);
  for (unsigned char obj = 0; obj < 16; obj++)
  {
    glColor3ub(r, g, b);
    glVertex3f(hobj.vtx[hobj.tri[obj].a].x, hobj.vtx[hobj.tri[obj].a].y, hobj.vtx[hobj.tri[obj].a].z);
    glColor3ub((r == 50 ? r : r - 50), (g == 50 ? g : g - 50), (b == 50 ? b : b - 50));
    glVertex3f(hobj.vtx[hobj.tri[obj].b].x, hobj.vtx[hobj.tri[obj].b].y, hobj.vtx[hobj.tri[obj].b].z);
    glColor3ub((r == 50 ? r : r - 100), (g == 50 ? g : g - 100), (b == 50 ? b : b - 100));
    glVertex3f(hobj.vtx[hobj.tri[obj].c].x, hobj.vtx[hobj.tri[obj].c].y, hobj.vtx[hobj.tri[obj].c].z);
  }
  glEnd();
}

//draw multiple-hosts object
void mobjDraw(bool sel)
{
  glBegin(GL_TRIANGLES);
  unsigned char obj = 0;
  for (; obj < 8; obj++)
  {
    (sel ? glColor3ub(brred[0], brred[1], brred[2]) : glColor3ub(red[0], red[1], red[2]));
    glVertex3f(mobj.vtx[mobj.tri[obj].a].x, mobj.vtx[mobj.tri[obj].a].y, mobj.vtx[mobj.tri[obj].a].z);
    (sel ? glColor3ub(brred[0] - 50, brred[1], brred[2]) : glColor3ub(yellow[0], yellow[1], yellow[2]));
    glVertex3f(mobj.vtx[mobj.tri[obj].b].x, mobj.vtx[mobj.tri[obj].b].y, mobj.vtx[mobj.tri[obj].b].z);
    (sel ? glColor3ub(brred[0] - 100, brred[1], brred[2]) : glColor3ub(green[0], green[1], green[2]));
    glVertex3f(mobj.vtx[mobj.tri[obj].c].x, mobj.vtx[mobj.tri[obj].c].y, mobj.vtx[mobj.tri[obj].c].z);
  }
  for (; obj < 16; obj++)
  {
    glColor3ub(red[0], red[1], red[2]);
    glVertex3f(mobj.vtx[mobj.tri[obj].a].x, mobj.vtx[mobj.tri[obj].a].y, mobj.vtx[mobj.tri[obj].a].z);
    glColor3ub(yellow[0], yellow[1], yellow[2]);
    glVertex3f(mobj.vtx[mobj.tri[obj].b].x, mobj.vtx[mobj.tri[obj].b].y, mobj.vtx[mobj.tri[obj].b].z);
    glColor3ub(green[0], green[1], green[2]);
    glVertex3f(mobj.vtx[mobj.tri[obj].c].x, mobj.vtx[mobj.tri[obj].c].y, mobj.vtx[mobj.tri[obj].c].z);
  }
  glEnd();
}

void mobjDrawMono(int r, int g, int b)
{
  glBegin(GL_TRIANGLES);
  for (unsigned char obj = 0; obj < 16; obj++)
  {
    glColor3ub(r, g, b);
    glVertex3f(mobj.vtx[mobj.tri[obj].a].x, mobj.vtx[mobj.tri[obj].a].y, mobj.vtx[mobj.tri[obj].a].z);
    glColor3ub((r < 50 ? r : r - 50), (g < 50 ? g : g - 50), (b < 50 ? b : b - 50));
    glVertex3f(mobj.vtx[mobj.tri[obj].b].x, mobj.vtx[mobj.tri[obj].b].y, mobj.vtx[mobj.tri[obj].b].z);
    glColor3ub((r < 100 ? 0 : r - 100), (g < 100 ? 0 : g - 100), (b < 100 ? 0 : b - 100));
    glVertex3f(mobj.vtx[mobj.tri[obj].c].x, mobj.vtx[mobj.tri[obj].c].y, mobj.vtx[mobj.tri[obj].c].z);
  }
  glEnd();
}

static unsigned char packetColorClamp(int value)
{
  if (value < 0) return 0;
  if (value > 255) return 255;
  return (unsigned char)value;
}

static void packetColorBase(int c, int *r, int *g, int *b)
{
  switch (c)
  {
    case 1: *r = red[0]; *g = red[1]; *b = red[2]; break;
    case 2: *r = green[0]; *g = green[1]; *b = green[2]; break;
    case 3: *r = blue[0]; *g = blue[1]; *b = blue[2]; break;
    case 4: *r = yellow[0]; *g = yellow[1]; *b = yellow[2]; break;
    case 5: *r = orange[0]; *g = orange[1]; *b = orange[2]; break;
    default: *r = grey[0]; *g = grey[1]; *b = grey[2]; break;
  }
}

static void packetColorShade(int c, int shade)
{
  int r, g, b;
  packetColorBase(c, &r, &g, &b);
  if (c == 0)
  {
    if (shade == 0) glColor3ub(brgrey[0], brgrey[1], brgrey[2]);
    else if (shade == 1) glColor3ub(grey[0], grey[1], grey[2]);
    else glColor3ub(dlgrey[0], dlgrey[1], dlgrey[2]);
    return;
  }
  if (shade == 0) glColor3ub(packetColorClamp(r + 100), packetColorClamp(g + 100), packetColorClamp(b + 100));
  else if (shade == 1) glColor3ub(packetColorClamp(r + 50), packetColorClamp(g + 50), packetColorClamp(b + 50));
  else glColor3ub((unsigned char)r, (unsigned char)g, (unsigned char)b);
}

static void packetBoxDraw(int c, float zhalf)
{
  vtx_type vtx[8] =
  {
    {-1.0f,  1.0f,  zhalf},
    {-1.0f,  1.0f, -zhalf},
    { 1.0f,  1.0f, -zhalf},
    { 1.0f,  1.0f,  zhalf},
    {-1.0f, -1.0f,  zhalf},
    {-1.0f, -1.0f, -zhalf},
    { 1.0f, -1.0f, -zhalf},
    { 1.0f, -1.0f,  zhalf}
  };

  glBegin(GL_TRIANGLES);
  for (unsigned char obj = 0; obj < 12; obj++)
  {
    packetColorShade(c, 0);
    glVertex3f(vtx[pobj.tri[obj].a].x, vtx[pobj.tri[obj].a].y, vtx[pobj.tri[obj].a].z);
    packetColorShade(c, 1);
    glVertex3f(vtx[pobj.tri[obj].b].x, vtx[pobj.tri[obj].b].y, vtx[pobj.tri[obj].b].z);
    packetColorShade(c, 2);
    glVertex3f(vtx[pobj.tri[obj].c].x, vtx[pobj.tri[obj].c].y, vtx[pobj.tri[obj].c].z);
  }
  glEnd();
}

//draw packet object
void pobjDraw(int c)
{
  packetBoxDraw(c, 1.0f);
}

//draw double-length packet object
void pobj2Draw(int c)
{
  packetBoxDraw(c, 2.0f);
}

//draw triple-length packet object
void pobj3Draw(int c)
{
  packetBoxDraw(c, 3.0f);
}

//draw spherical packet object
void psobjDraw(int c)
{
  const int stacks = 6, slices = 10;
  const float radius = 1.35f;
  const double PI = 3.14159265358979323846;
  for (int lat = 0; lat < stacks; lat++)
  {
    double lat0 = (-PI / 2.0) + ((double)lat * PI / (double)stacks);
    double lat1 = (-PI / 2.0) + ((double)(lat + 1) * PI / (double)stacks);
    float y0 = (float)(sin(lat0) * radius);
    float y1 = (float)(sin(lat1) * radius);
    float r0 = (float)(cos(lat0) * radius);
    float r1 = (float)(cos(lat1) * radius);
    glBegin(GL_QUAD_STRIP);
    for (int lon = 0; lon <= slices; lon++)
    {
      double ang = ((double)lon * 2.0 * PI / (double)slices);
      float cx = (float)cos(ang), cz = (float)sin(ang);
      packetColorShade(c, (lat < 2 ? 0 : (lat < 4 ? 1 : 2)));
      glVertex3f(cx * r0, y0, cz * r0);
      packetColorShade(c, (lat < 1 ? 0 : (lat < 3 ? 1 : 2)));
      glVertex3f(cx * r1, y1, cz * r1);
    }
    glEnd();
  }
}

//draw pyramid packet object
void ppobjDraw(int c)
{
  static const vtx_type vtx[5] =
  {
    { 0.0f,  1.6f,  0.0f},
    {-1.0f, -1.0f,  1.0f},
    {-1.0f, -1.0f, -1.0f},
    { 1.0f, -1.0f, -1.0f},
    { 1.0f, -1.0f,  1.0f}
  };

  glBegin(GL_TRIANGLES);
  for (unsigned char face = 0; face < 4; face++)
  {
    packetColorShade(c, 0);
    glVertex3f(vtx[0].x, vtx[0].y, vtx[0].z);
    packetColorShade(c, 1);
    glVertex3f(vtx[1 + face].x, vtx[1 + face].y, vtx[1 + face].z);
    packetColorShade(c, 2);
    glVertex3f(vtx[1 + ((face + 1) % 4)].x, vtx[1 + ((face + 1) % 4)].y, vtx[1 + ((face + 1) % 4)].z);
  }
  glEnd();

  glBegin(GL_TRIANGLES);
  packetColorShade(c, 2);
  glVertex3f(vtx[1].x, vtx[1].y, vtx[1].z);
  glVertex3f(vtx[2].x, vtx[2].y, vtx[2].z);
  glVertex3f(vtx[3].x, vtx[3].y, vtx[3].z);
  glVertex3f(vtx[3].x, vtx[3].y, vtx[3].z);
  glVertex3f(vtx[4].x, vtx[4].y, vtx[4].z);
  glVertex3f(vtx[1].x, vtx[1].y, vtx[1].z);
  glEnd();
}

//draw alert object
void aobjDraw(bool an)
{
  if (an)
  {
    glBegin(GL_LINE_LOOP);
      glVertex3f(-0.4,  0.7,  0.4);
      glVertex3f(-0.4,  0.7, -0.4);
      glVertex3f(-0.7,  0.0, -0.7);
      glVertex3f(-0.4, -0.7, -0.4);
      glVertex3f(-0.4, -0.7,  0.4);
      glVertex3f(-0.7,  0.0,  0.7);
    glEnd();
    glBegin(GL_LINE_LOOP);
      glVertex3f( 0.4,  0.7,  0.4);
      glVertex3f( 0.4,  0.7, -0.4);
      glVertex3f( 0.7,  0.0, -0.7);
      glVertex3f( 0.4, -0.7, -0.4);
      glVertex3f( 0.4, -0.7,  0.4);
      glVertex3f( 0.7,  0.0,  0.7);
    glEnd();
  }
  else
  {
    glBegin(GL_LINE_LOOP);
      glVertex3f(-0.5,  0.5,  0.5);
      glVertex3f(-0.5,  0.5, -0.5);
      glVertex3f( 0.5,  0.5, -0.5);
      glVertex3f( 0.5,  0.5,  0.5);
    glEnd();
    glBegin(GL_LINES);
      glVertex3f(-0.5,  0.5,  0.5);
      glVertex3f(-0.5, -0.5,  0.5);
    glEnd();
    glBegin(GL_LINES);
      glVertex3f(-0.5,  0.5, -0.5);
      glVertex3f(-0.5, -0.5, -0.5);
    glEnd();
    glBegin(GL_LINES);
      glVertex3f( 0.5,  0.5, -0.5);
      glVertex3f( 0.5, -0.5, -0.5);
    glEnd();
    glBegin(GL_LINES);
      glVertex3f( 0.5,  0.5,  0.5);
      glVertex3f( 0.5, -0.5,  0.5);
    glEnd();
    glBegin(GL_LINE_LOOP);
      glVertex3f(-0.5, -0.5,  0.5);
      glVertex3f(-0.5, -0.5, -0.5);
      glVertex3f( 0.5, -0.5, -0.5);
      glVertex3f( 0.5, -0.5,  0.5);
    glEnd();
  }
}

//return path with hosts3d data directory
char *hsddata(const char *fl)
{
  strcpy(fullpath, HSDDATA);
  return strcat(fullpath, fl);
}

//add service to host
void svcAdd(host_type *sht, int svc, bool anm)
{
  for (unsigned char cnt = 0; cnt < SVCS; cnt++)
  {
    if (sht->svc[cnt] == svc) break;
    if (sht->svc[cnt] == -1)
    {
      sht->svc[cnt] = svc;
      if (anm) sht->anm = 1;
      break;
    }
  }
}

//load netpos file into memory, create empty
void netpsLoad()
{
  FILE *npos;
  if ((npos = fopen(hsddata("netpos.txt"), "r")))
  {
    netpsDestroy();
    netp_type ne;
    char line[256];
    while (fgets(line, sizeof(line), npos))
      if (netpsParseLine(line, &ne)) ntpsLL.Write(new netp_type(ne));
    fclose(npos);
  }
  else if ((npos = fopen(hsddata("netpos.txt"), "w")))
  {
    fputs("#pos net x-position y-position z-position colour [hold]\n", npos);
    fputs("#host ip=1.2.3.4 [mac=00:11:22:33:44:55] [fqdn=host.example] x=0 y=0 z=0 color=green [hold]\n", npos);
    fclose(npos);
  }
}

bool netpsLineExactIp(const char *line, in_addr *ip)
{
  netp_type ne;
  in_addr_t addr;
  if (!line || !ip) return false;
  if (!netpsParseLine(line, &ne)) return false;
  if (!netpExactIp(&ne, &addr)) return false;
  ip->s_addr = addr;
  return true;
}

//destroy net positions LL
void netpsDestroy()
{
  netp_type *ne;
  ntpsLL.Start(1);
  while ((ne = (netp_type *)ntpsLL.Read(1)))
  {
    delete ne;
    ntpsLL.Next(1);
  }
  ntpsLL.Destroy();
}

//check if an IP address is a broadcast address
in_addr_t isBroadcast(in_addr ip)
{
  in_addr_t mask;
  char ntmp[19], *cd;
  netp_type *ne;
  ntpsLL.Start(2);
  while ((ne = (netp_type *)ntpsLL.Read(2)))
  {
    if (ne->rtp == 'h')
    {
      ntpsLL.Next(2);
      continue;
    }
    strcpy(ntmp, ne->nt);
    if ((cd = strchr(ntmp, '/')))
    {
      *cd = '\0';
      if (atoi(++cd)) mask = htonl(0xffffffff << (32 - atoi(cd)));
      else mask = 0;
      if ((inet_addr(ntmp) | ~mask) == ip.s_addr) return mask;
    }
    ntpsLL.Next(2);
  }
  return 0;
}

//check if a host is to be positioned into a net
int hostNet(host_type *ht)
{
  netp_type *ne;
  ntpsLL.Start(1);
  while ((ne = (netp_type *)ntpsLL.Read(1)))
  {
    if (((ne->rtp == 'h') && netpMatchHostRule(ne, ht))
        || ((ne->rtp != 'h') && inNet(ne->nt, ht->htip)))
    {
      netpApplyVisual(ht, ne);
      return (ne->hld ? 2 : 1);
    }
    ntpsLL.Next(1);
  }
  return 0;
}

//copy packet traffic file from/to
void pkrcCopy(const char *ifn, const char *ofn)
{
  FILE *ifile, *ofile;
  if ((ifile = fopen(ifn, "rb")))
  {
    char hpt[4] = {0, 0, 0, 0}, buf[4096];
    size_t got;
    if (fread(hpt, 1, 3, ifile) == 3)
    {
      if (!strcmp(hpt, "HPT") || !strcmp(hpt, "HP2"))
      {
        if ((ofile = fopen(ofn, "wb")))
        {
          rewind(ifile);
          while ((got = fread(buf, 1, sizeof(buf), ifile)) > 0)
          {
            if (fwrite(buf, 1, got, ofile) != got) break;
          }
          fclose(ofile);
        }
      }
    }
    fclose(ifile);
  }
}
