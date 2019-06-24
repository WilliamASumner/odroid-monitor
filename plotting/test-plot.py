# test-plot
import matplotlib.pyplot as plt
import numpy as np
import sys



#data = genfromtxt("logs/kmean/" + sys.argv[1],
'''
data = np.genfromtxt("logs/kmean/mon-big-local.log",
       delimiter=';',
       skip_header=1,
       names=None,
       filling_values=0,
       dtype=dt,
       usecols=(0,1,2,3,4,5,6))
'''

filename = "logs/kmean/mon-big-local.log"
data_mat = []
maxlen = 0
with open(filename) as f:
    for line in f:
        line = line.replace(';\n','')
        data_row = line.split(";")
        linelen = len(data_row)
        if linelen > maxlen:
            maxlen = linelen
            for i in range(len(data_mat)): # pad previous entries
                data_mat[i] += ['0'] * (maxlen-len(data_mat[i]))
        elif linelen < maxlen: # if less than current max, pad it
            data_row += ['0'] *(maxlen-linelen)
        data_mat.append(data_row)
data_mat_tup = []
for l in data_mat:
    data_mat_tup.append(tuple(l))

max_threads = maxlen-5


data_type_mat = [(data_mat[0][i],np.float64) for i in range(5)] + [('TID'+ str(j//2),np.float64) if j % 2 == 0 else ('CID'+str(j//2-1),np.int16) for j in range(max_threads)]
print(data_type_mat)
dt = np.dtype(data_type_mat)
headers = data_mat_tup[0]
del data_mat_tup[0]
del data_mat[0]

data = np.array(data_mat_tup,dtype=dt)

timestamps = data['Timestamp']
timestamps -= timestamps[0]
end_time = max(timestamps)
num_ticks = 5
timestamp_ticks = np.arange(0,end_time,end_time/num_ticks)

unqiue_threads,indices = np.unique(data['TID0'],return_inverse=True)




fig, axs = plt.subplots(2, 1, figsize=(8, 8))
axs[0].set_ylabel('Power (W)')
axs[0].set_xlabel('Time(us)')
axs[0].set_title('Power used by A15 Cluster over Time')
axs[0].set_xticks(timestamp_ticks)
axs[0].xaxis.get_major_formatter().set_powerlimits((0, 1))
plt.setp(axs[0].get_xticklabels(), rotation=30, horizontalalignment='right')

axs[1].set_ylabel('Core ID')
axs[1].set_xlabel('Time(us)')
axs[1].set_title('Core ID of Thread 1 over Time')
axs[1].set_xticks(timestamp_ticks)
axs[1].xaxis.get_major_formatter().set_powerlimits((0, 1))
axs[1].tick_params(labelcolor='black', labelsize='medium', width=3)
plt.setp(axs[1].get_xticklabels(), rotation=30, horizontalalignment='right')

fig.tight_layout() # Or equivalently,  "plt.tight_layout()"


axs[0].plot(data['Timestamp'], data['SENS_A15'])
axs[1].plot(data['Timestamp'],data['CID1'],'b-');


plt.show()
