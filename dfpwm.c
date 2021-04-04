/* Copyright 2021, Ayush Bardhan Tripathy
 * This software is licensed under BSD 2-Clause License
 * See LICENSE.md for detailed info.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define CLIENT_MAX 256
#define MODKEY (Mod1Mask | ControlMask)
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

/////////////////////////////////////////////////
// Structs
////////////////////////////////////////////////

typedef struct Vec2 {
    int x, y;
} Vec2;

typedef struct Drag {
    Vec2 startpos;
    Vec2 startframepos;
    Vec2 startframesize;
} Drag;

typedef struct Client Client;
typedef struct Client {
    Window window;
    Window frame;
    Client *next;
} Client;

typedef union Arg {
    int i;
    bool b;
    char *s;
    char **v;
} Arg;

typedef struct KeyEvent {
    unsigned int modifier;
    unsigned int key;
    void (*func)(Arg *a);
    Arg arg;
} KeyEvent;

typedef struct ButtonEvent {
    unsigned int modifier;
    unsigned int button;
} ButtonEvent;

typedef struct Config {
    unsigned int frame_border_width;
    unsigned long frame_inactive_color;
} Config;

void killclient(Arg *a);
void spawn(Arg *a);

#include "config.h"

/////////////////////////////////////////////////
// Info
////////////////////////////////////////////////

static struct {
    Display *dsp;
    Window root;
    Window focused;

    Client clients[CLIENT_MAX];
    Client *client_top;
    Drag drag;
} info;

/////////////////////////////////////////////////
// Client
////////////////////////////////////////////////

const Window findclient(Window window) {
    for (Client *c = info.clients; c < info.client_top; c += 1) {
        if (c->window == window)
            return c->frame;
    }
    return -1;
}

void removeclient(Window window) {
    for (Client *c = info.clients; c < info.client_top; c += 1) {
        if (c->window == window) {
            c->window = -1;
            c->frame = -1;
        }
    }
}

/////////////////////////////////////////////////
// X Error handlers
////////////////////////////////////////////////

int xerror(Display *dsp, XErrorEvent *err) {
    char *buf = malloc(256);
    XGetErrorText(info.dsp, err->error_code, buf, 256);
    fprintf(stderr, "dfpwm: %s\n\trequest code: %d, error code: %d, resource id: %ld\n",
        buf, err->request_code, err->error_code, err->resourceid);
    free(buf);
    return 0;
}

int xerrorwm(Display *dsp, XErrorEvent *err) {
    fprintf(stderr, "dfpwm: another window manager is already running\n");
    return -1;
}

/////////////////////////////////////////////////
// Frame
////////////////////////////////////////////////

void framewindow(Window win) {
    if (findclient(win) != -1)
        return;
    
    Window root = XDefaultRootWindow(info.dsp);
    XWindowAttributes attr;
    XGetWindowAttributes(info.dsp, win, &attr);

    const Window frame = XCreateSimpleWindow(info.dsp, root, 
            attr.x, attr.y, attr.width, attr.height, 
            config.frame_border_width, 
            config.frame_inactive_color, 0xffffff);

    XSelectInput(info.dsp, frame, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(info.dsp, frame);
    XReparentWindow(info.dsp, win, frame, 0, 0);
    XMapWindow(info.dsp, frame);

    Client client;
    client.window = win;
    client.frame = frame;
    
    *info.client_top = client;
    info.client_top += 1;

    for (int i = 0; i < LENGTH(buttons); i += 1) {
        XGrabButton(info.dsp, buttons[i].button, buttons[i].modifier, win, 
            False, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
            GrabModeAsync, GrabModeAsync, None, None);
    }

    for (int i = 0; i < LENGTH(keys); i += 1) {
        XGrabKey(info.dsp, XKeysymToKeycode(info.dsp, keys[i].key), keys[i].modifier, win,
            False, GrabModeAsync, GrabModeAsync);
    }
}

void unframewindow(Window win) {
    const Window frame = findclient(win);
    XUnmapWindow(info.dsp, frame);
    XReparentWindow(info.dsp, win, info.root, 0, 0);
    XRemoveFromSaveSet(info.dsp, win);
    XDestroyWindow(info.dsp, frame);
    removeclient(win);
}

void focus(Window window) {
    info.focused = window;
    XRaiseWindow(info.dsp, info.focused);
}

/////////////////////////////////////////////////
// Events
////////////////////////////////////////////////

void event_buttonpress(XEvent *event) {
    XButtonEvent *e = (XButtonEvent *)&event->xbutton;

    const Window frame = findclient(e->window);  
    focus(e->window);

    Window root;
    int x, y;
    unsigned int width, height, border_width, depth;

    XGetGeometry(info.dsp, frame, &root, &x, &y, &width, &height, &border_width, &depth);

    info.drag.startpos.x = e->x_root;
    info.drag.startpos.y = e->y_root;
    info.drag.startframepos.x = x;
    info.drag.startframepos.y = y;
    info.drag.startframesize.x = width;
    info.drag.startframesize.y = height;
}

void event_configrequest(XEvent *event) {
    XConfigureRequestEvent *e = (XConfigureRequestEvent *)&event->xconfigurerequest;
    
    XWindowChanges changes;    
    changes.x = e->x;
    changes.y = e->y;
    changes.width = e->width;
    changes.height = e->height;
    changes.border_width = e->border_width;
    changes.sibling = e->above;
    changes.stack_mode = e->detail;

    const Window frame = findclient(e->window);
    if (frame != -1) {
        XConfigureWindow(info.dsp, frame, e->value_mask, &changes);
    }

    XConfigureWindow(info.dsp, e->window, e->value_mask, &changes);
    XSync(info.dsp, False);
}

void event_enternotify(XEvent *event) {
    /* blank for now */
}

