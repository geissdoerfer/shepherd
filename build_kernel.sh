#!/bin/bash
cd /opt/shepherd/software/kernel-module/src
make install
rmmod shepherd
modprobe shepherd