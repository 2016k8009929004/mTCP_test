import math
import os
import sys

with open(str(sys.argv[1]), "r") as f:
    total_put = 0.0
    total_get = 0.0
    total_scan = 0.0
    count = 0
    for line in f:
        res = line.split()
        if len(res) == 6:
           total_put += float(res[1])
           total_get += float(res[3])
           total_scan += float(res[5])
           count += 1
    f.close()
    avg_put = round(total_put/float(count), 4)
    avg_get = round(total_get/float(count), 4)
    avg_scan = round(total_scan/float(count), 4)

print('put avg: %.4f, get avg: %.4f, scan avg: %.4f' % (avg_put, avg_get, avg_scan))