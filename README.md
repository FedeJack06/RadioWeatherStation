# RadioWeatherStation
<p>Weather stazione with outdoor sensors radio connected with an indoor lcd and database.</p>
<p>One Arduino mega2560 (UNO can also be used) collecting data from external sensor, send data via radio with a nRF24 2.4GHz module to indoor Arduino; data are locally saved in SD for backup.</p>
<p>Second indor Arduino mega (UNO can also be used) show data on 1.8" TFT screen, send data to raspberry on serial port.</p>
<p>Raspeberry store data in mariaDB database and host the <a href="https://meteocremolino.ddns.net">station site</a>.</p>

<p>Originally the station was made with only two arduino, the indoor one was a server for a html page (see Webbino code version v1.3). That's why two arduino are still present in the project, indoor arduino can be removed.</p>
<p>Data from a sensor is the average of 60 measures over one minute, sampling frequency of 1Hz.</p> 

<p>
  <strong>Sensors:</strong>
<li>Sensirion sht35 (I2C)</li>
<li>Bosch bmp180 (I2C)</li>
<li>DHT22 (digital) (optional)</li>
<li>ds18b20 (digital) (optional)</li>
<li>Anemometer DeAgostini (reed contact)</li>
<li>Wind direction DeAgostini (voltage output)</li>
<li>Rain gauge DeAgostini (reed contact)</li>
</p>

<p>
  <strong>Variables measured:</strong>
<li>Temperature</li>
<li>Humidity</li>
<li>Atmospheric pressure</li>
<li>Wind Speed</li>
<li>Wind Direction</li>
<li>mm of rain</li>
<li>Rain rate</li>
<li>DewPoint, WetBulb, HeatIndex, WindChill</li>
</p>
