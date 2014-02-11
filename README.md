LCDproc driver for PiFace Control and Display
=============================================
11/02/2014 - The patch has been merged into the LCDproc dev branch. There
have been changes made to the way inputs work by [Markus Dolze](http://lists.omnipotent.net/pipermail/lcdproc/2014-February/014217.html), before the merge.
The code in this repository does not reflect these changes.

Install
-------
First, enable SPI:

    $ sudo raspi-config

Navigate to `Advanced Options`, `SPI` and select `yes`. Reboot.

Download the latest version of LCDproc from [here](http://lcdproc.cvs.sourceforge.net/)
(click `Download GNU tarball`).

Build:

    $ tar xvf lcdproc.tar.gz
    $ cd lcdproc/lcdproc
    $ sh ./autogen.sh
    $ ./configure --enable-drivers=hd44780
    $ make

Download the config:

    $ curl https://raw2.github.com/tompreston/pifacecad-lcdproc/master/lcdproc/LCDd.conf > LCDd.conf

Run the server as root in one terminal session (ssh/tmux/background):

    $ sudo server/LCDd -c LCDd.conf

Run the client in another terminal session to test:

    $ clients/lcdproc/lcdproc -f C M T

XBMC
====
Once the server is running you can use the [LCDproc XBMC addon](http://wiki.xbmc.org/index.php?title=Add-on:XBMC_LCDproc) to display XBMC information on PiFace Control and Display.
