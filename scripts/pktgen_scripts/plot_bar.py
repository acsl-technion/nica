# a stacked bar plot with errorbars
import numpy as np
import matplotlib.pyplot as plt
import argparse
from scipy import stats
import math


def avg(numbers):
    return float(sum(numbers)) / max(len(numbers), 1)


parser = argparse.ArgumentParser()
parser.add_argument('-n', '--name', help="graph name", required=True)
parser.add_argument('-ib', '--baseline_input', help="baseline input data - FILE", required=True)
parser.add_argument('-ii', '--ikernel_input', help="ikernel input data - FILE", required=True)
parser.add_argument('-x', '--x_axis', help="X axis name", required=True)
parser.add_argument('-y', '--y_axis', help="Y axis name", required=True)
parser.add_argument('-o', '--output', help="graph jpg name", required=True)

args = parser.parse_args()

x_b = []
y_b = []
conf_int_b = []
x_i = []
y_i = []
conf_int_i = []

with open(args.baseline_input) as fp:
        for line in fp:
                temp = line.split(" ")
                temp.pop()
                x_b.append(float(temp[0]))
                temp.pop(0)
                data = []
                for val in temp:
                        data.append(float(val))
                mean, sigma = np.mean(data), np.std(data)
                conf_int = stats.t.interval(0.95,len(data)-1,loc=mean,scale=sigma/math.sqrt(len(data)))
                y_b.append(avg(data))
                conf_int_b.append(conf_int)
fp.close()

with open(args.ikernel_input) as fp:
        for line in fp:
                temp = line.split(" ")
                temp.pop()
                x_i.append(float(temp[0]))
                temp.pop(0)
                data = []
                for val in temp:
                        data.append(float(val))
                mean, sigma = np.mean(data), np.std(data)
                conf_int = stats.t.interval(0.95,len(data)-1,loc=mean,scale=sigma/math.sqrt(len(data)))
                y_i.append(avg(data))
                conf_int_i.append(conf_int)
fp.close()

fig = plt.figure()
N = len(x_b) #or x_i
ind = np.arange(N)    # the x locations for the groups
width = 3      # the width of the bars: can also be len(x) sequence

x_axis_fixed = []
for val in x_i: #or x_i
        x_axis_fixed.append(val+(width/2.0))

i=0
for xe,ye in zip(x_axis_fixed,conf_int_b): 
	new_ye = []
	for val in ye:
		new_ye.append(val + y_i[i])
	i+=1
	plt.plot([xe] * len(new_ye), new_ye, 'g-')
for xe,ye in zip(x_axis_fixed,conf_int_i):
        plt.plot([xe] * len(ye), ye,'g-')


N = len(x_b) #or x_i
ind = np.arange(N)    # the x locations for the groups
width = 3      # the width of the bars: can also be len(x) sequence

p1=plt.bar(x_i,y_i,width,color="r")
p2=plt.bar(x_b,y_b,width,bottom=y_i)

plt.ylabel(args.y_axis)
plt.xlabel(args.y_axis)
plt.title(args.name)
plt.xticks(x_axis_fixed, x_b) #or x_i
plt.yticks(np.arange(0, 81, 10))
plt.legend((p1[0], p2[0]), ('ikernel', 'baseline'))

fig.savefig(args.output+'.jpg')
#plt.show()

