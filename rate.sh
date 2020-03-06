#!/bin/bash

txbytes_1=$(ifconfig $1 | awk '/TX bytes/ {print $6}' | cut -d ":" -f2) || exit
sleep 10
txbytes_2=$(ifconfig $1 | awk '/TX bytes/ {print $6}' | cut -d ":" -f2) || exit
#rate=(($txbytes_2 - $txbytes_1) / 10) * 8
rate_1=`expr $txbytes_2 - $txbytes_1`
rate_2=`expr $rate_1 \* 8`
rate_3=`expr $rate_2 / 10`
calc=$(echo $rate_3 1000000000 | awk '{ printf "%f", $1 / $2 }')
echo "TX rate: $calc Gbps"

