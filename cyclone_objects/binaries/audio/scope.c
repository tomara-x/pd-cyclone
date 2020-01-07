/* Copyright (c) 2002-2005 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.

- LATER cache gui commands & think about resizing - mouse events aren't bound to any of scope~'s
 'widget', but to a special item created only for a selected scope~. For the other scheme see the
 'comment' class (no indicator there, though - neither a handle, nor a pointer change). One way
 or the other, the traffic from the gui layer should be kept possibly low, at least in run-mode.
 
- 2016 = Derek Kwan: haven't cleaned out old dependencies (grow/loud/fitter/forky). Methods (bufsize,
 period, calccount, range, delay, trigger, triglevel, frgb/brgb) are rewritten, as well as attr
 declaration. Wrote color version that take vals 0-1 instead of 0-255.
- 2017-20 = Porres finished cleaning dependencies (sic/grow/loud/fitter/forky/all_guis/math),
cleanup the code drastically, fixed a regression bug and implemented zoom;
- Matt didn't put what he did but he's the one who did more stuff on this, like cresting a
 properties dialog and implementing the magic stuff */

#include "common/api.h"
#include "m_pd.h"
#include "g_canvas.h"
#include "common/magicbit.h"
#include <stdlib.h>
#include <string.h>

#define SCOPE_DEFSIZE       130
#define SCOPE_MINSIZE       18
#define SCOPE_MINPERIOD     2
#define SCOPE_MAXPERIOD     8192
#define SCOPE_MINBUFSIZE    8
#define SCOPE_MAXBUFSIZE    256
#define SCOPE_MINDELAY      0
#define SCOPE_DEFFGRED      205
#define SCOPE_DEFFGGREEN    229
#define SCOPE_DEFFGBLUE     232
#define SCOPE_DEFBGRED      74
#define SCOPE_DEFBGGREEN    79
#define SCOPE_DEFBGBLUE     77
#define SCOPE_DEFGRRED      96
#define SCOPE_DEFGRGREEN    98
#define SCOPE_DEFGRBLUE     102
#define SCOPE_SELBORDER     "#5aadef" // select color from max (apparently)
#define SCOPE_SELBDWIDTH    3
#define SCOPEHANDLE_SIZE    12
#define SCOPE_GUICHUNK      128 // performance-related hacks, LATER investigate

typedef struct _scope{
    t_object        x_obj;
    t_inlet        *x_rightinlet;
    t_glist        *x_glist;
    t_canvas       *x_canvas;
    char            x_tag[64];
    char            x_fgtag[64];
    char            x_bgtag[64];
    char            x_gridtag[64];
    char            x_margintag[64];
    int             x_width;
    int             x_height;
    float           x_minval;
    float           x_maxval;
    int             x_delay;
    int             x_trigmode;
    float           x_triglevel;
    int             x_drawstyle;
    unsigned char   x_fgrgb[3];
    unsigned char   x_bgrgb[3];
    unsigned char   x_grrgb[3];
    int             x_xymode;
    float           x_xbuffer[SCOPE_MAXBUFSIZE*4];
    float           x_ybuffer[SCOPE_MAXBUFSIZE*4];
    float           x_xbuflast[SCOPE_MAXBUFSIZE*4];
    float           x_ybuflast[SCOPE_MAXBUFSIZE*4];
    int             x_bufsize;
    int             x_lastbufsize;
    int             x_bufphase;
    int             x_period;
    int             x_phase;
    int             x_precount;
    int             x_retrigger;
    float           x_ksr;
    float           x_currx;
    float           x_curry;
    float           x_trigx;
    int             x_frozen;
    int             x_zoom;
    t_float        *x_signalscalar;
    t_clock        *x_clock;
    t_pd           *x_handle;
}t_scope;

typedef struct _scopehandle{
    t_pd       h_pd;
    t_scope   *h_master;
    t_symbol  *h_bindsym;
    char       h_pathname[64];
    char       h_outlinetag[64];
    int        h_dragon;
    int        h_dragx;
    int        h_dragy;
}t_scopehandle;

static t_class *scope_class, *scopehandle_class;
static t_widgetbehavior scope_widgetbehavior;

// ----------------- DRAW ----------------------------------------------------------------
static void scope_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2);

