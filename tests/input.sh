#!/bin/bash

#arr=("cmd1" "cmd2" "cmd3" "cmd4" "cmd5")
arr=("cmd1" "cmd2" "cmd3" "{" "cmd4" "cmd5" "cmd6" "cmd7" "}")
#arr=("{" "cmd1" "cmd2" "{" "cmd3" "cmd4" "}" "cmd5" "cmd6" "}")
#arr=("cmd1" "cmd2" "cmd3" "{" "cmd4" "cmd5" "cmd6" "cmd7")
for (( i = 0; i < ${#arr[@]}; i++ )); do
	echo ${arr[$i]}
	#sleep 0.5
done