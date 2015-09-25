#!/usr/bin/python

import threading
import plotly.plotly as py
from plotly.graph_objs import *
import datetime
import time
import serial
import curses
import atexit
import sys
BAUD = 115200

ser = serial.Serial('/dev/ttyAMA0', BAUD, timeout=3)

def exit():
	'''ser.close()'''
	curses.nocbreak()
	screenstd.keypad(0)
	curses.echo()
	curses.endwin()
	print "Exit successful."
atexit.register(exit)

BAUD = 115200
screenWidth = 80
screenHeight = 24
screenstd = curses.initscr()

curses.start_color()
curses.init_pair(1, curses.COLOR_GREEN, curses.COLOR_BLACK)

curses.cbreak()
screenstd.keypad(1)
curses.curs_set(0)
h, w = screenstd.getmaxyx()
win = curses.newwin(1,w,0,0)
win.erase()
win.refresh()
win.clear()
screen = curses.newwin((h-2),w,1,0)
screen2= curses.newwin((h-2),w,1,0)
screen2.erase()
screen2.refresh()
screen2.clear()
bottomWin = curses.newwin((1),w,h,0)
ser = serial.Serial('/dev/ttyAMA0', BAUD, timeout=3)

'''Global vars'''

username = 'Alarindris'
api_key = '7775k8lmv3'
stream_token = 'v1qy8o8a39'
py.sign_in(username, api_key)

trace1 = Scatter(
	x=[],
	y=[],
	yaxis = 'y2',
	name= "Temp(C)",
	stream=dict(
		token=stream_token,
		maxpoints=1000
	)
)

trace2 = Scatter(
	x=[],
	y=[],
	name= "PID output",
	stream=dict(
		token='qp5s1voa07',
		maxpoints=1000
	)
)

layout = Layout(
	title='Generator Temperature Plot',
	showlegend=True,
	autosize=True,
	width=864,
	height=499,
	xaxis=XAxis(
		title='Time CST',
		range=[1423200390292.55, 1423200478609.1],
		type='date',
		autorange=True
	),
	yaxis=YAxis(
		title='PID output %',
		range=[0, 10000],
		type='linear',
		autorange=False
	),
	legend=Legend(
		x=1.0015037593984963,
		y=-0.1755485893416928
	),
	yaxis2=YAxis(
		title='Temperature C',
		range=[23.468888888888888, 23.49111111111111],
		type='linear',
		autorange=True,
		anchor='x',
		overlaying='y',
		side='right'
	)
)

fig = Figure(data=[trace1,trace2], layout=layout)

print py.plot(fig, filename='Generator #1')

i = 0
stream = py.Stream(stream_token)
stream1 = py.Stream('qp5s1voa07')
stream.open()
stream1.open()

def getSerialLine():
	result = ser.readline().strip()
	while result == "":
		result = ser.readline().strip()
	return result

def writeSerialLine(token):
	eventstr = screen.getstr()
	ser.write(token)
	ser.write("\n")
	ser.write(eventstr)
	ser.write("\n")
	ser.flush()

