#!/bin/sh
# You need to run this as a super user to use minix-service.
# LCOM facilities will help you later on as a regular user.
clang bitwise.c rtc.c main.c -static -lsys -ltimers -o lab1 && \
minix-service run $(pwd)/lab1
