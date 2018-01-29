#!/bin/bash
echo hello
# exit 0

for ((i = 0; i < 3; i++)); do
	echo hello $i
	sleep 1
done
