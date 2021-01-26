#!/bin/bash

echo -n "buffer size: "
read buff_size
#buff_size=1024

#echo -n "number of CPU cores: "
#read num_core
num_cores=1

test_time=60
#echo -n "number of connections: "
#read num_connection

echo -n "benchmark type[open/close loop]?: "
read test_mode

for j in $(seq 0 8)
do
    num_connection=`echo "2^$j" | bc `

    echo "Testing RTT for $num_connection connections..."

    ./server    --num_cores=$num_cores \
                --size=$buff_size \
                --time=$test_time \
                --num_client=$num_connection \
                --test_mode=$test_mode

    echo "Test done"
done