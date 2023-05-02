#!/bin/zsh

port=$1

gcc -o server server.c -lssl -lcrypto

./server $port