class plotlyThread (threading.Thread):
	def __init__(self, threadID, name, counter):
		threading.Thread.__init__(self)
		self.threadID = threadID
		self.name = name
		self.counter = counter
	def run(self):
		sensor_data = 0
		global aSetpoint
		global aP
		global aI
		global aD
		global aPOut
		global aIOut
		global aDOut
		global aTuning
		global aCont
		global aMode
		global aOver
		global aHAlarm
		global aLAlarm
		global aAlarm
		global aAmbient
		global aHum
		global aTemp
		global aOut
		aTemp = 0
		aOut = 0
		aP = 0
		aI = 0
		aD = 0
		aPOut = 0
		aIOut = 0
		aDOut = 0
		aTuning = 0
		aCont = 0
		aMode = 0
		aOver = 0
		aHAlarm = 0
		aLAlarm = 0
		aAlarm = 0
		aAmbient = 0
		aHum = 0
		aSetpoint = 0
		
		while exitflag == 0:

			old_sensor_data = sensor_data
			sensor_data = getSerialLine()

			if sensor_data == "^":
				aTemp = getSerialLine()
				aOut = getSerialLine()
				x = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
				try:
					stream.write({'x': x, 'y': "%.3f" % (float(aTemp)/10)})
					stream1.write({'x': x, 'y': aOut})
				except:
					pass
			if sensor_data == "#":
				aSetpoint = getSerialLine()                  
			if sensor_data == "}":
				aHum = getSerialLine()                    
			if sensor_data == "$":
				aP = getSerialLine()                   
			if sensor_data == "%":
				aI = getSerialLine()                    
			if sensor_data == "&":
				aD = getSerialLine()          
			if sensor_data == "=":
				aPOut = getSerialLine()
			if sensor_data == "*":
				aIOut = getSerialLine()
			if sensor_data == "(":
				aDOut = getSerialLine()
			if sensor_data == "'":
				aTuning = getSerialLine()
			if sensor_data == "_":
				aCont = getSerialLine()
			if sensor_data == "+":
				aMode = getSerialLine()
			if sensor_data == "-":
				aOver = getSerialLine()
			if sensor_data == ")":
				aHAlarm = getSerialLine()
			if sensor_data == "(":
				aLAlarm = getSerialLine()
			if sensor_data == "]":
				aAlarm  = getSerialLine()
			if sensor_data == "{":
				aAmbient = getSerialLine()
			
			if display == True:
				screen.addstr(1, 16, str(aSetpoint), curses.A_REVERSE)
				screen.addstr(3, 16, "%.3f" % (float(aTemp)/10), curses.A_REVERSE)
				screen.addstr(5, 16, str(aHum), curses.A_REVERSE)
				screen.addstr(7, 16, str(aAmbient), curses.A_REVERSE)
				screen.addstr(9, 16, str(aCont), curses.A_REVERSE)
				screen.addstr(11,16, "N/A", curses.A_REVERSE)
				screen.addstr(13,16, "N/A", curses.A_REVERSE)
				
				screen.addstr(1, 40, str(aP), curses.A_REVERSE)
				screen.addstr(3, 40, str(aI), curses.A_REVERSE)
				screen.addstr(5, 40, str(aD), curses.A_REVERSE)
				screen.addstr(7, 40, "N/A", curses.A_REVERSE)
				screen.addstr(9, 40, "N/A", curses.A_REVERSE)
				screen.addstr(11,40, "N/A", curses.A_REVERSE)
				screen.addstr(13,40, str(aOut), curses.A_REVERSE)
				screen.addstr(15,40, "N/A", curses.A_REVERSE)
				
				screen.addstr(1, 60, str(aHAlarm), curses.A_REVERSE)
				screen.addstr(3, 60, str(aAlarm), curses.A_REVERSE)
				screen.addstr(5, 60, str(aLAlarm), curses.A_REVERSE)
				
				screen.refresh()
			else:
				win.erase()
				win.addstr(0, 1, "Temp:      Target:      Control:")
				win.addstr(0, 6, "%.3f" % (float(aTemp)/10), curses.A_REVERSE)
				win.addstr(0, 19, str(aSetpoint), curses.A_REVERSE)
				win.addstr(0, 33, str(aTuning), curses.A_REVERSE)
				win.refresh()
			

