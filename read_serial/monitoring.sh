#!/bin/bash

serial9600() {
	/usr/bin/python3 /home/pi/dscript/serial9600_2.py
}
until serial9600; do
   	echo "$(date): 'serial9600.py' crashed with exit code $?. Restarting... \n" >> '/home/pi/dscript/restart.log'
	sleep 5
done
