#!/bin/zsh

host=$1
port=$2

gcc -o client client.c

./client $host $port