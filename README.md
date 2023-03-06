# RadioWeatherStation
<p>Weather stazione with outdoor sensors radio connected with indoor lcd.</p>
<p>One arduino mega (one can be used) collecting data from external sensor, send data with a nRF24 2.4GHz (monopol antenna), data are localli saved in sd.</p>
<p>Second indor arduino mega store data in sd, show them on 1.8" TFT screen, send data to raspberry on serial port.</p>
<p>Raspeberry store data in mariaDB database and host the station site</p>

<p>Originally the station was made with only two arduino, that's why two arduino are still present in the project. Indoor arduino can be removed.</p>
<p>All the data came from sensor is considered in the average calculus of these and stored according to statistical treatment of data.</p> 

<p>
  <strong>Sensors:</strong>
<li>sht35 (I2C)</li>
<li>bmp180 (I2C)</li>
<li>dht22 (digital) (optional)</li>
<li>ds18b20 (digital) (optional)</li>
<li>anemometer (reed contact)</li>
<li>wind direction (voltage output)</li>
<li>rain gauge (reed contact)</li>
</p>

<p>
  <strong>Variables measured:</strong>
<li>Temperature</li>
<li>Humidity</li>
<li>Atmospheric pressure</li>
<li>Wind Speed</li>
<li>Wind Direction</li>
<li>mm rain</li>
<li>Rain rate</li>
<li>DewPoint, WetBulb, HeatIndex, WindChill</li>
</p>
