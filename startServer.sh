#!/bin/zsh

port=$1

gcc -o server server.c

./server $port