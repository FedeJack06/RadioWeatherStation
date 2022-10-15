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
datagraph = open("/home/pi/dscript/datagraph.dat", "a")

##time for timing 
starttime = time.time()
starttime2 = time.time()
startGraph = time.time()
daysaved = int(datetime.today().strftime('%d')) 
##
errorCount = 0

#extractDay = open("/home/pi/dscript/countDay.dat", "r")
#startDay = float(extractDay.read())
#print(startDay)

## PREOCESSING
while True:
	#### INPUT DATI DA ARDUINO
	init = ser.readline()
	link_status = ser.readline().decode('utf-8').rstrip()
	send_status = ser.readline().decode('utf-8').rstrip()
	ws_date = ser.readline().decode('utf-8').rstrip()
	ws_time = ser.readline().decode('utf-8').rstrip()
	tp = float(ser.readline().decode('utf-8').rstrip())
	tpdht = float(ser.readline().decode('utf-8').rstrip())
	tp2 = float(ser.readline().decode('utf-8').rstrip())
	ur = float(ser.readline().decode('utf-8').rstrip())
	ur2 = float(ser.readline().decode('utf-8').rstrip())
	pressure = float(ser.readline().decode('utf-8').rstrip())
	slmpressure = float(ser.readline().decode('utf-8').rstrip())
	wind = float(ser.readline().decode('utf-8').rstrip())
	windmedio = float(ser.readline().decode('utf-8').rstrip())
	windmax = float(ser.readline().decode('utf-8').rstrip())
	winddir = ser.readline().decode('utf-8').rstrip()
	winddirmedio = ser.readline().decode('utf-8').rstrip()
	windang = int(float(ser.readline().decode('utf-8').rstrip()))
	windangmedio = int(float(ser.readline().decode('utf-8').rstrip()))
	mmpioggia = float(ser.readline().decode('utf-8').rstrip())
	rainrate = float(ser.readline().decode('utf-8').rstrip())
	rrmedio = float(ser.readline().decode('utf-8').rstrip())
	rrmax = float(ser.readline().decode('utf-8').rstrip())
	tpmax = float(ser.readline().decode('utf-8').rstrip())
	tpmin = float(ser.readline().decode('utf-8').rstrip())
	urmax = float(ser.readline().decode('utf-8').rstrip())
	urmin = float(ser.readline().decode('utf-8').rstrip())
	slmpressuremax = float(ser.readline().decode('utf-8').rstrip())
	slmpressuremin = float(ser.readline().decode('utf-8').rstrip())
	maxwind = float(ser.readline().decode('utf-8').rstrip())
	maxrr = float(ser.readline().decode('utf-8').rstrip())
	mediatp = float(ser.readline().decode('utf-8').rstrip())
	end_data = ser.readline()
	### errori-controlli
	bmp180_status = ser.readline().decode('utf-8').rstrip()
	radio_status = ser.readline().decode('utf-8').rstrip()
	sd_status = ser.readline().decode('utf-8').rstrip()
	volt = ser.readline().decode('utf-8').rstrip()
	end_control = ser.readline()
	print (link_status)
	print (send_status)
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
	###se OK LINK alla stazione esterna
	if link_status == "1":
		print ("ok link")
		print (ws_date)
		print (ws_time)
		print (ur)
		print (winddirmedio)
		print (mediatp)
		#datatime attuali
		today = datetime.today().strftime('%Y-%m-%d')
		nowtime = datetime.today().strftime('%H:%M:%S')

		#lineameteo
		lineameteo = open("/home/pi/www/meteo.txt", "w")
		today2 = datetime.today().strftime('%d/%m/%Y')
		lineameteo.write(today2 + " " + nowtime)
		lineameteo.write("&v0=44.6327777777775&v0=8.6&v0=Cremolino(AL)&v0=312m s.l.m.&v0=")
		str1 = str(tp) + "&v0=" + str(ur2) + "&v0=" + str(slmpressure/100) + "&v0=" + str(windmedio*3.6) + "&v0=" + winddirmedio + "&v0=" + str(mmpioggia) + "&v0=" + str(tpmin) + "&v0=" + str(tpmax) + "&v0=" + str(windmax*3.6) + "&v0=" + str(rainrate)
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
			str2 = "pmn175;" + today3 + ";" + nowtime2 + ";" + str(tp)  + ";" + str(round(slmpressure/100, 1))
			str3 = str2 + ";" + str(round(ur2, 1)) + ";" + str(round(windmedio*3.6, 1)) + ";" + str(windangmedio) + ";" + str(round(windmax*3.6, 1))  + ";" + str(round(rainrate, 1)) + ";" + str(round(mmpioggia, 1)) + ";" + str(round(dewpoint, 1)) + ";IDEarduino;1.8.17;-99999;-99999;-99999;-99999; \n"
			meteonetwork.write(str3)
			print("row add to pmn175.txt")
			meteonetwork.close()

		#data Graph
		#reset ogni 2 giorni
		'''if (time.time() - startDay) > 259200: #due giorni
			startDay = time.time()
			datagraph = open("/home/pi/dscript/datagraph.dat", "w")
			datagraph.write("0")
			datagraph.close()
			print("dataGraph reset")
			#saved the startday
			countDay = open("/home/pi/dscript/countDay.dat", "w")
			countDay.write(str(startDay))
		#save data every 3 minute
		if (time.time() - startGraph) > 180:
			startGraph = time.time()
			graph = str(time.time()) + " " + str(tp) + " " + str(tpdht) + " " + str(ur) + " " + str(dewpoint) + " " + str(heatindex) + " " + str(slmpressure) + " " + str(float(wind)*3.6) + " " + str(windangmedio) + " " + str(windchill) + " " + str(mmpioggia) + " " + str(rainrate) + " \n"
			datagraph = open("/home/pi/dscript/datagraph.dat", "a")
			datagraph.write(graph)
			print("datagraph written")
			datagraph.close()'''

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
