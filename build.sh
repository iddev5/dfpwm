#!/bin/sh
set -e

install() {
    echo "build.sh: unimplemented"
}

run() {
    xinit ./test/xinitrc -- "$(which Xephyr)" :100 -ac -screen 1280x960 -host-cursor
}

build() {
    gcc -Wall -O2 -o dfpwm dfpwm.c $(pkg-config --cflags --libs x11)
}

if [ "$1" = "install" ]
then install

elif [ "$1" = "run" ]
then run

else build
fi
