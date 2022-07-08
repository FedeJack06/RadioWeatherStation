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
time.sleep(5)
ser.open()

#open file
lineameteo = open("/home/pi/www/meteo.txt", "w")
meteonetwork = open("/home/pi/www/pmn175.txt", "a")
datagraph = open("/home/pi/www/datagraph.dat", "a")

##time for timing 
starttime = time.time()
starttime2 = time.time()
startGraph = time.time()
startDay = time.time()
daysaved = int(datetime.today().strftime('%d')) 
##
errorCount = 0

## PREOCESSING
while True:
	#### INPUT DATI DA ARDUINO
	link_status = ser.readline().decode('utf-8').rstrip()
	send_status = ser.readline().decode('utf-8').rstrip()
	ws_date = ser.readline().decode('utf-8').rstrip()
	ws_time = ser.readline().decode('utf-8').rstrip()
	tp = ser.readline().decode('utf-8').rstrip()
	tpdht = ser.readline().decode('utf-8').rstrip()
	tp2 = ser.readline().decode('utf-8').rstrip()
	ur = ser.readline().decode('utf-8').rstrip()
	ur2 = ser.readline().decode('utf-8').rstrip()
	dewpoint = ser.readline().decode('utf-8').rstrip()
	heatindex = ser.readline().decode('utf-8').rstrip()
	pressure = ser.readline().decode('utf-8').rstrip()
	slmpressure = ser.readline().decode('utf-8').rstrip()
	wind = ser.readline().decode('utf-8').rstrip()
	windmedio = ser.readline().decode('utf-8').rstrip()
	windmax = ser.readline().decode('utf-8').rstrip()
	winddir = ser.readline().decode('utf-8').rstrip()
	winddirmedio = ser.readline().decode('utf-8').rstrip()
	windang = ser.readline().decode('utf-8').rstrip()
	windangmedio = ser.readline().decode('utf-8').rstrip()
	windchill = ser.readline().decode('utf-8').rstrip()
	mmpioggia = ser.readline().decode('utf-8').rstrip()
	rainrate = ser.readline().decode('utf-8').rstrip()
	rrmedio = ser.readline().decode('utf-8').rstrip()
	rrmax = ser.readline().decode('utf-8').rstrip()
	tpmax = ser.readline().decode('utf-8').rstrip()
	tpmin = ser.readline().decode('utf-8').rstrip()
	urmax = ser.readline().decode('utf-8').rstrip()
	urmin = ser.readline().decode('utf-8').rstrip()
	slmpressuremax = ser.readline().decode('utf-8').rstrip()
	slmpressuremin = ser.readline().decode('utf-8').rstrip()
	previousThree = ser.readline().decode('utf-8').rstrip()
	previousSix = ser.readline().decode('utf-8').rstrip()
	tpChange = ser.readline().decode('utf-8').rstrip()
	urChange = ser.readline().decode('utf-8').rstrip()
	dpChange = ser.readline().decode('utf-8').rstrip()
	end_data = ser.readline()
	### errori-controlli
	bmp180_status = ser.readline().decode('utf-8').rstrip()
	radio_status = ser.readline().decode('utf-8').rstrip()
	sd_status = ser.readline().decode('utf-8').rstrip()
	write_extreme = ser.readline().decode('utf-8').rstrip()
	read_extreme = ser.readline().decode('utf-8').rstrip()
	volt = ser.readline().decode('utf-8').rstrip()
	end_control = ser.readline()
	print (link_status)
	print (send_status)
	###se OK LINK alla stazione esterna
	if link_status == "1":
		print ("ok link")
		print (ws_date)
		print (ws_time)
		print (ur)
		print (winddirmedio)
		#datatime attuali
		today = datetime.today().strftime('%Y-%m-%d')
		nowtime = datetime.today().strftime('%H:%M:%S')

		#lineameteo
		lineameteo = open("/home/pi/www/meteo.txt", "w")
		today2 = datetime.today().strftime('%d/%m/%Y')
		lineameteo.write(today2 + " " + nowtime)
		lineameteo.write("&v0=44.6327777777775&v0=8.6&v0=Cremolino(AL)&v0=312m s.l.m.&v0=")
		str1 = str(tp) + "&v0=" + str(ur) + "&v0=" + str(float(slmpressure)/100) + "&v0=" + str(float(windmedio)*3.6) + "&v0=" + winddirmedio + "&v0=" + str(mmpioggia) + "&v0=" + str(tpmin) + "&v0=" + str(tpmax) + "&v0=" + str(float(windmax)*3.6) + "&v0=" + str(rainrate)
		lineameteo.write(str1)
		lineameteo.close()

		#meteonetwork
		if (time.time() - starttime2) > 180:
			starttime2 = time.time()
			day = int(datetime.today().strftime('%d'))
			print("3 min passed")
			if daysaved != day:
				meteonetwork = open("/home/pi/www/pmn175.txt", "w")
				daysaved = day
				print("pmn175 reset")
			meteonetwork = open("/home/pi/www/pmn175.txt", "a")
			today3 = datetime.today().strftime('%d/%m/%y')
			nowtime2 = datetime.today().strftime('%H:%M')
			str2 = "pmn175;" + today3 + ";" + nowtime2 + ";" + str(tp)  + ";" + str(round(float(slmpressure)/100, 1))
			str3 = str2 + ";" + str(round(float(ur), 1)) + ";" + str(round(float(windmedio)*3.6, 1)) + ";" + str(windangmedio) + ";" + str(round(float(windmax)*3.6, 1))  + ";" + str(round(float(rainrate), 1)) + ";" + str(round(float(mmpioggia), 1)) + ";" + str(round(float(dewpoint), 1)) + ";IDEarduino;1.8.17;-99999;-99999;-99999;-99999; \n"
			meteonetwork.write(str3)
			print("row add to pmn175.txt")
			meteonetwork.close()
		
		#data Graph
		if (time.time() - startGraph) > 180:
			startGraph = time.time()
			day = time.time()
			if day - startDay > 172800: #due giorni
				startDay = time.time()
				datagraph = open("/home/pi/www/datagraph.dat", "w")
				print("dataGraph reset")
			graph = str(day) + " " + str(tp) + " " + str(tpdht) + " " + str(ur) + " " + str(dewpoint) + " " + str(heatindex) + " " + str(slmpressure) + " " + str(wind*3.6) + " " + str(windangmedio) + " " + str(windchill) + " " + str(mmpioggia) + " " + str(rainrate) + " \n"
			datagraph.write(graph)
			datagraph.close()

		#import in meteo.dataday
		sql = "INSERT INTO dataday(date, time, wsDate, wsTime, tp, tpdht, tp2, ur, ur2, dewpoint, heatindex, pressione, pressionelivellodelmare, wind, windMedio, windMax, windDir, windDirMedio, windAng, windAngMedio, windchill, mmPioggia, rainrate, rainrateMedio, rainrateMax) values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
		val = (today, nowtime, ws_date, ws_time, tp , tpdht, tp2, ur, ur2, dewpoint, heatindex, pressure, slmpressure, wind, windmedio, windmax , winddir, winddirmedio, windang, windangmedio, windchill, mmpioggia, rainrate, rrmedio, rrmax)
		#import in meteo.control
		sql2 = "INSERT INTO control(bmp_status, radio_status, sd_status, write_extreme, read_extreme, volt) values(%s, %s, %s, %s, %s, %s)"
		val2 = (bmp180_status, radio_status, sd_status, write_extreme, read_extreme, volt)
		#import andamenti
		sql3 = "INSERT INTO andamenti(date, time, pressione3, pressione6, tpchange, urchange, dpchange) values(%s, %s, %s, %s, %s, %s, %s)"
		val3 = (today, nowtime, previousThree, previousSix, tpChange, urChange, dpChange)
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
			##andamenti
			mycursor.execute(sql3, val3)
			mydb.commit()
			print(mycursor.rowcount, "andamenti inserted.") 
	elif link_status == "0":
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
