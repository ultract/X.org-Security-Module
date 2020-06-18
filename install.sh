#!/bin/bash

# Xorg Module
sudo cp ./xsm.so /usr/lib/xorg/modules/extensions/
sudo cp ./99-xsm.conf /usr/share/X11/xorg.conf.d/

# XSM Policy
xsm_policy_dir="/etc/xsm/"
if [ ! -d "$xsm_policy_dir" ];
then
	sudo mkdir -p "$xsm_policy_dir"
fi
sudo cp ./default.rules "$xsm_policy_dir"

# DBus Policy for GNOME Desktop Envirionment
if [ "$(echo "$XDG_CURRENT_DESKTOP" | grep -c "GNOME")" -ge 1 ];
then
	sudo cp ./dbus-policy/xsm-policy.conf /etc/dbus-1/session.d/xsm-policy.conf
fi
