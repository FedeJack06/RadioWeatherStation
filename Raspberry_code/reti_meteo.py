import time
import mysql.connector
from datetime import datetime
import smtplib, ssl

mydb = mysql.connector.connect(
    host="localhost",
    user="meteo",
    password="federico3000",
    database="meteo"
)
mycursor = mydb.cursor()

lineameteo = open("/home/pi/www/meteo.txt", "w")
meteonetwork = open("/home/pi/www/pmn175.txt", "a")

daysaved = int(datetime.today().strftime('%d'))
timer = time.time()

direzioni = ["NNE","NE","ENE","E","ESE","SE","SSE","S","SSO","SO","OSO","O","ONO","NO","NNO","N"]
gradi = [22,45,67,90,112,135,157,180,202,225,247,270,292,315,337,0]
#windang = gradi[direzioni.index(winddir)]
#winddir = direzioni[gradi.index(int(windang))]
dooo = True
lastTime = 0

while True:
	if (time.time() - timer) > 60:
		timer = time.time()
		todayMYSQL = datetime.today().strftime('%Y-%m-%d')
		#nowtime = datetime.today().strftime('%H:%M:%S')

		mycursor.execute(f"select tp,ur,pressionelivellodelmare/100, wind*3.6, windAng, mmPioggia, rainrate, date, time, dewpoint, UNIX_TIMESTAMP(TIMESTAMP(date,time)) from dataday ORDER BY TIMESTAMP(date,time) DESC LIMIT 1")
		data = mycursor.fetchone()
		currentTime = data[10]
		mycursor.execute(f"select min(tpmin), max(tpmax), max(windmax)*3.6 from estremi where date = '{todayMYSQL}'")
		extr = mycursor.fetchone()
		mydb.commit()

		if currentTime != lastTime and extr:
			lastTime = currentTime

			#lineameteo
			lineameteo = open("/home/pi/www/meteo.txt", "w")
			todayLM = datetime.strptime(str(data[7]), '%Y-%m-%d').strftime('%d/%m/%Y')
			lineameteo.write(todayLM + " " + str(data[8]))
			lineameteo.write("&v0=44.6327777777775&v0=8.6&v0=Cremolino(AL)&v0=312m s.l.m.&v0=")
			winddir = direzioni[gradi.index(data[4])]
			str1 = str(data[0]) + "&v0=" + str(data[1]) + "&v0=" + str(data[2]) + "&v0=" + str(data[3]) + "&v0=" + winddir + "&v0=" + str(data[5]) + "&v0=" + str(extr[0]) + "&v0=" + str(extr[1]) + "&v0=" + str(extr[2]) + "&v0=" + str(data[6])
			lineameteo.write(str1)
			lineameteo.close()

			#meteonetwork
			day = int(datetime.today().strftime('%d'))
			if daysaved != day:
				meteonetwork = open("/home/pi/www/pmn175.txt", "w")
				daysaved = day
				print("pmn175 reset")
			meteonetwork = open("/home/pi/www/pmn175.txt", "a")
			todayMN = datetime.strptime(str(data[7]), '%Y-%m-%d').strftime('%d/%m/%y')
			timeMN = datetime.strptime(str(data[8]), '%H:%M:%S').strftime('%H:%M')
			str2 = "pmn175;" + todayMN + ";" + timeMN + ";" + str(round(data[0], 1))  + ";" + str(round(data[2], 1))
			str3 = str2 + ";" + str(round(data[1], 1)) + ";" + str(round(data[3], 1)) + ";" + str(data[4]) + ";" + str(round(extr[2], 1))  + ";" + str(round(data[6], 1)) + ";" + str(round(data[5], 1)) + ";" + str(round(data[9], 1)) + ";Arduino-Python;1.8.17-3.7.3;-99999;-99999;-99999;-99999; \n"
			meteonetwork.write(str3)
			print("row add to pmn175.txt")
			meteonetwork.close()

		#controllo inserimento data
		if time.time() - currentTime > 600 and dooo:
			dooo = False
			#email stuff
			port = 587  # For starttls
			smtp_server = "smtp.gmail.com"
			sender_email = "giacobbefederico3@gmail.com"
			receiver_email = "fedejek06@gmail.com"
			password = "uhhbuwqtwsfaqglo"
			message = """\
			Subject: Dati meteo non inseriti

			Dati non inseriti da 10 minuti nel database."""
			context = ssl.create_default_context()
			with smtplib.SMTP(smtp_server, port) as server:
				server.ehlo()  # Can be omitted
				server.starttls(context=context)
				server.ehlo()  # Can be omitted
				server.login(sender_email, password)
				server.sendmail(sender_email, receiver_email, message)
			print("email sent")
		if time.time() - currentTime < 600:
			dooo=True
mycursor.close()
mydb.close()
