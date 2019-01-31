# Homebrewing Fermentation Monitor

Our system is designed for the use of monitoring the home brew fermentation process. Normally fermentation is monitored by taking manual daily samples of the beer with a hydrometer to determine the liquid density. This liquid density decreases as the yeast convert the sugars into carbon dioxide and ethanol, and levels out when fermentation is complete. This method can easily introduce outside contamination each time a sample is taken, so hands free automated process is preferred.  

The system includes a freely floating sensor which measures gravitational acceleration in two planes. This correlates the tilt angle of the sensor to the density of the liquid. As the liquid density decreases, the angle of the tube floating in the water changes as well. The accelerometer data is sent over wifi to another wifi module. This wifi module controls an LCD screen and outputs fermentation status. Care will be taken to reduce power consumption on the sensor module by placing it to sleep when not in use, as the fermentation process can take a few weeks. 

Sensor materials: 
MMA8452Q Triple Axis Accelerometer
ESP-WROOM-02 battery and wifi module (with 3400 mAh LiIon battery)
Glass tube

Base materials: 
D1 mini wifi module
TFT 2.4 touchscreen
