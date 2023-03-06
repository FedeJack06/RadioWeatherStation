import requests
import time
from bs4 import BeautifulSoup
from datetime import datetime
#temporizzazione
starttime = time.time()

while True:
	if (time.time() - starttime) > 30:
		starttime = time.time()

# Inizializzo una sessione per mantenere in memoria tutti i cookie
		session = requests.Session()
		session.headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.110 Safari/537.36'}

# Ottengo la pagina index in cui si trova l'immagine
		index_page = session.get('https://www.meteoblue.com/it/tempo/previsioni/multimodel/cremolino_italia_3177839?fcstlength=144&params%5B%5D=&params%5B%5D=NMM4&params%5B%5D=NMM22&params%5B%5D=NEMS4&params%5B%5D=NEMS12&params%5B%5D=NEMS12_E&params%5B%5D=NEMSGLOBAL&params%5B%5D=NEMSGLOBAL_E&params%5B%5D=&params%5B%5D=IFS04&params%5B%5D=UMGLOBAL&params%5B%5D=ICONEU&params%5B%5D=ICON&params%5B%5D=ICOND2&params%5B%5D=GFS05&params%5B%5D=GEM15&params%5B%5D=MFFR&params%5B%5D=MFEU&params%5B%5D=MFGLOBAL&params%5B%5D=HARMONIE&params%5B%5D=HIREU7')
		index_page = BeautifulSoup(index_page.text, 'html.parser')

# Cerco l'immagine nel codice HTML
		image_url = index_page.find('div', {'id': 'blooimage'}).find('img').attrs['data-original']

# Navigo all'url dell'immagine e estraggo il contenuto
		image_content = session.get('https:' + image_url).content

# Salvo il file immagine
		open('/home/pi/www/getmeteoblue.jpg', 'wb').write(image_content)


# Ottengo la pagina index in cui si trova l'immagine
		index_page2 = session.get('https://www.meteoblue.com/it/tempo/previsioni/multimodel/cremolino_italia_3177839?fcstlength=72&params%5B%5D=&params%5B%5D=NMM4&params%5B%5D=NMM22&params%5B%5D=NEMS4&params%5B%5D=NEMS12&params%5B%5D=NEMS12_E&params%5B%5D=NEMSGLOBAL&params%5B%5D=NEMSGLOBAL_E&params%5B%5D=&params%5B%5D=IFS04&params%5B%5D=UMGLOBAL&params%5B%5D=ICONEU&params%5B%5D=ICON&params%5B%5D=ICOND2&params%5B%5D=GFS05&params%5B%5D=GEM15&params%5B%5D=MFFR&params%5B%5D=MFEU&params%5B%5D=MFGLOBAL&params%5B%5D=HARMONIE&params%5B%5D=HIREU7')
		index_page2 = BeautifulSoup(index_page2.text, 'html.parser')

# Cerco l'immagine nel codice HTML
		image_url2 = index_page2.find('div', {'id': 'blooimage'}).find('img').attrs['data-original']

# Navigo all'url dell'immagine e estraggo il contenuto
		image_content2 = session.get('https:' + image_url2).content

# Salvo il file immagine
		open('/home/pi/www/getmeteoblue3.jpg', 'wb').write(image_content2)
		#print("done ")
