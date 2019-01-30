#!/usr/bin/env python2

from collections import defaultdict

import sys

# TSC ticks in a second, based on a sleep() test in rt-test
cal=None

last={}


sum=defaultdict(int)
count=defaultdict(int)

histo=defaultdict(lambda: defaultdict(int))

# Round bucket down
def bucket(x):
    return int(x*100000.0)/100.0

while True:
    #Read in a line from the input file
    line = sys.stdin.readline()
    if not line:
        break
    #Remove trailing characters
    line = line.rstrip()

    #Split the 2 number line at commas and assign to variables
    thread, t = line.split(',')
    #Convert value to int
    t=int(t)

    if thread == 'cal':
	# Special value: cycles in 1s
	    cal = t
	    print ('Calibration: %d' % cal)
    elif thread not in last:
	    last[thread] = t
    else:
        #Percentage of 1 second
	    duration = float(t-last[thread])/cal

        #move onto next thread
	    last[thread] = t

	    # Prepare for averages
        sum[thread] = sum[thread] + duration
	    count[thread] = count[thread] + 1

	    histo[thread][bucket(duration)] = histo[thread][bucket(duration)] + 1


# Make averages

avg={}
#f takes on the values 1, 2, 3
for f in sum.keys():
     avg[f] = sum[f]/count[f]

max = 0
for f in sorted(histo.keys()):
    for g in sorted(histo[f].keys()):
	    if histo[f][g] > max:
	        max = histo[f][g]

for f in sorted(histo.keys()):
    if f in avg:
        print '%s,%f' % (f, avg[f])
    for g in sorted(histo[f].keys()):
	    print '%s,%f,%d,%s' % (f, g, histo[f][g], '*' * (histo[f][g]*80/max))
     
