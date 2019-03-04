# Homebrewing Fermentation Monitor

## Background
Our system is designed for the use of monitoring the home brew fermentation process. Normally fermentation is monitored by taking manual daily samples of the beer with a hydrometer to determine the liquid density. This liquid density decreases as the yeast convert the sugars into carbon dioxide and ethanol, and levels out when fermentation is complete. This method can easily introduce outside contamination each time a sample is taken, so a hands free automated process is preferred.  

The system includes a freely floating sensor which measures gravitational acceleration in three planes. This correlates the tilt angle of the sensor to the density of the liquid. As the liquid density decreases, the angle of the tube floating in the water changes correspondingly. The accelerometer data is wirelessly sent via UDP packet to another wifi module. This base wifi module logs the data in an SD card and controls an LCD screen to output fermentation status. Care will be taken to reduce power consumption on the sensor module by placing it to sleep when not in use, as the fermentation process can take a few weeks. 

## Sensor materials: 
* MMA8452Q Triple Axis Accelerometer
* ESP-WROOM-02 battery and wifi module (with 3400 mAh LiIon battery)
* Watertight tube
* Lead weights

## Base materials: 
* D1 mini wifi module
* TFT 2.4 touchscreen
* Micro SD Card Shield
