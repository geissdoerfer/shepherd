Command-line tools
==================

Shepherd comes with two command line utilities:

`Shepherd-herd`_ is the command line utility for remotely controlling a group of shepherd nodes.
This is the key user interface to shepherd.
The pure-python package is installed on the user's local machine and sends commands to the shepherd nodes over *ssh*.

`Shepherd-sheep`_ is the command line utility for locally controlling a single shepherd node.
Depending on your use-case you may not even need to directly interact with it!

Shepherd-herd
-------------

.. click:: shepherd_herd:cli
   :prog: shepherd-herd
   :show-nested:

Examples
********

Here are some examples showing how to use shepherd-herd.

Shepherd-sheep
--------------

.. click:: shepherd.cli:cli
   :prog: shepherd-sheep
   :show-nested:


Examples
********

Here are some examples showing how to use shepherd-sheep.