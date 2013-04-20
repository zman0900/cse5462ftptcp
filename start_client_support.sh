#!/bin/bash

ctrl_c() {
	kill $TROLL_PID
	echo "Killed troll"
}

trap ctrl_c SIGINT

if [ $# -ne 2 ]; then
	echo $0 "<server-host> <server-port>"
	exit
fi

# Clean
#./troll -S localhost -b 6664 -C $1 -a $2 6665 -x 0 -t &
# Garbling
#./troll -S localhost -b 6664 -C $1 -a $2 6665 -x 0 -g 25 -t &
# Reording
./troll -S localhost -b 6664 -C $1 -a $2 6665 -x 0 -se50 -t &
# Reording and garbling
#./troll -S localhost -b 6664 -C $1 -a $2 6665 -x 0 -se50 -g 25 -t &
TROLL_PID=$!
./tcpd $2 $1
kill $TROLL_PID
