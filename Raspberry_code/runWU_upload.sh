#!/bin/bash

wunderground() {
        /usr/bin/python3 /home/pi/dscript/WU_upload.py
}
until wunderground; do
        echo "$(date): 'WU_upload.py' crashed with exit code $?. Restarting... \n" >> '/home/pi/dscript/restart.log'
        sleep 5
done