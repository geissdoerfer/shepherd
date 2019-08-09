#!/bin/bash
cd /opt/shepherd/software/firmware
sudo -E make
rmmod shepherd
modprobe shepherd