static void scope_drawfg(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
    float dx, dy, xx = 0, yy = 0, oldx, oldy, sc, xsc, ysc;
    float *xbp = x->x_xbuflast, *ybp = x->x_ybuflast;
    int bufsize = x->x_lastbufsize;
    if(x->x_xymode == 1){
        dx = (float)(x2 - x1) / (float)bufsize;
        oldx = x1;
        sc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    else if(x->x_xymode == 2){
        dy = (float)(y2 - y1) / (float)bufsize;
        oldy = y1;
        sc = ((float)x->x_width - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    else if(x->x_xymode == 3){
        xsc = ((float)x->x_width - 2.) / (float)(x->x_maxval - x->x_minval);
        ysc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    sys_vgui(".x%lx.c create line \\\n", cv);
    for(int i = 0; i < bufsize; i++){
        if(x->x_xymode == 1){
            xx = oldx;
            yy = (y2 - 1) - sc * (*xbp++ - x->x_minval);
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
            oldx += dx;
        }
        else if(x->x_xymode == 2){
            yy = oldy;
            xx = (x2 - 1) - sc * (*ybp++ - x->x_minval);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            oldy += dy;
        }
        else if(x->x_xymode == 3){
            xx = x1 + xsc * (*xbp++ - x->x_minval);
            yy = y2 - ysc * (*ybp++ - x->x_minval);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
        }
        sys_vgui("%d %d \\\n", (int)xx, (int)yy);
    }
    sys_vgui("-fill #%2.2x%2.2x%2.2x -width %d -tags {%s %s}\n",
        x->x_fgrgb[0], x->x_fgrgb[1], x->x_fgrgb[2], x->x_zoom, x->x_fgtag, x->x_tag);
}

static void scope_drawmargins(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
  // margin lines:  mask overflow so they appear as gaps and not clipped signal values, LATER rethink
  sys_vgui(".x%lx.c create line %d %d %d %d %d %d %d %d %d %d -fill #%2.2x%2.2x%2.2x -width %d -tags {%s %s}\n", cv, x1,
    y1 , x2, y1, x2, y2, x1, y2, x1, y1, x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2], x->x_zoom, x->x_margintag, x->x_tag);
}

static void scope_drawbg(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
    int i;
    float dx, dy, xx, yy;
    dx = (x2 - x1) * 0.125;
    dy = (y2 - y1) * 0.25;
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill #%2.2x%2.2x%2.2x -width %d -tags {%s %s}\n", cv, x1, y1,
        x2, y2, x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2], x->x_zoom, x->x_bgtag, x->x_tag);
    for(i = 0, xx = x1 + dx; i < 7; i++, xx += dx)
        sys_vgui(".x%lx.c create line %f %d %f %d -width %d -tags {%s %s} -fill #%2.2x%2.2x%2.2x\n", cv, xx, y1,
            xx, y2, x->x_zoom, x->x_gridtag, x->x_tag, x->x_grrgb[0], x->x_grrgb[1], x->x_grrgb[2]);
    for(i = 0, yy = y1 + dy; i < 3; i++, yy += dy)
        sys_vgui(".x%lx.c create line %d %f %d %f -width %d -tags {%s %s} -fill #%2.2x%2.2x%2.2x\n", cv, x1, yy,
            x2, yy, x->x_zoom, x->x_gridtag, x->x_tag, x->x_grrgb[0], x->x_grrgb[1], x->x_grrgb[2]);
}

static void scope_draw(t_scope *x, t_canvas *cv){
    int x1, y1, x2, y2;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    scope_drawbg(x, cv, x1, y1, x2, y2);
    if(x->x_xymode)
        scope_drawfg(x, cv, x1, y1, x2, y2);
    scope_drawmargins(x, cv, x1, y1, x2, y2);
}

static void scope_redraw(t_scope *x, t_canvas *cv){
    int bufsize;
    int nleft = bufsize = x->x_lastbufsize;
    char chunk[32 * SCOPE_GUICHUNK];  // LATER estimate
    char *chunkp = chunk;
    int x1, y1, x2, y2, xymode = x->x_xymode;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    float dx, dy, xx, yy, oldx, oldy, sc, xsc, ysc;
    float *xbp = x->x_xbuflast, *ybp = x->x_ybuflast;
    if(xymode == 1){
        dx = (float)(x2 - x1) / (float)bufsize;
        oldx = x1;
        sc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    else if(xymode == 2){
        dy = (float)(y2 - y1) / (float)bufsize;
        oldy = y1;
        sc = ((float)x->x_width - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    else if(xymode == 3){
        xsc = ((float)x->x_width - 2.) / (float)(x->x_maxval - x->x_minval);
        ysc = ((float)x->x_height - 2.) / (float)(x->x_maxval - x->x_minval);
    }
    sys_vgui(".x%lx.c coords %s \\\n", cv, x->x_fgtag);
    while(nleft > SCOPE_GUICHUNK){
        int i = SCOPE_GUICHUNK;
        while(i--){
            if(xymode == 1){
                xx = oldx;
                yy = (y2 - 1) - sc * (*xbp++ - x->x_minval);
                if(yy > y2)
                    yy = y2;
                else if(yy < y1)
                    yy = y1;
                oldx += dx;
            }
            else if(xymode == 2){
                yy = oldy;
                xx = (x2 - 1) - sc * (*ybp++ - x->x_minval);
                if(xx > x2)
                    xx = x2;
                else if(xx < x1)
                    xx = x1;
                oldy += dy;
            }
            else if(xymode == 3){
                xx = x1 + xsc * (*xbp++ - x->x_minval);
                yy = y2 - ysc * (*ybp++ - x->x_minval);
                if(xx > x2)
                    xx = x2;
                else if(xx < x1)
                    xx = x1;
                if(yy > y2)
                    yy = y2;
                else if(yy < y1)
                    yy = y1;
            }
            sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
            chunkp += strlen(chunkp);
        }
        strcpy(chunkp, "\\\n");
        sys_gui(chunk);
        chunkp = chunk;
        nleft -= SCOPE_GUICHUNK;
    }
    while(nleft--){
        if(xymode == 1){
            xx = oldx;
            yy = (y2 - 1) - sc * (*xbp++ - x->x_minval);
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
            oldx += dx;
        }
        else if(xymode == 2){
            yy = oldy;
            xx = (x2 - 1) - sc * (*ybp++ - x->x_minval);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            oldy += dy;
        }
        else if(xymode == 3){
            xx = x1 + xsc * (*xbp++ - x->x_minval);
            yy = y2 - ysc * (*ybp++ - x->x_minval);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
        }
        sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
        chunkp += strlen(chunkp);
    }
    strcpy(chunkp, "\n");
    sys_gui(chunk);
}

//------------------ WIDGET -----------------------------------------------------------------
static void scope_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2){
    t_scope *x = (t_scope *)z;
    float x1 = text_xpix((t_text *)x, gl), y1 = text_ypix((t_text *)x, gl);
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x1 + x->x_width;
    *yp2 = y1 + x->x_height;
}

static void scope_displace(t_gobj *z, t_glist *gl, int dx, int dy){
    t_scope *x = (t_scope *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    t_canvas *cv = glist_getcanvas(gl);
    sys_vgui(".x%lx.c move %s %d %d\n", cv, x->x_tag, dx*x->x_zoom, dy*x->x_zoom);
    canvas_fixlinesfor(cv, (t_text*)x);
}

static void scope_select(t_gobj *z, t_glist *glist, int state){
    t_scope *x = (t_scope *)z;
    t_canvas *cv = glist_getcanvas(glist);
    t_scopehandle *sh = (t_scopehandle *)x->x_handle;
    if(state){
        int x1, y1, x2, y2;
        scope_getrect(z, glist, &x1, &y1, &x2, &y2);
        sys_vgui(".x%lx.c itemconfigure %s -outline %s -width %d -fill #%2.2x%2.2x%2.2x\n",
            cv, x->x_bgtag, SCOPE_SELBORDER, SCOPE_SELBDWIDTH * x->x_zoom,
            x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2]);
        sys_vgui("canvas %s -width %d -height %d -bg #fedc00 -bd 0\n",
            sh->h_pathname, SCOPEHANDLE_SIZE, SCOPEHANDLE_SIZE);
        sys_vgui(".x%lx.c create window %d %d -anchor nw -width %d -height %d -window %s -tags %s\n", cv,
            x2 - (SCOPEHANDLE_SIZE*x->x_zoom - SCOPE_SELBDWIDTH*x->x_zoom),
            y2 - (SCOPEHANDLE_SIZE*x->x_zoom - SCOPE_SELBDWIDTH*x->x_zoom),
            SCOPEHANDLE_SIZE*x->x_zoom, SCOPEHANDLE_SIZE*x->x_zoom, sh->h_pathname, x->x_tag);
        sys_vgui("bind %s <Button> {pdsend [concat %s _click 1 \\;]}\n",
            sh->h_pathname, sh->h_bindsym->s_name);
        sys_vgui("bind %s <ButtonRelease> {pdsend [concat %s _click 0 \\;]}\n",
            sh->h_pathname, sh->h_bindsym->s_name);
        sys_vgui("bind %s <Motion> {pdsend [concat %s _motion %%x %%y \\;]}\n",
            sh->h_pathname, sh->h_bindsym->s_name);
    }
    else{
        sys_vgui(".x%lx.c itemconfigure %s -outline black -width %d -fill #%2.2x%2.2x%2.2x\n",
            cv, x->x_bgtag, x->x_zoom, x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2]);
        sys_vgui("destroy %s\n", sh->h_pathname);
    }
}

static void scope_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void scope_vis(t_gobj *z, t_glist *glist, int vis){
    t_scope *x = (t_scope *)z;
    x->x_canvas = glist_getcanvas(glist);
    if(vis){
        t_scopehandle *sh = (t_scopehandle *)x->x_handle;
        sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_canvas, (unsigned long)sh);
        int bufsize = x->x_bufsize;
        x->x_bufsize = x->x_lastbufsize;
        scope_draw(x, x->x_canvas);
        x->x_bufsize = bufsize;
    }
    else
        sys_vgui(".x%lx.c delete %s\n", (unsigned long)x->x_canvas, x->x_tag);
}

static void scope_motion(void){ // dummy
}

static int scope_click(t_gobj *z, t_glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    t_scope *x = (t_scope *)z;
    if(doit){
        x->x_frozen = 1;
        glist_grab(x->x_glist, &x->x_obj.te_g, (t_glistmotionfn)scope_motion, 0, xpix, ypix);
    }
    else
        x->x_frozen = 0;
    glist = NULL;
    shift = alt = dbl = 0;
    return(1);
}

//------------------------------ METHODS ------------------------------------------------------------------
static void scope_bufsize(t_scope *x, t_float bufsz){
    if(bufsz < SCOPE_MINBUFSIZE)
        x->x_bufsize = SCOPE_MINBUFSIZE;
    else if(bufsz > SCOPE_MAXBUFSIZE)
        x->x_bufsize = SCOPE_MAXBUFSIZE;
    else
        x->x_bufsize = bufsz;
    pd_float((t_pd *)x->x_rightinlet, x->x_bufsize);
    x->x_phase = 0;
    x->x_bufphase = 0;
    x->x_precount = 0;
}

static void scope_period(t_scope *x, t_float f){
    int period = f < 2 ? 2 : f > 8192 ? 8192 : (int)f;
    if(x->x_period != period){
        x->x_period = period;
        x->x_phase = 0;
        x->x_bufphase = 0;
        x->x_precount = 0;
    }
}

static void scope_range(t_scope *x, t_float min, t_float max){
    t_float minval = min;
    t_float maxval = max;
    if(minval < maxval){ // swapping, ignoring if equal
        x->x_minval = minval;
        x->x_maxval = maxval;
    }
    else{
        x->x_minval = maxval;
        x->x_maxval = minval;
    }
}

static void scope_delay(t_scope *x, t_float f){
    x->x_delay = f < 0 ? 0 : f;
}

static void scope_drawstyle(t_scope *x, t_float drawstyle){
    x->x_drawstyle = (int)(drawstyle != 0);
}

static void scope_trigger(t_scope *x, t_float trig){
    x->x_trigmode = trig < 0 ? 0 : trig > 2 ? 2 : (int)trig;
    if(x->x_trigmode == 0) // no trigger
        x->x_retrigger = 0;
}

static void scope_triglevel(t_scope *x, t_float f){
    x->x_triglevel = f;
}

static void scope_fgcolor(t_scope *x, t_float r, t_float g, t_float b){ //scale is 0-1
    x->x_fgrgb[0] = r < 0. ? 0 : r > 1. ? 255 : (int)(r * 255);
    x->x_fgrgb[1] = g < 0. ? 0 : g > 1. ? 255 : (int)(g * 255);
    x->x_fgrgb[2] = b < 0. ? 0 : b > 1. ? 255 : (int)(b * 255);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_fgtag, x->x_fgrgb[0], x->x_fgrgb[1], x->x_fgrgb[2]);
}

