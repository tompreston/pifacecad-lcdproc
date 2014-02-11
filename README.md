LCDproc driver for PiFace Control and Display
=============================================
11/02/2014 - The patch has been accepted into the LCDproc dev branch
(http://lists.omnipotent.net/pipermail/lcdproc/2014-February/014217.html)

Download the latest version of LCDproc from [here](http://lcdproc.cvs.sourceforge.net/)
(click `Download GNU tarball`).

Build:

    $ tar xvf lcdproc.tar.gz
    $ cd lcdproc/lcdproc
    $ sh ./autogen.sh
    $ ./configure --enable-drivers=hd44780
    $ make

Load the SPI module: http://piface.github.io/pifacecommon/installation.html#enable-the-spi-module

Download the config:

    $ curl https://raw2.github.com/tompreston/pifacecad-lcdproc/master/lcdproc/LCDd.conf > LCDd.conf

Run the server in one terminal session (ssh/tmux/background):

    $ server/LCDd -c LCDd.conf

Run the client in another terminal session to test:

    $ clients/lcdproc/lcdproc -f C M T

XBMC
====
Once the server is running you can use the [LCDproc XBMC addon](http://wiki.xbmc.org/index.php?title=Add-on:XBMC_LCDproc) to display XBMC information on PiFace Control and Display.


Patching LCDproc for PiFace Control and Display
===============================================
**This patch has already been included into LCDproc and is now out of date,
further changes to the code were made by Markus Dolze before being added to
LCDproc (See
http://lists.omnipotent.net/pipermail/lcdproc/2014-February/014217.html).**