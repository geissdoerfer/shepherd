#!/bin/bash
# sudo python3 /opt/shepherd/software/python-package/shepherd/cli.py -vv record --mode load -l 10
sudo shepherd-sheep -vv record --mode load -l $1
