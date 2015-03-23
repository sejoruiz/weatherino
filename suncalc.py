#!/usr/bin/env python
from datetime import datetime, timedelta
from pytz import timezone
import pytz
import math
import logging
class SunCalc:
    
    def __init__(self, latitude, longitude, date, formatting='%d %b %Y %H:%M:%S', tzone='Europe/Madrid'):
        self.latitude=latitude
        # The longitude is west in the calculations
        self.longitude=-longitude
        #Convert date to a suitable format
        if not isinstance(date, datetime):
            date = datetime.strptime(date, formatting)
        # Add the timezone information, and store it in tzone
        if date.tzinfo is None:
            tzone = timezone(tzone)
            date = tzone.localize(date)
        else:
            tzone = date.tzinfo
        self.tzone = tzone
        #Convert to UTC
        self.date = date.astimezone(timezone("UTC"))
        self.julian = self.timeToJulian()

	#Given a Gregorian date (in UTC) calculate the corresponding Julian Date
    def timeToJulian(self, date=None):
		# If we didn't get a datetime instance, we try to convert it
        if date is None:
            date=self.date

        c0 = math.floor((date.month-3)/12)
        x4 = date.year+c0
        x3 = math.floor(x4/100)
        x2 = x4%100
        x1=date.month - 12*c0 -3
        jd = math.floor(146097*x3/4) + math.floor(36525*x2/100) + math.floor((153*x1+2)/5) + date.day + 1721119
        return jd

    def julianToTime(self, julianDate=None):
        if julianDate is None:
            julianDate=self.julian
            
        k3=4*(julianDate-1721120)+3
        x3=math.floor(k3/146097)
        k2=100*math.floor((k3%146097)/4)+99
        x2=math.floor(k2/36525)
        k1=5*math.floor((k2%36525)/100)+2
        x1=math.floor(k1/153)
        c0=math.floor((x1+2)/12)
        year=int(round(100*x3+x2+c0))
        month=int(round(x1-12*c0+3))
        day=int(math.floor((k1%153)/5)+1)
        #Hour minutes and seconds calculation
        day_frac=julianDate - math.floor(julianDate)
        #Hours since the beginning of the JULIAN day. Julian days begin at 12 AM
        hour=math.floor(day_frac*24)
        day_frac=day_frac*24 - hour
        #Adjust hour: 0 in Julian is 12AM in Gregorian
        hour=int((12+hour)%24)
        minute=int(math.floor(day_frac*60))
        day_frac=day_frac*60 - minute
        second=int(math.floor(day_frac*60))
        targetTime=datetime(year,month,day,hour,minute,second)
        zone=timezone("UTC")
        targetTime=zone.localize(targetTime)
        targetTime=targetTime.astimezone(self.tzone)
        logging.debug(targetTime.strftime('Date is %d, %b %Y %H:%M:%S'))
        return targetTime
        
    #Calculate the mean anomaly of the orbit of the Earth
    def meanAnomalyCalc(self, julian=None):
        if julian is None:
            julian=self.julian

        #Constant definition
        m0=357.5291
        m1=0.98560028
        j2000=2451545
        
        #Calculation
        self.anomaly = (m0+m1*(self.julian-j2000))%360
        return self.anomaly

    #Calculate the center of the orbit of the Earth, with the true Anomaly
    def centerOrbitCalc(self, anomaly=None):
        if anomaly is None:
            anomaly=self.anomaly

        c1=1.9148
        c2=0.02
        c3=0.0003
        anomaly = math.radians(anomaly)
        self.center = ((c1*math.sin(anomaly) + c2*math.sin(2*anomaly) + c3*math.sin(3*anomaly))%360 + 360)%360
        self.trueAnomaly = (self.center + math.degrees(anomaly))%360
        return self.center

    #Calculate the eliptical longitude of the Sun 
    def elipticLengthCalc(self, trueAnomaly=None):
        if trueAnomaly is None:
            trueAnomaly=self.trueAnomaly

        perihelion = 102.9372
        self.eliptic = (trueAnomaly + perihelion + 180)%360
        return self.eliptic

    #Calculates ascension and declination of the sun
    def ascensiondeclinationCalc(self, eliptic=None):
        if eliptic is None:
            eliptic=self.eliptic
        epsil = math.radians(23.45)
        eliptic = math.radians(eliptic)
        ascension = math.atan2(math.sin(eliptic)*math.cos(epsil), math.cos(eliptic))
        declination = math.asin(math.sin(eliptic)*math.sin(epsil))
        #Asign the value to self so it is easy to retrieve again
        self.ascension = (math.degrees(ascension)+360)%360
        self.declination = (math.degrees(declination)+360)%360
        return self.declination

    def siderealTimeCalc(self, julian=None, longitude=None):
        if julian is None:
            julian=self.julian
        if longitude is None:
          longitude=self.longitude      

        theta0=280.16
        theta1=360.9856235
        julian2k=2451545
        self.sidereal = (theta0 + theta1*(julian-julian2k))%360 - longitude
        return self.sidereal

    def hourAngleCalc(self, sidereal=None, ascension=None, declination=None, latitude=None):
        if sidereal is None:
            sidereal=self.sidereal
        if ascension is None:
            ascension=self.ascension
        if declination is None:
            declination=self.declination
        if latitude is None:
            latitude=self.latitude

        self.hourAngle=sidereal - ascension
        hour = math.radians(self.hourAngle)
        latitude = math.radians(latitude)
        declination = math.radians(declination)
        self.azimuth=math.atan2(math.sin(hour), math.cos(hour)*math.sin(latitude)-math.tan(declination)*math.cos(latitude))
        return self.hourAngle

    #Transit calculations. The moment the Sun passes over the highest point
    def transitCalc(self, targetHour, julian=None, longitude=None, maxIterations=10, threshold=0.00001):
        if julian is None:
            julian=self.julian
        if longitude is None:
            longitude=self.longitude
        julian2k=2451545
        j0=0.0009
        j1=0.0053
        j2=-0.0069
        j3=1
        perihelion = 102.9372
        n=round((julian - julian2k - j0)/j3 - (longitude+targetHour)/360)
        jx=julian2k+j0+(targetHour+longitude)*j3/360+j3*n #First approx of the transit near J
        logging.debug("Jx is %f" % jx)
        logging.debug("n is %f" % n)
        #Second part of the algorithm. Now we reestimate the parameters for the actual sunset/sunrise
        m=self.meanAnomalyCalc(julian=jx)
        lsun=(m+perihelion+180)%360
        logging.debug("New m is %f" % m)
        logging.debug("lsun is %f" % lsun)
        m=math.radians(m)
        lsun=math.radians(lsun)
        jtrans=jx+j1*math.sin(m) + j2*math.sin(2*lsun)
        return jtrans
        # logging.debug("jtrans is %f" % jtrans)
        # #Third iteration, we go for max accuracy. Iterate until threshold. It seems like we lose accuracy here
        # iterations=0
        # jtrans_new=jtrans
        # jtrans_old=0
        # hnew=targetHour
        # hold=0
        # while (abs(jtrans_new - jtrans_old)>threshold and iterations<maxIterations):
        #     jtrans_old=jtrans_new
        #     hold=hnew
        #     sidereal=self.siderealTimeCalc(julian=jtrans_old)
        #     hnew=self.hourAngleCalc(sidereal=sidereal)
        #     jtrans_new=jtrans_old + j3*(hold-hnew)/360
        #     iterations+=1
            
        # print("Number of iterations performed: %d" % iterations) 
        # return jtrans_new
        
    def hourTargetSunset(self, julian=None, latitude=None, declination=None):
        if julian is None:
            julian=self.julian
        if latitude is None:
            latitude=self.latitude
        if declination is None:
            declination=self.declination
            
        declination=math.radians(declination)
        latitude=math.radians(latitude)
        h0=math.radians(-0.83)
        hour = math.acos((math.sin(h0)-math.sin(declination)*math.sin(latitude))/(math.cos(declination)*math.cos(latitude)))
        hour=math.degrees(hour)
        return hour
    
    def hourTargetSunrise(self, julian=None, latitude=None, declination=None):
        if julian is None:
            julian=self.julian
        if latitude is None:
            latitude=self.latitude
        if declination is None:
            declination=self.declination
            
        declination=math.radians(declination)
        latitude=math.radians(latitude)
        h0=math.radians(-0.83)
        hour = math.acos((math.sin(h0)-math.sin(declination)*math.sin(latitude))/(math.cos(declination)*math.cos(latitude)))
        hour=-math.degrees(hour)
        return hour
    
    def sunsetCalc(self, latitude=None, longitude=None, date=None):
        if latitude is None:
            latitude=self.latitude
        if longitude is None:
            longitude=self.longitude
        if date is None:
            date=self.date
            
        self.meanAnomalyCalc() 
        self.centerOrbitCalc()
        self.elipticLengthCalc()
        self.ascensiondeclinationCalc()
        hour=self.hourTargetSunset()
        sunsetJulian=self.transitCalc(targetHour=hour)
        setTime = self.julianToTime(julianDate=sunsetJulian)
        return setTime

    def sunriseCalc(self, latitude=None, longitude=None, date=None):
        if latitude is None:
            latitude=self.latitude
        if longitude is None:
            longitude=self.longitude
        if date is None:
            date=self.date
            
        self.meanAnomalyCalc() 
        self.centerOrbitCalc()
        self.elipticLengthCalc()
        self.ascensiondeclinationCalc()
        hour=self.hourTargetSunrise()
        sunriseJulian=self.transitCalc(targetHour=hour)
        riseTime = self.julianToTime(julianDate=sunriseJulian)
        return riseTime
