import time
import mysql.connector
from datetime import datetime

mydb = mysql.connector.connect(
  host="localhost",
  user="user",
  password="",
  database="db"
)
mycursor = mydb.cursor()

lineameteo = open("/home/pi/www/meteo.txt", "w")
meteonetwork = open("/home/pi/www/pmn175.txt", "a")

daysaved = int(datetime.today().strftime('%d'))
starttime = time.time()

while True:
	#time date
	todayMYSQL = datetime.today().strftime('%Y-%m-%d')
	nowtime = datetime.today().strftime('%H:%M:%S')

	#lineameteo
	mycursor.execute(f"select tp,ur,pressionelivellodelmare/100, windMedio*3.6, windDirMedio, mmPioggia, min(tp), max(tp), max(windMax)*3.6, rainrate from dataday where date = '{todayMYSQL}'")
	linM = mycursor.fetchone()

	if linM[0]:
		lineameteo = open("/home/pi/www/meteo.txt", "w")
		today2 = datetime.today().strftime('%d/%m/%Y')
		lineameteo.write(today2 + " " + nowtime)
		lineameteo.write("&v0=44.6327777777775&v0=8.6&v0=Cremolino(AL)&v0=312m s.l.m.&v0=")
		str1 = str(linM[0]) + "&v0=" + str(linM[1]) + "&v0=" + str(linM[2]) + "&v0=" + str(linM[3]) + "&v0=" + linM[4] + "&v0=" + str(linM[5]) + "&v0=" + str(linM[6]) + "&v0=" + str(linM[7]) + "&v0=" + str(linM[8]) + "&v0=" + str(linM[9])
		lineameteo.write(str1)
		lineameteo.close()

	#meteonetwork
	mycursor.execute(f"select tp,pressionelivellodelmare/100, ur, windMedio*3.6, windAngMedio, max(windMax)*3.6, rainrate, mmPioggia, dewpoint from dataday where date = '{todayMYSQL}'")
	MN = mycursor.fetchone()

	if MN[0]:
		if (time.time() - starttime) > 180:
			starttime = time.time()
			day = int(datetime.today().strftime('%d'))
			print("3 min passed")
			if daysaved != day:
				meteonetwork = open("/home/pi/www/pmn175.txt", "w")
				daysaved = day
				print("pmn175 reset")
			meteonetwork = open("/home/pi/www/pmn175.txt", "a")
			today3 = datetime.today().strftime('%d/%m/%y')
			nowtime2 = datetime.today().strftime('%H:%M')
			str2 = "pmn175;" + today3 + ";" + nowtime2 + ";" + str(MN[0])  + ";" + str(round(MN[1], 1))
			str3 = str2 + ";" + str(round(MN[2], 1)) + ";" + str(round(MN[3], 1)) + ";" + str(MN[4]) + ";" + str(round(MN[5], 1))  + ";" + str(round(MN[6], 1)) + ";" + str(round(MN[7], 1)) + ";" + str(round(MN[8], 1)) + ";IDEarduino;1.8.17;-99999;-99999;-99999;-99999; \n"
			meteonetwork.write(str3)
			print("row add to pmn175.txt")
			meteonetwork.close()