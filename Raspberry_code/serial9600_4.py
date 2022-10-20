import serial
import time
import mysql.connector
from datetime import datetime

time.sleep(1)
#connect to meteo.dataday
mydb = mysql.connector.connect(
  host="localhost",
  user="meteo",
  password="federico3000",
  database="meteo"
)
mycursor = mydb.cursor()

#begin serial
ser = serial.Serial('/dev/ttyACM0', 9600)
time.sleep(3)
ser.close()
ser.open()

##timing 
starttime = time.time()
errorCount = 0

## PREOCESSING
while True:
	#### INPUT DATI DA ARDUINO
	link_status = ser.readline().decode().rstrip()
	send_status = ser.readline().decode().rstrip()
	ws_date = ser.readline().decode().rstrip()
	ws_time = ser.readline().decode().rstrip()
	TP = ser.readline().decode().rstrip()
	tpdht = ser.readline().decode().rstrip()
	tp2 = ser.readline().decode().rstrip()
	UR = ser.readline().decode().rstrip()
	ur2 = ser.readline().decode().rstrip()
	pressure = ser.readline().decode().rstrip()
	SLPRESSURE = ser.readline().decode().rstrip()
	WIND = ser.readline().decode().rstrip()
	windmedio = ser.readline().decode().rstrip()
	windmax = ser.readline().decode().rstrip()
	winddir = ser.readline().decode().rstrip()
	winddirmedio = ser.readline().decode().rstrip()
	windang = ser.readline().decode().rstrip()
	windangmedio = ser.readline().decode().rstrip()
	mmpioggia = ser.readline().decode().rstrip()
	rainrate = ser.readline().decode().rstrip()
	rrmedio = ser.readline().decode().rstrip()
	rrmax = ser.readline().decode().rstrip()
	maxwind = ser.readline().decode().rstrip()
	maxrr = ser.readline().decode().rstrip()
	mediatp = ser.readline().decode().rstrip()
	### errori-controlli
	bmp180_status = ser.readline().decode().rstrip()
	radio_status = ser.readline().decode().rstrip()
	sd_status = ser.readline().decode().rstrip()
	volt = ser.readline().decode().rstrip()
	print (link_status)
	print (send_status)
	print (ws_date)
	print (ws_time)
	print (TP)
	print (UR)
	print (SLPRESSURE)
	print (winddirmedio)
	if link_status == "%1":
		print ("ok link")
		tp = float(TP)
		ur = float(UR)
		slmpressure = float(SLPRESSURE)
		wind = float(WIND)
		#derivate
		dewpoint = pow(ur/100,0.125)*(112+(0.9*tp)) + (0.1*tp) - 112
        
		if tp>27 and ur>40:
			heatindex = -42.379 + 2.04901523*(tp*1.8 + 32) + 10.14333127*ur - 0.22475541*(tp*1.8 + 32)*ur - 0.00683783*pow(tp*1.8 + 32,2) - 0.05481717*pow(ur,2) + 0.00122874*pow(tp*1.8 + 32,2)*ur + 0.00085282*(tp*1.8 + 32,2)*pow(ur,2) - 0.00000199*pow(tp*1.8 + 32,2)*pow(ur,2)
		else:
			heatindex = tp

		if wind >= 1.3:
			windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind*3.6, 0.16)) + (0.3965 * tp * pow(wind*3.6, 0.16))
		else:
			windchill = tp

		wetbulb = tp * (0.45 + 0.006 * ur * pow(slmpressure/106000, 0.5))
		#datatime attuali
		today = datetime.today().strftime('%Y-%m-%d')
		nowtime = datetime.today().strftime('%H:%M:%S')

		#import in meteo.dataday
		sql = "INSERT INTO dataday(date, time, wsDate, wsTime, tp, tpdht, tp2, ur, ur2, dewpoint, heatindex, pressione, pressionelivellodelmare, wind, windMedio, windMax, windDir, windDirMedio, windAng, windAngMedio, windchill, mmPioggia, rainrate, rainrateMedio, rainrateMax, maxWindInt, maxRRInt, mediatp, wetbulb) values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
		val = (today, nowtime, ws_date, ws_time, tp, tpdht, tp2, ur, ur2, dewpoint, heatindex, pressure, slmpressure, wind, windmedio, windmax , winddir, winddirmedio, windang, windangmedio, windchill, mmpioggia, rainrate, rrmedio, rrmax, maxwind, maxrr, mediatp, wetbulb)
		#import in meteo.control
		sql2 = "INSERT INTO control(bmp_status, radio_status, sd_status, volt) values(%s, %s, %s, %s)"
		val2 = (bmp180_status, radio_status, sd_status, volt)
		# IF TIME RECORD DATA
		if (time.time() - starttime) > 60:
			starttime = time.time()
			mycursor.execute(sql, val)
			mydb.commit()
			print(mycursor.rowcount, "data inserted.")
			##control
			mycursor.execute(sql2, val2)
			mydb.commit()
			print(mycursor.rowcount, "controls inserted.")
	elif link_status == "%0":
		print ("link lost")
	else:
		print("error")
		errorCount += 1
		if errorCount == 10:
			print("error... reopen serial")
			errorCount = 0
			ser.close()
			time.sleep(3)
			ser.open()
			time.sleep(1)
	print ("____")