#!/bin/bash

ctrl_c() {
	kill $TROLL_PID
	echo "Killed troll"
}

trap ctrl_c SIGINT

if [ $# -ne 2 ]; then
	echo $0 "<client-host> <listen-port>"
	exit
fi

./troll -S localhost -b 6661 -C $1 -a $(expr $2 + 1) 6662 -x 0 -g 25 -t &
TROLL_PID=$!
./tcpd $2
kill $TROLL_PID