static void scope_frgb(t_scope *x, t_float r, t_float g, t_float b){ //scale is 0-255
    x->x_fgrgb[0] = (int)(r < 0 ? 0 : r > 255 ? 255 : r);
    x->x_fgrgb[1] = (int)(g < 0 ? 0 : g > 255 ? 255 : g);
    x->x_fgrgb[2] = (int)(b < 0 ? 0 : b > 255 ? 255 : b);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_fgtag, x->x_fgrgb[0], x->x_fgrgb[1], x->x_fgrgb[2]);
}

static void scope_bgcolor(t_scope *x, t_float r, t_float g, t_float b){ //scale: 0-1
    x->x_bgrgb[0] = r < 0. ? 0 : r > 1. ? 255 : (int)(r * 255);
    x->x_bgrgb[1] = g < 0. ? 0 : g > 1. ? 255 : (int)(g * 255);
    x->x_bgrgb[2] = b < 0. ? 0 : b > 1. ? 255 : (int)(b * 255);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_bgtag, x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2]);
}

static void scope_brgb(t_scope *x, t_float r, t_float g, t_float b){ // scale: 0-255
    x->x_bgrgb[0] = (int)(r < 0 ? 0 : r > 255 ? 255 : r);
    x->x_bgrgb[1] = (int)(g < 0 ? 0 : g > 255 ? 255 : g);
    x->x_bgrgb[2] = (int)(b < 0 ? 0 : b > 255 ? 255 : b);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_bgtag, x->x_bgrgb[0], x->x_bgrgb[1], x->x_bgrgb[2]);
}

