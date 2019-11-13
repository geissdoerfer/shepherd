#!/bin/bash
cd /opt/shepherd/software/python-package
suffix=b
number=1
sudo python3 -m shepherd.cli -vv emulate --virtcap --force -o /var/shepherd/emulation/emu.$number$suffix.h5 /var/shepherd/recordings/solv3_$number.h5
# sudo python3 -m shepherd.cli "$@"
