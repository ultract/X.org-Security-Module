# X.org Security Module

## Intro
- Preventing clipboard, screenshot, screencast and X session record/replay on X.org
- Demo video (https://youtu.be/G5orm5gmMuc)

## Test environment
- Debian 9 (Stretch), Debian 10 (Buster)
- Cinnmon, Gnome, KDE, LXQt, LXDE, Xfce4
- X.org 1.20.4

## Prerequisite
    $ sudo apt install build-essential pkg-config -y
    $ sudo apt install xserver-xorg-dev libjson-c-dev libnotify-dev -y
    $ sudo apt install libdbus-glib-1-dev -y
    $ sudo apt install libsystemd-dev -y

## Build and install
    $ ./build.sh
	$ ./install.sh # then reboot :)

## Notice
### XSM Policy
	$ sudo vi /etc/xsm/default.rules

### XSM Logs
    $ sudo journalctl -f | grep "XSM-LOG"

## Etc
### X.org debug message
	# Add the options to /etc/lightdm/lightdm.conf below
    xserver-command=Xorg -logverbose 6 or
    xserver-command=Xorg -audit 4 -logverbose 11

### X protocol by xtrace
    $ xtrace --print-offset [xclient program name]

## Acknowledgments
This tool has been developed for access control of the Gooroom platform which is an open source project. This work was supported by Institute for Information & communications Technology Promotion (IITP) grant funded by the Korea government (MSIP) (No.R0236-15-1006, Open Source Software Promotion).
