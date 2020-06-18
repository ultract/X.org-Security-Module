#!/bin/bash

#
# MIT-SHM:GetImage
#

echo "[-] Recordmydesktop Test ..."

if [ -z "$(which recordmydesktop)" ];
then
	sudo apt install recordmydesktop -y
fi

/usr/bin/recordmydesktop &
sleep 1

#pkill -2 recordmydesktop
kill -9 $!
sleep 2
file_name="out.ogv"
if [ -e "./$file_name" ];
then
	if [ -n "$(/usr/bin/file ./$file_name | grep "Ogg data, Skeleton")" ];
	then
		echo "[*] Recordmydesktop Success ..."
		#rm -rf "./$file_name"
	else
		echo "[*] Recordmydesktop Fail :)"
	fi

else
		echo "[*] Recordmydesktop Fail :)"
fi


echo "[-] kazam Test ..."
/usr/bin/kazam