static void scope_gridcolor(t_scope *x, t_float r, t_float g, t_float b){ //scale: 0-1
    x->x_grrgb[0] = r < 0. ? 0 : r > 1. ? 255 : (int)(r * 255);
    x->x_grrgb[1] = g < 0. ? 0 : g > 1. ? 255 : (int)(g * 255);
    x->x_grrgb[2] = b < 0. ? 0 : b > 1. ? 255 : (int)(b * 255);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_gridtag, x->x_grrgb[0], x->x_grrgb[1], x->x_grrgb[2]);
}

static void scope_grgb(t_scope *x, t_float r, t_float g, t_float b){ // scale: 0-255
    x->x_grrgb[0] = (int)(r < 0 ? 0 : r > 255 ? 255 : r);
    x->x_grrgb[1] = (int)(g < 0 ? 0 : g > 255 ? 255 : g);
    x->x_grrgb[2] = (int)(b < 0 ? 0 : b > 255 ? 255 : b);
    if(glist_isvisible(x->x_glist))
        sys_vgui(".x%lx.c itemconfigure %s -fill #%2.2x%2.2x%2.2x\n",
            glist_getcanvas(x->x_glist), x->x_gridtag, x->x_grrgb[0], x->x_grrgb[1], x->x_grrgb[2]);
}

static void scope_resize(t_scope *x, t_float w, t_float h){
    x->x_width  = (int)(w < SCOPE_MINSIZE ? SCOPE_MINSIZE : w);
    x->x_height = (int)(h < SCOPE_MINSIZE ? SCOPE_MINSIZE : h);
    if(glist_isvisible(x->x_glist)){
        if(x->x_xymode)
            scope_redraw(x, x->x_canvas);
        sys_vgui(".x%lx.c delete %s\n", x->x_canvas, x->x_tag);
        scope_draw(x, x->x_canvas);
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
    }
}

static void scope_dim(t_scope *x, t_float width, t_float height){
    if(width < SCOPE_MINSIZE)
        width = SCOPE_MINSIZE;
    if(height < SCOPE_MINSIZE)
        height = SCOPE_MINSIZE;
    x->x_width = (int)width * x->x_zoom;
    x->x_height = (int)height * x->x_zoom;
}

static void scope_zoom(t_scope *x, t_floatarg zoom){
    float mul = (zoom == 1. ? 0.5 : 2.);
    x->x_width = (int)((float)x->x_width * mul);
    x->x_height = (int)((float)x->x_height * mul);
    x->x_zoom = (int)zoom;
    scope_resize(x, x->x_width, x->x_height);
}

