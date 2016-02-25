#!/bin/bash
file=$1
L=2
for((mark=1;$mark<5;mark++));
do
    echo L:$L >> $file
    cat log_9_${L}.out|grep -a "MSG-CRT">>$file
    cat log_9_${L}.out|grep -a "RCV-RCH">>$file
    L=$[L*2]
done
