Patching LCDproc for PiFace Control and Display
===============================================
Changed source files are in `lcdproc/`. Check the patch for other changes.

Applying the patch
------------------
Go to [here](http://lcdproc.cvs.sourceforge.net/) and click `Download GNU tarball`
to get the latest development version of LCDproc.

Untar and autogen some configuration files:

    $ tar xf lcdproc.tar.gz
    $ cd lcdproc/lcdproc/
    $ sh autogen.sh

Then download the patch from this repository and apply it with:

    $ wget https://raw.github.com/tompreston/pifacecad-lcdproc/master/hd44780-pifacecad.patch
    $ patch -p1 < hd44780-pifacecad.h

Building and Running LCDproc
----------------------------
Configure the build to enable the hd44780 drivers and then `make` with:

    $ ./configure --enable-drivers=hd44780
    $ make

Run the server with:

    $ server/LCDd -c LCDd.conf