class uiThread (threading.Thread):
	def __init__(self, threadID, name, counter):
		threading.Thread.__init__(self)

	def run(self):
		global exitflag
		global display
		display = False
		exitflag = 0
		x = 0
		
		screen.erase()
		screen.refresh()
		while True:
			screen.clear()
			screen.border(0)
			screen.addstr(2, 4, "***Main Menu***", curses.color_pair(1))
			screen.addstr(4, 8, "Setup", curses.color_pair(1))
			screen.addstr(5, 8, "System", curses.color_pair(1))
			screen.addstr(6, 8, "Display", curses.color_pair(1))
			screen.addstr(7, 8, "Exit", curses.color_pair(1))
			
			screen.addstr(4, 4, "1 - ")
			screen.addstr(5, 4, "2 - ")
			screen.addstr(6, 4, "3 - ")
			screen.addstr(7, 4, "4 - ")
			screen.refresh()

			x = screen.getch()
			if x == ord('1'):
				while True:
					screen.clear()
					screen.border(0)
					screen.addstr(2, 2, "***Setup***")
					screen.addstr(4, 4, "1 - Tuning parameters")
					screen.addstr(5, 4, "2 - Alarms")
					screen.addstr(6, 4, "3 - Back")
					screen.refresh()
		
					x = screen.getch()
					if x == ord('1'):
						while True:
							screen.erase()
							screen.border(0)
							screen.addstr(2, 2, "***Tuning parameters***")
							screen.addstr(4, 4, "1 - Autotune")
							screen.addstr(5, 4, "2 - Setpoint")
							screen.addstr(6, 4, "3 - Proportional gain")
							screen.addstr(7, 4, "4 - Integral gain")
							screen.addstr(8, 4, "5 - Derivative gain")
							screen.addstr(9, 4, "6 - Control window")
							screen.addstr(10, 4, "7 - Back")
							screen.refresh()
							
							x = screen.getch()
							if x == ord('1'):
								while True:
									screen.erase()
									screen.border(0)
									screen.addstr(2, 2, "***Autotune***")
									screen.addstr(4, 4, "1 - Autotune on")
									screen.addstr(5, 4, "2 - Autotune off")
									screen.addstr(6, 4, "3 - Back")
									screen.refresh()
									
									x = screen.getch()
									if x == ord('1'):
										ser.write("'\n")
										ser.write("1.0")
										ser.write("\n")
										break
									if x == ord('2'):
										ser.write("'\n")
										ser.write("0.03")
										ser.write("\n")
										break
									if x == ord('3'):
										x = 0
										break
									screen.nodelay(1)
							elif x == ord('2'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new setpoint:")
								screen.refresh()
								writeSerialLine("#")
							elif x == ord('3'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new proportional gain:")
								screen.refresh()
								writeSerialLine("$")
							elif x == ord('4'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new integral gain:")
								screen.refresh()
								writeSerialLine("%")
							elif x == ord('5'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new derivative gain:")
								screen.refresh()
								writeSerialLine("&")
							elif x == ord('6'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new control window:")
								screen.refresh()
								x = screen.getch()
							elif x == ord('7'):
								x = 0
								break
					elif x == ord('2'):
						while True:
							screen.erase()
							screen.border(0)
							screen.addstr(2, 2, "***Alarms***")
							screen.addstr(4, 4, "1 - High alarm")
							screen.addstr(5, 4, "2 - Low alarm")
							screen.addstr(6, 4, "3 - Alarm enable/disable ")
							screen.addstr(7, 4, "4 - Alarm test")
							screen.addstr(8, 4, "5 - Back")
							screen.refresh()
							
							x = screen.getch()
							if x == ord('1'):
								screen.erase()
								screen.border(0)
								screen.addstr("Enter new high alarm temperature")
								screen.refresh()
								x = screen.getch()
							elif x == ord('2'):
								screen.erase()
								screen.border(0)
								screen.addstr("Enter new low alarm temperature")
								screen.refresh()
								x = screen.getch()
							elif x == ord('3'):
								screen.erase()
								screen.border(0)
								screen.addstr("ALARM ENABLE DISABLE")
								screen.refresh()
								x = screen.getch()
							elif x == ord('4'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Testing, press any key to abort.")
								screen.refresh()
								ser.write("aon\n")
								eventchr = screen.getch()
								ser.write("aoff\n")
							elif x == ord('5'):
								x = 0
								break
					elif x == ord('3'):
						x = 0
						break
			elif x == ord('2'):
				while True:
					screen.erase()
					screen.border(0)
					screen.addstr(2, 2, "***System***")
					screen.addstr(4, 4, "1 - Auto update")
					screen.addstr(5, 4, "2 - Networking")
					screen.addstr(6, 4, "3 - Preferences")
					screen.addstr(7, 4, "4 - Back")
					screen.refresh()
		
					x = screen.getch()
					if x == ord('1'):
						screen.erase()
						screen.border(0)          
						screen.addstr(2,2, "Feature not yet implemented")
						screen.refresh()
						x = screen.getch()
					elif x == ord('2'):
						while True:
							screen.erase()
							screen.border(0)
							screen.addstr(2, 2, "***Wireless***")
							screen.addstr(4, 4, "1 - Set SSID")
							screen.addstr(5, 4, "2 - Set gateway")
							screen.addstr(6, 4, "3 - Set password")
							screen.addstr(7, 4, "4 - Back")
							screen.refresh()
							
							x = screen.getch()
							if x == ord('1'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new SSID:")
								screen.refresh()
								x = screen.getch()
							elif x == ord('2'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new gateway:")
								screen.refresh()
								x = screen.getch()
							elif x == ord('3'):
								screen.erase()
								screen.border(0)
								screen.addstr(2, 2, "Enter new password:")
								screen.refresh()
								x = screen.getch()
							elif x == ord('4'):
								x = 0
								break
					elif x == ord('3'):
						screen.erase()
						screen.border(0)          
						screen.addstr(2,2, "Feature not yet implemented")
						screen.refresh()
						x = screen.getch()
					elif x == ord('4'):
						x = 0
						break
			elif x == ord('3'):
				display = True
				win.erase()
				win.refresh()
				screen.erase()
				screen.border(0)
				screen.addstr(1, 2, "Setpoint.....:")
				screen.addstr(3, 2, "Temp.........:")
				screen.addstr(5, 2, "Humidity.....:")
				screen.addstr(7, 2, "Ambient temp.:")
				screen.addstr(9, 2, "Relay state..:")
				screen.addstr(11, 2, "Override.....:")
				screen.addstr(13, 2, "System status:")
				
				screen.addstr(1, 32, "Px.....:")
				screen.addstr(3, 32, "Ix.....:")
				screen.addstr(5, 32, "Dx.....:")
				screen.addstr(7, 32, "P-Out..:")
				screen.addstr(9, 32, "I-Out..:")
				screen.addstr(11, 32, "D-Out..:")
				screen.addstr(13, 32, "Output.:")
				screen.addstr(15, 32, "Setting:")
				
				screen.addstr(1, 52, "H-Alarm:")
				screen.addstr(3, 52, "Status.:")
				screen.addstr(5, 52, "L-Alarm:")
				screen.addstr(7, 52, "Status.:")

				x = screen.getch()
				x = 0
				display = False
			elif x == ord('4'):
				x = 0
				break
		
		exitflag = 1


		
# Create new threads
thread1 = plotlyThread(1, "Plotly-thread", 1)
thread2 = uiThread(2, "ui-thread", 2)

# Start new Threads
thread2.start()
thread1.start()
thread2.join()
thread1.join()
sys.exit(0)