#!/bin/bash

meteoblue() {
        /usr/bin/python3 /home/pi/dscript/getmeteoblue.py
}
until meteoblue; do
        echo "$(date): 'getmeteoblue.py' crashed with exit code $?. Restarting... \n" >> '/home/pi/dscript/restart.log'
        sleep 5
done
