#!/bin/bash

# Record
echo "[-] Xnee(cnee) Xsession Record Test ..."

if [ -z "$(which cnee)" ];
then
	sudo apt install cnee -y
fi

file_name="event1.xnr"
cnee --record --keyboard --mouse --time 3 -o ./$file_name
if [ -e "./$file_name" ];
then
	if [ "$(stat --printf="%s" ./$file_name)" != 4284 ];
	then
		echo "[*] Xnee(cneE) Xsession Record Success ..."
	else
		echo "[*] Xnee(cneE) Xsession Record Fail :)"
	fi
else
		echo "[*] Xnee(cneE) Xsession Record Fail :)"
fi

# Replay
cnee --replay --no-synchronise --force-core-replay -f ./$file_name
