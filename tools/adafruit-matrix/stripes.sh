#!/bin/bash

function red() {
	echo -en '\x00\x00\xff'
}

function green() {
	echo -en '\x00\xff\x00'
}

function blue() {
	echo -en '\xff\x00\x00'
}

for i in `seq 0 $(($1 - 1))`; do
	for j in `seq 0 $(($2 - 1))`; do
		res=$((j - i))
		if [ $res -lt 0 ]; then
			res=$(((-1) * $res));
		fi
		color=$(( $res % 3))
		case $color in
			0)
				red ;;
			1)
				green ;;
			2)
				blue ;;
		esac
	done
done
