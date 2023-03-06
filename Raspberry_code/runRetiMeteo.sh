#!/bin/bash

retimeteo() {
        /usr/bin/python3 /home/pi/dscript/reti_meteo.py
}
until retimeteo; do
        echo "$(date): 'reti_meteo.py' crashed with exit code $?. Restarting... \n" >> '/home/pi/dscript/restart.log'
        sleep 5
done
