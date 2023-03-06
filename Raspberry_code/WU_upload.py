import requests
import time
import mysql.connector
from datetime import datetime, timezone

mydb = mysql.connector.connect(
  host="localhost",
  user="meteo",
  password="federico3000",
  database="meteo"
)
mycursor = mydb.cursor()

timer = time.time()
lastTime = 0

def CtoF (celsius):
    return (celsius*9./5.)+32

WUurl = "https://weatherstation.wunderground.com/weatherstation\
/updateweatherstation.php?"
WU_station_id = "ICREMO15" # Replace XXXX with your PWS ID
WU_station_pwd = "0xOmtYC1" # Replace YYYY with your Password
WUcreds = "ID=" + WU_station_id + "&PASSWORD="+ WU_station_pwd
#date_str = "&dateutc=now"
action_str = "&action=updateraw"

def uploadWU():
    link = WUurl + WUcreds + action_str
    link+= "&dateutc=" + str(datetime.fromtimestamp(currentTime, tz=timezone.utc).strftime('%Y-%m-%d+%H:%M:%S')).replace(":", "%3A")
    link+= "&winddir=" + "{0:.0f}".format(row[0]) #ist 0-360
    link+= "&windspeedmph=" + "{0:.2f}".format(row[1]*2.237) #ist
    link+= "&windgustmph=" + "{0:.2f}".format(raffica[0]*2.237)#raffica nel periodo stazione
    link+= "&humidity=" + "{0:.2f}".format(row[2]) #0-100
    link+= "&dewptf=" + "{0:.2f}".format(CtoF(row[3])) #[F]
    link+= "&tempf=" + "{0:.2f}".format(CtoF(row[4])) #[F]
    link+= "&dailyrainin=" + "{0:.1f}".format(row[5]*0.0393701) #[inch]
    link+= "&baromin=" + "{0:.2f}".format(row[6]*0.00029529980164712) #[inHg]
    if rain1hour[0] != None:
        link += "&rainin=" + "{0:.2f}".format(rain1hour[0]*0.0393701) #if rain1hour inch not None
    r= requests.get(link)
    if r.status_code != 200:
        print("At " + str(datetime.fromtimestamp(currentTime).strftime('%Y-%m-%d+%H:%M:%S')) + " Received " + str(r.status_code) + " " + str(r.text))

while True:
    if (time.time() - timer) > 60:
        timer = time.time()

        mycursor.execute("select MAX(mmPioggia)-MIN(mmPioggia) from dataday where timestamp(date,time) > now() - interval 1 hour")
        rain1hour = mycursor.fetchone()
        mycursor.execute("SELECT windAng, wind, ur, dewpoint, tp, mmPioggia, pressionelivellodelmare, UNIX_TIMESTAMP(TIMESTAMP(date,time)) from dataday order by timestamp(date,time) DESC LIMIT 1")
        row = mycursor.fetchone()
        currentTime = row[7]
        mycursor.execute("SELECT raffica from estremi order by timestamp(date,time) DESC LIMIT 1")
        raffica = mycursor.fetchone()
        mydb.commit()

        if currentTime != lastTime:
            uploadWU()
            lastTime = currentTime
        else:
            print("Same data")
mycursor.close()
mydb.close()

