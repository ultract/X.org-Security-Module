#!/bin/bash

#
# xclip, /usr/lib/ibus/ibus-ui-gtk3
# 
# X11:SetSelectionOwner, X11:ConvertSelection
#

# Do the Clipboard Action (Ctrl+c, Ctrl+v)
# Copy
if [ -z "$(which xclip)" ];
then
	sudo apt install xclip -y
fi

#
# xclip 
#
echo "[-] xclip Test ..."
# copy
echo "Hello World!" | xclip -selection clipboard
# Past
file_name="test.txt"
xclip -out -selection c > ./$file_name
if [ "$(stat --printf="%s" ./$file_name)" == "0" ];
then
	echo "[*] xclip Test failed :)"
else
	echo "[*] xclip Test success ..."
fi
rm ./$file_name
