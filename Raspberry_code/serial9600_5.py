import serial
import time
import mysql.connector
from datetime import datetime
import numpy as np

time.sleep(1)
#connect to meteo.dataday
mydb = mysql.connector.connect(
  host="localhost",
  user="meteo",
  password="federico3000",
  database="meteo"
)
mycursor = mydb.cursor()

##timing 
starttime = time.time()
errorCount = 0
link_lost = False

##data structur
data = np.empty((0,11)) #matrice per medie su colonne
rowPy = [0,0,0,0,0,0,0,0,0,0,0] #righa dati aggiunta ad ogni ciclo
label = {"send":0, "tp":1, "tpdht":2, "tp2":3, "ur":4, "ur2":5, "pressure":6, "slpressure":7, "wind":8, "rainrate":9, "volt":10}
gradi = [22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0]
modaDir = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0] #array con frequenza assoluta della direzione del vento, indici risppettivi a gradi[]
rowNumber = 0

#begin serial
ser = serial.Serial('/dev/ttyACM0', 9600)
ser.close()
ser.open()

## PREOCESSING
while True:
	radio_link = ser.readline().decode().rstrip()#cerco testa del treno dati
	print(radio_link)
	if radio_link == "%0" or radio_link == "%1": #inizio treno dati con %0 o %1
		#LETTURA DATI da seriale
		for i in range(10): #i from 0 to 9
			rowPy[i] = float(ser.readline().decode().rstrip())
		ws_date = ser.readline().decode().rstrip()
		ws_time = ser.readline().decode().rstrip()
		winddirindex = int(float(ser.readline().decode().rstrip()))
		mmpioggia = float(ser.readline().decode().rstrip())
		mediatp = float(ser.readline().decode().rstrip())
		if mediatp < -40:
			mediatp = None
		raffica = float(ser.readline().decode().rstrip())
		RRmax = float(ser.readline().decode().rstrip())
		bmp180_status = ser.readline().decode().rstrip()
		radio_status = ser.readline().decode().rstrip()
		sd_status = ser.readline().decode().rstrip()
		rowPy[label['volt']] = float(ser.readline().decode().rstrip())
        #VERIFICA ERRORI
		row = np.array(rowPy)
		row[row == 999] = np.nan #nan where sensor not connected	

        #INSERIMENTO DATI IN MATRICE MEDIE
		if radio_link == "%1":                      #se dato è fresco dallo slave, èa appena arrivato dal master
			print(ws_date, ws_time, rowPy)
			data = np.append(data, [row], axis = 0) #add row to matrix
			modaDir[winddirindex] += 1              #aggiungo 1 frequenza alla moda direzione vento
			rowNumber += 1
			print("row added ___")
			link_lost = False
		elif radio_link == "%0":  #se dato non fresco non fare niente, altrimenti stesso dato contato più volte nella media
			link_lost = True

		#DATI DATABASE
		if (time.time() - starttime) > 60 and np.shape(data)[0] > 0:      #quando è passato 1 min e almeno una riga inserita		
			starttime = time.time()             #medie calcolate sui dati freschi raccolti nel minuto, anche 1 sola riga  :(
			#datatime attuali
			today = datetime.today().strftime('%Y-%m-%d')
			nowtime = datetime.today().strftime('%H:%M:%S')
			#media
			means = np.nanmean(data, axis=0)
			MEANS = np.ndarray.tolist(np.where(np.isnan(means), None, means)) # Nan to None and convert to pythonlist, float type 
			tp = MEANS[label['tp']]
			tpdht = MEANS[label['tpdht']]
			tp2 = MEANS[label['tp2']]
			ur = MEANS[label['ur']]
			ur2 = MEANS[label['tp']]
			pressure = MEANS[label['pressure']]
			slpressure = MEANS[label['slpressure']]
			wind = MEANS[label['wind']]
			rainrate = MEANS[label['rainrate']]
			windang = int(gradi[modaDir.index(max(modaDir))])
			volt = MEANS[label['volt']]
			#DERIVATE
			#dewpoint - wetbulb
			if not ur or (not tp and not tpdht):
				dewpoint = None
				wetbulb = None
			elif not tp:
				dewpoint = pow(ur/100,0.125)*(112+(0.9*tpdht)) + (0.1*tpdht) - 112
				wetbulb = tpdht * (0.45 + 0.006 * ur * pow(slpressure/106000, 0.5))
			else:
				dewpoint = pow(ur/100,0.125)*(112+(0.9*tp)) + (0.1*tp) - 112
				wetbulb = tp * (0.45 + 0.006 * ur * pow(slpressure/106000, 0.5))
			#heatindex
			if not ur or not tp:
				heatindex = None
			elif tp>27 and ur>40:
				heatindex = -42.379 + 2.04901523*(tp*1.8 + 32) + 10.14333127*ur - 0.22475541*(tp*1.8 + 32)*ur - 0.00683783*pow(tp*1.8 + 32,2) - 0.05481717*pow(ur,2) + 0.00122874*pow(tp*1.8 + 32,2)*ur + 0.00085282*(tp*1.8 + 32,2)*pow(ur,2) - 0.00000199*pow(tp*1.8 + 32,2)*pow(ur,2)
			else:
				heatindex = tp
			#windchill
			if not tp:
				windchill = None
			elif wind >= 1.3:
				windchill = (13.12 + 0.6215 * tp) - (11.37 * pow(wind*3.6, 0.16)) + (0.3965 * tp * pow(wind*3.6, 0.16))
			else:
				windchill = tp
			
			#import in meteo.dataday
			sql = "INSERT INTO dataday(date, time, wsDate, wsTime, tp, tpdht, tp2, ur, ur2, dewpoint, heatindex, pressione, pressionelivellodelmare, wind, windAng, windchill, mmPioggia, rainrate, mediatp, wetbulb, nData) values (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
			val = (today, nowtime, ws_date, ws_time, tp, tpdht, tp2, ur, ur2, dewpoint, heatindex, pressure, slpressure, wind, windang, windchill, mmpioggia, rainrate, mediatp, wetbulb, rowNumber)
			mycursor.execute(sql, val)
			mydb.commit()
			print(mycursor.rowcount, "data inserted.")
			#import in meteo.control
			sql2 = "INSERT INTO control(date, time, bmp_status, radio_status, sd_status, volt) values(%s, %s, %s, %s, %s, %s)"
			val2 = (today, nowtime, bmp180_status, radio_status, sd_status, volt)
			mycursor.execute(sql2, val2)
			mydb.commit()
			print(mycursor.rowcount, "controls inserted.")
			#import in meteo.estremi
			maxx = np.nanmax(data, axis=0)
			MAXX = np.ndarray.tolist(np.where(np.isnan(maxx), None, maxx)) # Nan to None and convert to pythonlist, float type 
			minn = np.nanmin(data, axis=0)
			MINN = np.ndarray.tolist(np.where(np.isnan(minn), None, minn)) # Nan to None and convert to pythonlist, float type 
			tpmax = MAXX[label['tp']]
			tpmin = MINN[label['tp']]
			urmax = MAXX[label['ur']]
			urmin = MINN[label['ur']]
			pressuremax = MAXX[label['pressure']]
			pressuremin = MINN[label['pressure']]
			slpressuremax = MAXX[label['slpressure']]
			slpressuremin = MINN[label['slpressure']]
			windmax = MAXX[label['wind']]
			rainratemax = MAXX[label['rainrate']]

			sql3 = "INSERT INTO estremi(`date`, `time`, `tpmax`, `tpmin`, `urmax`, `urmin`, `pressuremax`, `pressuremin`, `slpressuremax`, `slpressuremin`, `windmax`, `rainratemax`, `raffica`, `RRmax`) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)"
			val3 = (today, nowtime, tpmax, tpmin, urmax, urmin, pressuremax, pressuremin, slpressuremax, slpressuremin, windmax, rainratemax, raffica, RRmax)
			mycursor.execute(sql3, val3)
			mydb.commit()
			print(mycursor.rowcount, "extreme inserted. ___")

			#reset data matrix and moda dir
			data = np.empty((0,11))
			modaDir = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
			rowNumber = 0
	else:
		#se dopo 50 righe lette non trovo testa del treno dati riapro seriale
		errorCount += 1
		if errorCount == 50:
			print("error... reopen serial ___")
			errorCount = 0
			ser.close()
			ser.open()
# np.ma.masked_where(data == 999, data)