// --------------------- scopehandle ---------------------------------------------------
static void scopehandle__clickhook(t_scopehandle *sh, t_floatarg f){
    int newstate = (int)f;
    t_scope *x = sh->h_master;
    if(sh->h_dragon && newstate == 0){
        x->x_width += sh->h_dragx;
        x->x_height += sh->h_dragy;
        sys_vgui(".x%lx.c delete %s\n", x->x_canvas, sh->h_outlinetag);
        sys_vgui(".x%lx.c delete %s\n", x->x_canvas, x->x_tag);
        scope_draw(x, x->x_canvas);
        sys_vgui("destroy %s\n", sh->h_pathname);
        scope_select((t_gobj *)x, x->x_glist, 1);
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
    }
    else if(!sh->h_dragon && newstate){
        int x1, y1, x2, y2;
        scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        sys_vgui("lower %s\n", sh->h_pathname);
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -outline %s -width %d -tags %s\n",
            x->x_canvas, x1, y1, x2, y2, SCOPE_SELBORDER, SCOPE_SELBDWIDTH, sh->h_outlinetag);
        sh->h_dragx = sh->h_dragy = 0;
    }
    sh->h_dragon = newstate;
}

static void scopehandle__motionhook(t_scopehandle *sh, t_floatarg f1, t_floatarg f2){
    if(sh->h_dragon){
        t_scope *x = sh->h_master;
        int dx = (int)f1, dy = (int)f2, x1, y1, x2, y2, newx, newy;
        scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        newx = x2 + dx, newy = y2 + dy;
        if(newx > x1 + SCOPE_MINSIZE && newy > y1 + SCOPE_MINSIZE){
            sys_vgui(".x%lx.c coords %s %d %d %d %d\n", x->x_canvas, sh->h_outlinetag, x1, y1, newx, newy);
            sh->h_dragx = dx, sh->h_dragy = dy;
        }
    }
}

//------------------------------------------------------------
static t_int *scope_perform(t_int *w){
    t_scope *x = (t_scope *)(w[1]);
    if(!x->x_xymode) // do nothing
        return(w+5);
    int bufphase = x->x_bufphase;
    int bufsize = (int)*x->x_signalscalar;
    if(bufsize != x->x_bufsize){
        scope_bufsize(x, bufsize);
        bufsize = x->x_bufsize;
    }
    if(bufphase < bufsize){
        int nblock = (int)(w[2]);
        if(x->x_precount >= nblock)
            x->x_precount -= nblock;
        else{
            t_float *in1 = (t_float *)(w[3]);
            t_float *in2 = (t_float *)(w[4]);
            t_float *in;
            int phase = x->x_phase;
            int period = x->x_period;
            float freq = 1. / period;
            float *bp1;
            float *bp2;
            float currx = x->x_currx;
            float curry = x->x_curry;
            if(x->x_precount > 0){
                nblock -= x->x_precount;
                in1 += x->x_precount;
                in2 += x->x_precount;
                phase = 0;
                bufphase = 0;
                x->x_precount = 0;
            }
            if(x->x_trigmode && (x->x_xymode == 1 || x->x_xymode == 2)){
                if(x->x_xymode == 1)
                    in = in1;
                else
                    in = in2;
                while(x->x_retrigger){
                    float triglevel = x->x_triglevel;
                    if(x->x_trigmode == 1){ // Trigger Up
                        if(x->x_trigx < triglevel){
                            while(nblock--){
                                if(*in >= triglevel){
                                    x->x_retrigger = 0;
                                    phase = 0;
                                    bufphase = 0;
                                    break;
                                }
                                else
                                    in++;
                            }
                        }
                        else{
                            while(nblock--){
                                if(*in++ < triglevel){
                                    x->x_trigx = triglevel - 1.;
                                    break;
                                }
                            }
                        }
                    }
                    else{ // Trigger Down
                        if(x->x_trigx > triglevel){
                            while(nblock--){
                                if(*in <= triglevel){
                                    phase = 0;
                                    bufphase = 0;
                                    x->x_retrigger = 0;
                                    break;
                                }
                                else
                                    in++;
                            }
                        }
                        else{
                            while(nblock--){
                                if(*in++ > triglevel){
                                    x->x_trigx = triglevel + 1.;
                                    break;
                                }
                            }
                        }
                    }
                    if(nblock <= 0){
                        x->x_bufphase = bufphase;
                        x->x_phase = phase;
                        return(w+5);
                    }
                }
                if(x->x_xymode == 1)
                    in1 = in;
                else
                    in2 = in;
            }
            else if(x->x_retrigger)
                x->x_retrigger = 0;
            while(nblock--){
                bp1 = x->x_xbuffer + bufphase;
                bp2 = x->x_ybuffer + bufphase;
                if(phase){
                    t_float f1 = *in1++;
                    t_float f2 = *in2++;
                    if(x->x_xymode == 1){ // CHECKED
                        if(!x->x_drawstyle){
                            if((currx<0 && (f1<currx || f1>-currx)) || (currx>0 && (f1>currx || f1<-currx)))
                                currx = f1;
                        }
                        else{
                            if(f1 < currx)
                                currx = f1;
                        }
                        curry = 0.;
                    }
                    else if(x->x_xymode == 2){
                        if(!x->x_drawstyle){
                            if((curry<0 && (f2<curry || f2>-curry)) || (curry>0 && (f2>curry || f2<-curry)))
                                curry = f2;
                        }
                        else{
                            if(f2 < curry)
                                curry = f2;
                        }
                        currx = 0.;
                    }
                    else{
                        currx += f1;
                        curry += f2;
                    }
                }
                else{
                    currx = *in1++;
                    curry = *in2++;
                }
                if(currx != currx)
                    currx = 0.;  // CHECKED NaNs bashed to zeros
                if(curry != curry)
                    curry = 0.;
                if(++phase >= period){
                    phase = 0;
                    if(x->x_xymode == 3){
                        currx *= freq;
                        curry *= freq;
                    }
                    if(++bufphase >= bufsize){
                        *bp1 = currx;
                        *bp2 = curry;
                        bufphase = 0;
                        x->x_lastbufsize = bufsize;
                        memcpy(x->x_xbuflast, x->x_xbuffer, bufsize * sizeof(*x->x_xbuffer));
                        memcpy(x->x_ybuflast, x->x_ybuffer, bufsize * sizeof(*x->x_ybuffer));
                        x->x_retrigger = (x->x_trigmode != 0);
                        x->x_trigx = x->x_triglevel;
                        clock_delay(x->x_clock, 0);
                    }
                    else{
                        *bp1 = currx;
                        *bp2 = curry;
                    }
                }
            }
            x->x_currx = currx;
            x->x_curry = curry;
            x->x_bufphase = bufphase;
            x->x_phase = phase;
        }
    }
    return(w+5);
}