void event_focusin(XEvent *event) {
    XFocusInEvent *e = (XFocusInEvent *)&event->xfocus;

    focus(e->window);
}

void event_keypress(XEvent *event) {
    XKeyEvent *e = (XKeyEvent *)&event->xkey;

    for (int i = 0; i < LENGTH(keys); i += 1) {
        if ((e->state & keys[i].modifier) && (e->keycode == XKeysymToKeycode(info.dsp, keys[i].key))) {
            if ((keys[i].func))
                keys[i].func(&keys[i].arg);
        }
    }
}

void event_maprequest(XEvent *event) {
    XMapRequestEvent *e = (XMapRequestEvent *)&event->xmaprequest;

    framewindow(e->window);
    XMapWindow(info.dsp, e->window);
}

void event_motionnotify(XEvent *event) {
    while (XCheckTypedWindowEvent(info.dsp, event->xmotion.window, MotionNotify, event));

    XMotionEvent *e = (XMotionEvent *)&event->xmotion;

    if (&info.clients[0] == NULL)
        return;
    const Window frame = findclient(e->window);

    Vec2 delta = { e->x_root - info.drag.startpos.x, e->y_root - info.drag.startpos.y };

    Vec2 position = {
        info.drag.startframepos.x + ((e->state & Button1Mask) ? delta.x : 0),
        info.drag.startframepos.y + ((e->state & Button1Mask) ? delta.y : 0),
    };

    Vec2 size = {
        MAX(1, info.drag.startframesize.x + ((e->state & Button3Mask) ? delta.x : 0)),
        MAX(1, info.drag.startframesize.y + ((e->state & Button3Mask) ? delta.y : 0))
    };


    XMoveResizeWindow(info.dsp, frame, position.x, position.y, size.x, size.y);
    XResizeWindow(info.dsp, e->window, size.x, size.y);
}

void event_unmapnotify(XEvent *event) {
    XUnmapEvent *e = (XUnmapEvent *)&event->xunmap;
    
    if (&info.clients[0] == NULL)
        return;
    if (e->event == info.root)
        return;
    unframewindow(e->window);
}

/////////////////////////////////////////////////
// Actions
////////////////////////////////////////////////

void killclient(Arg *a) {
    XKillClient(info.dsp, info.focused);
}

void spawn(Arg *a) { 
    if (fork() == 0) {
        if (execvp(a->v[0], (char**)a->v) == -1) {
            fprintf(stderr, "dfpwm: cannot spawn process");
            exit(EXIT_FAILURE);
        }
    }
}

/////////////////////////////////////////////////
// Main
////////////////////////////////////////////////

static void (*evhandler[]) (XEvent *) = {
    [ButtonPress] = event_buttonpress,
    [ConfigureRequest] = event_configrequest,
    [EnterNotify] = event_enternotify,
    [FocusIn] = event_focusin,
    [KeyPress] = event_keypress,
    [MapRequest] = event_maprequest,
    [MotionNotify] = event_motionnotify,
    [UnmapNotify] = event_unmapnotify,
};

void checkwm(void) {
    XSetErrorHandler(xerrorwm);
    XSelectInput(info.dsp, info.root, SubstructureRedirectMask);
    XSync(info.dsp, False);
    XSetErrorHandler(xerror);
    XSync(info.dsp, False);
}

void run(void) { 
    int running = 1;
    XEvent event;
    XSync(info.dsp, False);
    while (running && !XNextEvent(info.dsp, &event)) {
        if (evhandler[event.type])
            evhandler[event.type](&event);
    }
}

void init(void) {
    info.client_top = info.clients;
}

int main(int argc, char **argv) {
    init();

    info.dsp = XOpenDisplay(0x0);
    if (!info.dsp) {
        fprintf(stderr, "dfpwm: cannot open display\n");
        return EXIT_FAILURE;
    }

    info.root = XDefaultRootWindow(info.dsp);

    Arg autostart = { .v = (char *[]){"/bin/sh", "-c", "./test/autostart.sh", NULL } };
    spawn(&autostart);

    checkwm();
    run();

    XCloseDisplay(info.dsp);
}
