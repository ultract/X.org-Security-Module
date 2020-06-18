#!/bin/bash

#set -x

# Xfce
if [ "$XDG_CURRENT_DESKTOP" == "XFCE" ];
then
	gcc `pkg-config xorg-server --cflags --libs dbus-1,libnotify,json-c` -DXACE -D_XSERVER64 -DX_REGISTRY_REQUEST -DX_REGISTRY_RESOURCE -DCOMPOSITE xsm.c -o xsm.so -shared -ldl -fPIC -s
fi

# GNOME
if [ "$XDG_CURRENT_DESKTOP" == "GNOME" ];
then
	gcc `pkg-config xorg-server --cflags --libs dbus-1,libnotify,json-c` -DXACE -D_XSERVER64 -DX_REGISTRY_REQUEST -DX_REGISTRY_RESOURCE -DCOMPOSITE xsm.c -o xsm.so -shared -ldl -fPIC -s
fi