static void scope_dsp(t_scope *x, t_signal **sp){
    x->x_ksr = sp[0]->s_sr * 0.001;
    int xfeeder = magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int yfeeder = magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    int xymode = xfeeder + 2 * yfeeder;
    if(xymode != x->x_xymode){
        x->x_xymode = xymode;
        if(glist_isvisible(x->x_glist)){
            sys_vgui(".x%lx.c delete %s %s\n", x->x_canvas, x->x_fgtag, x->x_margintag);
            int x1, y1, x2, y2;
            scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
            if(x->x_xymode)
                scope_drawfg(x, x->x_canvas, x1, y1, x2, y2);
            scope_drawmargins(x, x->x_canvas, x1, y1, x2, y2);
        }
        x->x_precount = 0;
    }
    dsp_add(scope_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void scope_tick(t_scope *x){
    if(glist_isvisible(x->x_glist)){
        if(!x->x_canvas->gl_editor->e_onmotion)
            x->x_frozen = 0;
        if(!x->x_frozen && x->x_xymode)
                scope_redraw(x, x->x_canvas);
    }
    x->x_precount = (int)(x->x_delay * x->x_ksr);
}

static void scope_free(t_scope *x){
    if(x->x_clock)
        clock_free(x->x_clock);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_scopehandle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
}

static void scope_save(t_gobj *z, t_binbuf *b){
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)x;
    binbuf_addv(b, "ssiisiiiiiffiiifiiiiiiiiii;", gensym("#X"), gensym("obj"), (int)t->te_xpix,
        (int)t->te_ypix, atom_getsymbol(binbuf_getvec(t->te_binbuf)), x->x_width/x->x_zoom,
        x->x_height/x->x_zoom, x->x_period, 3, x->x_bufsize, x->x_minval, x->x_maxval, x->x_delay,
        0, x->x_trigmode, x->x_triglevel, x->x_fgrgb[0], x->x_fgrgb[1], x->x_fgrgb[2], x->x_bgrgb[0],
        x->x_bgrgb[1], x->x_bgrgb[2], x->x_grrgb[0], x->x_grrgb[1], x->x_grrgb[2], 0);
}

static void scope_properties(t_gobj *z, t_glist *owner){
    owner = NULL;
    t_scope *x = (t_scope *)z;
    int bgcol, grcol, fgcol;
    bgcol = ((int)x->x_bgrgb[0] << 16) + ((int)x->x_bgrgb[1] << 8) + (int)x->x_bgrgb[2];
    grcol = ((int)x->x_grrgb[0] << 16) + ((int)x->x_grrgb[1] << 8) + (int)x->x_grrgb[2];
    fgcol = ((int)x->x_fgrgb[0] << 16) + ((int)x->x_fgrgb[1] << 8) + (int)x->x_fgrgb[2];
    char buf[1000];
    sprintf(buf, "::dialog_scope::pdtk_scope_dialog %%s \
        dim %d wdt: %d hgt: \
        buf %d cal: %d bfs: \
        rng %g min: %g max: \
        del %d del: drs %d drs: \
        trg %d tmd: %g tlv: \
        dim_mins %d %d \
        cal_min_max %d %d bfs_min_max %d %d \
        del_mins %d \
        #%06x #%06x #%06x\n",
        x->x_width, x->x_height,
        x->x_period, x->x_bufsize,
        x->x_minval, x->x_maxval,
        x->x_delay, x->x_drawstyle,
        x->x_trigmode, x->x_triglevel,
        SCOPE_MINSIZE, SCOPE_MINSIZE,
        SCOPE_MINPERIOD, SCOPE_MAXPERIOD,
        SCOPE_MINBUFSIZE, SCOPE_MAXBUFSIZE,
        SCOPE_MINDELAY,
        bgcol, grcol, fgcol);
    gfxstub_new(&x->x_obj.ob_pd, x, buf);
}

static int scope_getcolorarg(int index, int ac, t_atom *av){
    if(index >= ac)
        return(0);
    if((av+index)->a_type == A_FLOAT)
        return atom_getintarg(index, ac, av);
    if((av+index)->a_type == A_SYMBOL){
        t_symbol *s = atom_getsymbolarg(index, ac, av);
        if('#' == s->s_name[0])
            return(strtol(s->s_name+1, 0, 16));
    }
    return(0);
}

static void scope_dialog(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    int width = (int)atom_getintarg(0, ac, av);
    int height = (int)atom_getintarg(1, ac, av);
    int period = (int)atom_getintarg(2, ac, av);
    int bufsize = (int)atom_getintarg(3, ac, av);
    float minval = (float)atom_getfloatarg(4, ac, av);
    float maxval = (float)atom_getfloatarg(5, ac, av);
    int delay = (int)atom_getintarg(6, ac, av);
    int drawstyle = (int)atom_getintarg(7, ac, av);
    int trigmode = (int)atom_getintarg(8, ac, av);
    float triglevel = (float)atom_getfloatarg(9, ac, av);
    int bgcol = (int)scope_getcolorarg(10, ac, av);
    int grcol = (int)scope_getcolorarg(11, ac, av);
    int fgcol = (int)scope_getcolorarg(12, ac, av);
    int bgred = (bgcol & 0xFF0000) >> 16;
    int bggreen = (bgcol & 0x00FF00) >> 8;
    int bgblue = (bgcol & 0x0000FF);
    int grred = (grcol & 0xFF0000) >> 16;
    int grgreen = (grcol & 0x00FF00) >> 8;
    int grblue = (grcol & 0x0000FF);
    int fgred = (fgcol & 0xFF0000) >> 16;
    int fggreen = (fgcol & 0x00FF00) >> 8;
    int fgblue = (fgcol & 0x0000FF);
    scope_period(x, period);
    scope_bufsize(x, bufsize);
    scope_range(x, minval, maxval);
    scope_delay(x, delay);
    scope_drawstyle(x, drawstyle);
    scope_trigger(x, trigmode);
    scope_triglevel(x, triglevel);
    if(x->x_width != width || x->x_height != height || x->x_bgrgb[0] != bgred || x->x_bgrgb[1] != bggreen
       || x->x_bgrgb[2] != bgblue || x->x_grrgb[0] != grred || x->x_grrgb[1] != grgreen || x->x_grrgb[2] != grblue
       || x->x_fgrgb[0] != fgred || x->x_fgrgb[1] != fggreen || x->x_fgrgb[2] != fgblue){
            scope_brgb(x, bgred, bggreen, bgblue);
            scope_grgb(x, grred, grgreen, grblue);
            scope_frgb(x, fgred, fggreen, fgblue);
            scope_resize(x, width, height);
            canvas_dirty(x->x_canvas, 1);
    }
}

static void *scope_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_scope *x = (t_scope *)pd_new(scope_class);
    x->x_handle = pd_new(scopehandle_class);
    t_scopehandle *sh = (t_scopehandle *)x->x_handle;
    sh->h_master = x;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
    x->x_xymode = x->x_frozen = x->x_precount = sh->h_dragon = 0;
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_canvas = 0;
    x->x_bufsize = 0;
    float width = SCOPE_DEFSIZE, height = SCOPE_DEFSIZE;
    float period = 256, bufsize = 128, minval = -1, maxval = 1;
    x->x_lastbufsize = (int)bufsize;
    float delay = 0, drawstyle = 0, trigger = 0, triglevel = 0;
    float fgred = (t_float)SCOPE_DEFFGRED;
    float fggreen = (t_float)SCOPE_DEFFGGREEN;
    float fgblue = (t_float)SCOPE_DEFFGBLUE;
    float bgred = (t_float)SCOPE_DEFBGRED;
    float bggreen = (t_float)SCOPE_DEFBGGREEN;
    float bgblue = (t_float)SCOPE_DEFBGBLUE;
    float grred = (t_float)SCOPE_DEFGRRED;
    float grgreen = (t_float)SCOPE_DEFGRGREEN;
    float grblue = (t_float)SCOPE_DEFGRBLUE;
    int fcolset = 0, bcolset = 0, gcolset = 0; // flag for colorset
    float fcred = 0., fcgreen = 0., fcblue = 0.;
    float bcred = 0., bcgreen = 0., bcblue = 0.;
    float gcred = 0., gcgreen = 0., gcblue = 0.;
    int argnum = 0;
    while(ac > 0){
        if(av -> a_type == A_FLOAT){
            t_float aval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    width = aval;
                    break;
                case 1:
                    height = aval;
                    break;
                case 2:
                    period = aval;
                    break;
                case 3:
                    break;
                case 4:
                    bufsize = aval;
                    break;
                case 5:
                    minval = aval;
                    break;
                case 6:
                    maxval = aval;
                    break;
                case 7:
                    delay = aval;
                    break;
                case 8:
                    break;
                case 9:
                    trigger = aval;
                    break;
                case 10:
                    triglevel = aval;
                    break;
                case 11:
                    fgred = aval;
                    break;
                case 12:
                    fggreen = aval;
                    break;
                case 13:
                    fgblue = aval;
                    break;
                case 14:
                    bgred = aval;
                    break;
                case 15:
                    bggreen = aval;
                    break;
                case 16:
                    bgblue = aval;
                    break;
                case 17:
                    grred = aval;
                    break;
                case 18:
                    grgreen = aval;
                    break;
                case 19:
                    grblue = aval;
                    break;
                default:
                    break;
            };
            argnum++, ac--, av++;
        }
        else if(av->a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbolarg(0, ac, av);
            if(strcmp(curarg->s_name, "@calccount") == 0){
                if(ac >= 2){
                    period = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@bufsize") == 0){
                if(ac >= 2){
                    bufsize = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@range") == 0){
                if(ac >= 3){
                    minval = atom_getfloatarg(1, ac, av);
                    maxval = atom_getfloatarg(2, ac, av);
                    ac-=3, av+=3;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@delay") == 0){
                if(ac >= 2){
                    delay = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@drawstyle") == 0){
                if(ac >= 2){
                    drawstyle = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@trigger") == 0){
                if(ac >= 2){
                    trigger = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@triglevel") == 0){
                if(ac >= 2){
                    triglevel = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@frgb") == 0){
                if(ac >= 4){
                    fgred = atom_getfloatarg(1, ac, av);
                    fggreen = atom_getfloatarg(2, ac, av);
                    fgblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@brgb") == 0){
                if(ac >= 4){
                    bgred = atom_getfloatarg(1, ac, av);
                    bggreen = atom_getfloatarg(2, ac, av);
                    bgblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@grgb") == 0){
                if(ac >= 4){
                    grred = atom_getfloatarg(1, ac, av);
                    grgreen = atom_getfloatarg(2, ac, av);
                    grblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@fgcolor") == 0){
                if(ac >= 4){
                    fcolset = 1;
                    fcred = atom_getfloatarg(1, ac, av);
                    fcgreen = atom_getfloatarg(2, ac, av);
                    fcblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@bgcolor") == 0){
                if(ac >= 4){
                    bcolset = 1;
                    bcred = atom_getfloatarg(1, ac, av);
                    bcgreen = atom_getfloatarg(2, ac, av);
                    bcblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(strcmp(curarg->s_name, "@gridcolor") == 0){
                if(ac >= 4){
                    gcolset = 1;
                    gcred = atom_getfloatarg(1, ac, av);
                    gcgreen = atom_getfloatarg(2, ac, av);
                    gcblue = atom_getfloatarg(3, ac, av);
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    }
    x->x_rightinlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    width *= x->x_zoom;
    height *= x->x_zoom;
    scope_dim(x, width, height);
    scope_period(x, period);
    scope_bufsize(x, bufsize);
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    scope_range(x, minval, maxval);
    scope_delay(x, delay);
    scope_drawstyle(x, drawstyle);
    scope_trigger(x, trigger);
    scope_triglevel(x, triglevel);
    fcolset ? scope_fgcolor(x, fcred, fcgreen, fcblue) : scope_frgb(x, fgred, fggreen, fgblue);
    bcolset ? scope_bgcolor(x, bcred, bcgreen, bcblue) : scope_brgb(x, bgred, bggreen, bgblue);
    gcolset ? scope_gridcolor(x, gcred, gcgreen, gcblue) : scope_grgb(x, grred, grgreen, grblue);
    sprintf(x->x_tag, "all%lx", (unsigned long)x);
    sprintf(x->x_bgtag, "bg%lx", (unsigned long)x);
    sprintf(x->x_gridtag, "gr%lx", (unsigned long)x);
    sprintf(x->x_fgtag, "fg%lx", (unsigned long)x);
    sprintf(x->x_margintag, "ma%lx", (unsigned long)x);
    x->x_clock = clock_new(x, (t_method)scope_tick);
    return(x);
errstate:
    pd_error(x, "scope~: improper creation arguments");
    return NULL;
}

CYCLONE_OBJ_API void scope_tilde_setup(void){
    scope_class = class_new(gensym("scope~"), (t_newmethod)scope_new,
            (t_method)scope_free, sizeof(t_scope), 0, A_GIMME, 0);
//    class_addcreator((t_newmethod)scope_new, gensym("Scope~"), A_GIMME, 0); // backwards compatible
//    class_addcreator((t_newmethod)scope_new, gensym("cyclone/Scope~"), A_GIMME, 0); // backwards compatible
    class_addmethod(scope_class, nullfn, gensym("signal"), 0);
    class_addmethod(scope_class, (t_method) scope_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(scope_class, (t_method)scope_period);
    class_addmethod(scope_class, (t_method)scope_period, gensym("calccount"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_bufsize, gensym("bufsize"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_resize, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_delay, gensym("delay"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_drawstyle, gensym("drawstyle"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_trigger, gensym("trigger"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_triglevel, gensym("triglevel"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_frgb, gensym("frgb"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_brgb, gensym("brgb"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_grgb, gensym("grgb"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_fgcolor, gensym("fgcolor"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_bgcolor, gensym("bgcolor"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_gridcolor, gensym("gridcolor"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_dialog, gensym("dialog"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_click, gensym("click"), A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_motion, gensym("motion"), 0);
    class_addmethod(scope_class, (t_method)scope_resize, gensym("resize"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_zoom, gensym("zoom"), A_CANT, 0);
    scopehandle_class = class_new(gensym("_scopehandle"), 0, 0, sizeof(t_scopehandle), CLASS_PD, 0);
    class_addmethod(scopehandle_class, (t_method)scopehandle__clickhook, gensym("_click"), A_FLOAT, 0);
    class_addmethod(scopehandle_class, (t_method)scopehandle__motionhook, gensym("_motion"), A_FLOAT, A_FLOAT, 0);
//    class_sethelpsymbol(scope_class, gensym("scope~"));
    class_setsavefn(scope_class, scope_save);
    class_setpropertiesfn(scope_class, scope_properties);
    class_setwidget(scope_class, &scope_widgetbehavior);
    scope_widgetbehavior.w_getrectfn  = scope_getrect;
    scope_widgetbehavior.w_displacefn = scope_displace;
    scope_widgetbehavior.w_selectfn   = scope_select;
    scope_widgetbehavior.w_deletefn   = scope_delete;
    scope_widgetbehavior.w_visfn      = scope_vis;
    scope_widgetbehavior.w_clickfn    = (t_clickfn)scope_click;
    #include "scope_dialog.c"
}
