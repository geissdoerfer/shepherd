#!/bin/bash
cd /opt/shepherd/software/python-package
sudo python3 -m shepherd.cli -vv emulate --emulation-type virtcap -l 1 -o /var/shepherd/emulation/emu.0.h5 /var/shepherd/recordings/rec.40.good.h5
