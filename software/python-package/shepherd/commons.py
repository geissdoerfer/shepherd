# -*- coding: utf-8 -*-

"""
shepherd.commons
~~~~~
Defines details of the data exchange protocol between PRU0 and the python code.
The various parameters need to be the same on both sides. Refer to the
corresponding implementation in `software/firmware/include/commons.h`

:copyright: (c) 2019 Networked Embedded Systems Lab, TU Dresden.
:license: MIT, see LICENSE for more details.
"""
MAX_GPIO_EVT_PER_BUFFER = 16384

MSG_DEP_ERROR = 0
MSG_DEP_BUF_FROM_HOST = 1
MSG_DEP_BUF_FROM_PRU = 2

MSG_DEP_ERR_INCMPLT = 3
MSG_DEP_ERR_INVLDCMD = 4
MSG_DEP_ERR_NOFREEBUF = 5
MSG_DEP_DBG_PRINT = 6

MSG_DEP_DBG_ADC = 0xF0
MSG_DEP_DBG_DAC = 0xF1
