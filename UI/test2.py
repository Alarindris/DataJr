#!/usr/bin/python

import threading
import plotly.plotly as py
from plotly.graph_objs import Scatter, Layout, Figure
import datetime
import time
import serial
import curses
import atexit
import sys

def exit():
    curses.nocbreak()
    screenstd.keypad(0)
    curses.echo()
    curses.endwin()
    print "Exit successful."
atexit.register(exit)

screenWidth = 80
screenHeight = 24
screenstd = curses.initscr()
curses.start_color()
curses.cbreak()
screenstd.keypad(1)
curses.curs_set(0)
h, w = screenstd.getmaxyx()
win = curses.newwin(2,w,0,0)
screen = curses.newwin((h-2),w,2,0)
 
ser = serial.Serial('/dev/ttyAMA0', 115200, timeout=3)
 
username = 'Alarindris'
api_key = '7775k8lmv3'
stream_token = 'v1qy8o8a39'
py.sign_in(username, api_key)
 
trace1 = Scatter(
    x=[],
    y=[],
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
    title='Fishtank temperature plot'
)
 
fig = Figure(data=[trace1,trace2], layout=layout)
 
print py.plot(fig, filename='Fishtank temp')
 
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
    

class plotlyThread (threading.Thread):
    def __init__(self, threadID, name, counter):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name
        self.counter = counter
    def run(self):
        while exitflag == 0:
            '''event = screen.getch(1,0)
            if event > 87:
                if(event == 83 or event == 115): #S or s
                    curses.curs_set(1)
                    screen.addstr(1,0,"New setpoint?")
                    screen.nodelay(0)
                    eventstr = screen.getstr()
                    ser.write("s\n")
                    ser.write(eventstr + "\n")
                elif(event == 88 or event == 120): #X or x
                    stop()
                    thread.exit()
                    sys.exit(0)
                elif(event == 65 or event == 97): #A or a
                    screen.addstr(1,0,"Autotune enable (y or n)?")
                    screen.nodelay(0)
                    eventstr = screen.getstr()
                    ser.write("a\n")
                    ser.write(eventstr + "\n")
                elif(event == 80 or event == 112): #P or p
                    curses.curs_set(1)
                    screen.addstr(1,0,"New proportional coefficient?")
                    screen.nodelay(0)
                    eventstr = screen.getstr()
                    ser.write("p\n")
                    ser.write(eventstr + "\n")
                elif(event == 73 or event == 105): #I or i
                    curses.curs_set(1)
                    screen.addstr(1,0,"New integral coefficient?")
                    screen.nodelay(0)
                    eventstr = screen.getstr()
                    ser.write("i\n")
                    ser.write(eventstr + "\n")
                elif(event == 68 or event == 100): #D or d
                    curses.curs_set(1)
                    screen.addstr(1,0,"New derivative coefficient?")
                    screen.nodelay(0)
                    eventstr = screen.getstr()
                    ser.write("d\n")
                    ser.write(eventstr + "\n")
            
                #screen.erase()
        
            #curses.curs_set(0)
            #screen.nodelay(1)'''
            
            sensor_data = getSerialLine()
            if sensor_data == "#":
                sensor_data = getSerialLine()
                win.erase()
                win.addstr(0,2, sensor_data, curses.A_REVERSE)
                win.refresh()
            if sensor_data == "^":
                sensor_data = getSerialLine()
                sensor1_data = getSerialLine()
                #screen.addstr(3, 39, "*Sent*")
                #screen.addstr(4, 40, str(sensor_data), curses.A_REVERSE)
                #screen.addstr(5, 40, str(sensor1_data),curses.A_REVERSE)
                x=datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
                stream.write({'x': x, 'y': sensor_data})
                stream1.write({'x': x, 'y': sensor1_data})


class uiThread (threading.Thread):
    def __init__(self, threadID, name, counter):
        threading.Thread.__init__(self)
        self.threadID = threadID
        self.name = name
        self.counter = counter
    def run(self):
        global exitflag
        exitflag = 0
        x = 0
        while True:
            screen.erase()
            screen.border(0)
            screen.addstr(2, 2, "***Main Menu***")
            screen.addstr(4, 4, "1 - Setup")
            screen.addstr(5, 4, "2 - System")
            screen.addstr(6, 4, "3 - Display")
            screen.addstr(7, 4, "4 - Exit")
            screen.refresh()

            x = screen.getch()
            if x == ord('1'):
                while True:
                    screen.erase()
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
                                        screen.addstr(8,4,"Autotune enable (y or n)?")
                                        screen.refresh()
                                        screen.nodelay(0)
                                        eventstr = screen.getstr()
                                        ser.write("a\n")
                                        ser.write(eventstr + "\n")
                                        break
                                    if x == ord('2'):
                                        break;
                                    if x == ord('3'):
                                        x = 0
                                        break
                                    screen.nodelay(1)
                            elif x == ord('2'):
                                screen.erase()
                                screen.border(0)
                                screen.addstr(2, 2, "Enter new setpoint:")
                                screen.refresh()
                                eventstr = screen.getstr()
                                ser.write("s\n")
                                ser.write(eventstr + "\n")
                            elif x == ord('3'):
                                screen.erase()
                                screen.border(0)
                                screen.addstr(2, 2, "Enter new proportional gain:")
                                screen.refresh()
                                x = screen.getch()
                            elif x == ord('4'):
                                screen.erase()
                                screen.border(0)
                                screen.addstr(2, 2, "Enter new integral gain:")
                                screen.refresh()
                                x = screen.getch()
                            elif x == ord('5'):
                                screen.erase()
                                screen.border(0)
                                screen.addstr(2, 2, "Enter new derivative gain:")
                                screen.refresh()
                                x = screen.getch()
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
                x = 0
                break
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