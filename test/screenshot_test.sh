#!/bin/bash

#set -x

#
# Usage : $ ./screenshot_test.sh 2>/dev/null
#

# Memo
# X11:GetImage, MIT-SHM:CreatePixmap, X11:CreatePixmap
# X11:CopyArea, X11:CreateGC, X11:CreateWindow, 
# RENDER:CreatePicture


# X11 XGetImage
echo "[-] XGetImage Program Test ..."
file_name="test.jpg"
./etc/screenshot-xgetimage $file_name &>/dev/null
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "JPEG image data")" ];
	then
		echo "[*] XGetImage Success ..."
		rm $file_name
	else
		echo "[*] XGetImage Fail :)"
	fi
else
	echo "[*] XGetImage Fail :)"
fi


# gtk2, gtk3
echo "[-] GTK2 Program Test ..."
./etc/screenshot-gtk2 &>/dev/null
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] GTK2-gdk_pixbuf_get_from_drawable Success ..."
			rm $file_name
		else
			echo "[*] GTK2-gdk_pixbuf_get_from_drawable Fail :)"
		fi
else
	echo "[*] GTK2-gdk_pixbuf_get_from_drawable Fail :)"
fi

echo "[-] GTK3 Program Test ..."
./etc/screenshot-gtk3 &> /dev/null
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] GTK3-gdk_pixbuf_get_from_window Success ..."
			rm $file_name
		else
			echo "[*] GTK3-gdk_pixbuf_get_from_window Fail :)"
		fi
else
	echo "[*] GTK3-gdk_pixbuf_get_from_window Fail :)"
fi


<<'COMMENT' 
# shutter is not included in Debian 10
# shutter gdk_pixbuf_get_from_drawable
if [ -z $(which shutter) ];
then
	sudo apt install shutter -y
fi

echo "[*] shutter Test-1 ..."
shutter -f -e -o ./test.png
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] shutter Test-1 (gtk2-gdk_pixbuf_get_from_drawable) Success ..."
			rm $file_name
		else
			echo "[*] shutter Test-1 (gtk2-gdk_pixbuf_get_from_drawable) Fail :)"
		fi
else
	echo "[*] shutter Test-1 (gtk2-gdk_pixbuf_get_from_drawable) Fail :)"
fi


echo "[*] shutter Test-2 ..."
shutter -s=100,100,300,300 -e -o ./test.png
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] shutter Test-2 (gtk2-gdk_pixbuf_get_from_drawable) Success ..."
			rm $file_name
		else
			echo "[*] shutter Test-2 (gtk2-gdk_pixbuf_get_from_drawable) Fail :)"
		fi
else
	echo "[*] shutter Test-2 (gtk2-gdk_pixbuf_get_from_drawable) Fail :)"
fi
COMMENT

# imagemagic
echo "[-] imagemagic Test ..."
import -window root test.png &> /dev/null
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] imagemagic Test (XGetImage) Success ..."
			rm $file_name
		else
			echo "[*] imagemagic Test (XGetImage) Fail :)"
		fi
else
	echo "[*] imagemagic Test (XGetImage) Fail :)"
fi


# gnome-screenshooter
echo "[-] gnome-screenshot Test ..."
gnome-screenshot -f ./test.png &> /dev/null
file_name="test.png"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
		then
			echo "[*] gnome-screenshot Test (gtk3-gdk_pixbuf_get_from_surface) Success ..."
			rm $file_name
		else
			echo "[*] gnome-screenshot Test (gtk3-gdk_pixbuf_get_from_surface) Fail :)"
		fi
else
	echo "[*] gnome-screenshot Test (gtk3-gdk_pixbuf_get_from_surface) Fail :)"
fi

# xfce4-screenshooter
echo "[-] xfce4-screenshooter Test ..."
xfce4-screenshooter -w -s ./ &
sleep 3
if [ -n "$(ps aux | grep "xfce4-screenshooter" | grep -v "grep")" ];
then
	echo "[*] xfce4-screenshooter Test Success ..."
	kill -9 $(ps aux | grep "xfce4-screenshooter" | grep -v "grep" | awk '{print $2}')
else
	echo "[*] xfce4-screenshooter Test Fail :)"
fi


# kazam
if [ -z "$(which kazam)" ];
then
	sudo apt install kazam -y
fi

echo "[-] kazam Test ..."
kazam -f & &>/dev/null
sleep 3

if [ -n "$(ps aux | grep "/usr/bin/kazam" | grep -v "grep")" ];
then
	echo "[*] kazam Test Success (python script) ..."
	pkill -9 kazam 
else
	echo "[*] kazam Test Fail (python script) :)"
fi


# scrot
if [ -z $(which scrot) ];
then
	sudo apt install scrot -y
fi

echo "[-] scrot Test ..."
file_name="test.png"
scrot ./$file_name
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "PNG image data")" ];
	then
		if [ "$(stat --printf="%s" ./$file_name)" != "27275" ];
		then
			echo "[*] scrot Test (giblib-gib_imlib_create_image_from_drawable) Success ..."
		else
			echo "[*] scrot Test (giblib-gib_imlib_create_image_from_drawable) Fail :)"
		fi
		#rm $file_name
	else
		echo "[*] scrot Test (giblib-gib_imlib_create_image_from_drawable) Fail :)"
	fi
else
	echo "[*] scrot Test (giblib-gib_imlib_create_image_from_drawable) Fail :)"
fi

