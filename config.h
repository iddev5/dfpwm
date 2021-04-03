
static Config config = {
    .frame_border_width = 2,
    .frame_inactive_color = 0x00ff97,
};

#define MOD (Mod1Mask | ControlMask)

char *termcmd[] = { "st", NULL }; 

static KeyEvent keys[] = {
    { MOD, XK_c, killclient, {0} },
    { MOD, XK_s, spawn, { .v = termcmd } },
};

static ButtonEvent buttons[] = {
    { MOD, Button1 },
    { MOD, Button3 },
};
