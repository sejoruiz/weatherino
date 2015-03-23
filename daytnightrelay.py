#!/usr/bin/env python
import datetime
import time
import logging
from suncalc import SunCalc
import serial

#Convert strings to bytes. According to the Arduino Doc, this is needed
isDay = b'Day'
isNight = b'Night'
serialBauds=115200
#Open the serial port
ser = serial.Serial('/dev/ttyACM0', serialBauds)

while not ser.isOpen():
     continue

while True:
    sunc = SunCalc(latitude=60, longitude=16.5, date=datetime.datetime.now(), tzone='Europe/Stockholm')
    riseDate = sunc.sunriseCalc()
    time.sleep((riseDate-datetime.datetime.now()).seconds)
    ser.write(isDay)
    sunc = SunCalc(latitude=60, longitude=16.5, date=datetime.datetime.now(), tzone='Europe/Stockholm')
    setDate = sunc.sunsetCalc()
    time.sleep((setdate-datetime.datetime.now()).seconds)
    ser.write(isNight)
    
