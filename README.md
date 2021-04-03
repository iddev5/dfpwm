# Deep Fried Pakoda Window Manager (dfpwm)
dfpwm is a simple, learning window manager for X11 using Xlib.  
It is currently is in very early stages and only support floating layout, but in future, more features would be worked on (hopefully!)

## Build
Dependencies: gcc, sh, pkg-config, xlib (libx11)
```sh
./build.sh
```
Check dfpwm.c for some customization options.

## Usage/Test
Dependencies: Xephyr
```sh
./build.sh run
```
You can customize the contents inside ``test`` folder: autostart.sh and xinitrc.  
(It is not recommended to use/test it directly as a WM as it lacks a lot of features, like being able to quit itself.)

## References
[1] https://dwm.suckless.org/  
[2] https://jichu4n.com/posts/how-x-window-managers-work-and-how-to-write-one-part-i/  
[3] http://www.6809.org.uk/evilwm/  
[4] https://tronche.com/gui/x/xlib/ 

## License
Copyright 2021, Ayush Bardhan Tripathy  
This software is licensed under BSD 2-Clause License  
See [LICENSE.md](LICENSE.md) for more info.
