#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define CLIENT_MAX 256
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define LENGTH(x) (sizeof(x) / sizeof(x[0]))

/////////////////////////////////////////////////
// Config
////////////////////////////////////////////////

static struct {
    struct {
        unsigned int modkey;
        unsigned int frame_border_width;
    } set;
    
    struct {
        unsigned long bg_frame;
        unsigned long fg_frame;
    } color;
} config;

#define MODKEY config.set.modkey

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

typedef struct Key {
    unsigned int modifier;
    unsigned int key;
} Key;

#include "config.h"

/////////////////////////////////////////////////
// Info
////////////////////////////////////////////////

static struct {
    Display *dsp;
    Window root;
    
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
    fprintf(stderr, "dfpwm: request code: %d, error code: %d, resource id: %ld :: %s\n",
        err->request_code, err->error_code, err->resourceid, buf);
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
            config.set.frame_border_width, 
            config.color.fg_frame, config.color.bg_frame);

    XSelectInput(info.dsp, frame, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(info.dsp, frame);
    XReparentWindow(info.dsp, win, frame, 0, 0);
    XMapWindow(info.dsp, frame);

    Client client;
    client.window = win;
    client.frame = frame;
    
    *info.client_top = client;
    info.client_top += 1;

    XGrabButton(info.dsp, Button1, MODKEY, win, False, 
        ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(info.dsp, Button3, MODKEY, win, False, 
        ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    
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

/////////////////////////////////////////////////
// Events
////////////////////////////////////////////////

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

void event_buttonpress(XEvent *event) {
    XButtonEvent *e = (XButtonEvent *)&event->xbutton;

    const Window frame = findclient(e->window);  
    XRaiseWindow(info.dsp, frame);

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

void event_keypress(XEvent *event) {
    XKeyEvent *e = (XKeyEvent *)&event->xkey;

    if ((e->state & MODKEY) && e->keycode == XKeysymToKeycode(info.dsp, XK_c)) {
        XKillClient(info.dsp, e->window);
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

    Vec2 dragpos = { e->x_root, e->y_root };
    Vec2 delta = { dragpos.x - info.drag.startpos.x, dragpos.y - info.drag.startpos.y };

    if (e->state & Button1Mask) {
        Vec2 position = { info.drag.startframepos.x + delta.x, info.drag.startframepos.y + delta.y };
        XMoveWindow(info.dsp, frame, position.x, position.y);
    }
    else if (e->state & Button3Mask) {
        Vec2 size_delta = { 
            MAX(delta.x, -info.drag.startframesize.x),
            MAX(delta.y, -info.drag.startframesize.y) 
        };
        Vec2 size = {
            info.drag.startframesize.x + size_delta.x,
            info.drag.startframesize.y + size_delta.y,
        };

        XResizeWindow(info.dsp, frame, size.x, size.y);
        XResizeWindow(info.dsp, e->window, size.x, size.y);
    }
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
// Main
////////////////////////////////////////////////

static void (*evhandler[]) (XEvent *) = {
    [ButtonPress] = event_buttonpress,
    [ConfigureRequest] = event_configrequest,
    [KeyPress] = event_keypress,
    [MapRequest] = event_maprequest,
    [MotionNotify] = event_motionnotify,
    [UnmapNotify] = event_unmapnotify,
};

void checkwm() {
    XSetErrorHandler(xerrorwm);
    XSelectInput(info.dsp, info.root, SubstructureRedirectMask);
    XSync(info.dsp, False);
    XSetErrorHandler(xerror);
    XSync(info.dsp, False);
}

void spawn(const char *argv[]) {
    if (fork() == 0) {
        if (execvp(argv[0], (char**)argv) == -1) {
            fprintf(stderr, "dfpwm: cannot spawn process");
            exit(EXIT_FAILURE);
        }
    }
}

void run() { 
    int running = 1;
    XEvent event;
    XSync(info.dsp, False);
    while (running && !XNextEvent(info.dsp, &event)) {
        if (evhandler[event.type])
            evhandler[event.type](&event);
    }
}

void init() {
    info.client_top = info.clients;
}

int main() {
    init();

    info.dsp = XOpenDisplay(0x0);
    if (!info.dsp) {
        fprintf(stderr, "dfpwm: cannot open display\n");
        return EXIT_FAILURE;
    }

    info.root = XDefaultRootWindow(info.dsp);

    config.set.modkey = Mod1Mask | ControlMask;
    config.set.frame_border_width = 1;
    config.color.fg_frame = 0x00ff04;
    config.color.bg_frame = 0xffffff;

    const char *autostart[] = { "/bin/sh", "-c", "./test/autostart.sh", NULL };
    spawn(autostart);

    checkwm();
    run();

    XCloseDisplay(info.dsp);
}
