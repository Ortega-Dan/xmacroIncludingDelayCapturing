This is an improved version wich also records input delays.\
(The version provided by Debian based distributions doesn't record delays\
That version information is in:\
http://xmacro.sourceforge.net/)

This Code site is:\
https://download.sarine.nl/xmacro/Description.html

Latest version 0.4.6 (same code as here) can be downloaded (and was imported) from:\
http://download.sarine.nl/xmacro/xmacro-0.4.6.tar.gz

Requirments:
* C compiler
* X11 record extention
* X11 test extention

You can install those dependencies on Debian based distributions with:\
sudo apt install gcc make libxtst-dev

Then just run:\
make

and then copy:\
sudo cp xmacrorec2 /usr/local/bin/.\
sudo cp xmacroplay /usr/local/bin/.


How to use it then:

(The following correct and easy instructions were previously found at: https://sadasant.com/2014/04/05/xmacro-tasks-automation.html Seems like the site was changed ... anyway ...)

Essentially, xmacro records your pressed keys and saves them to a file. To run it, see the line below:

xmacrorec2 > myrecording

Once you call it, first you'll have to pick a quit-key ([Esc] is fine), then whatever you do will be recorded: moving your mouse, clicking, pressing any key, etc. Do some stuff, then press the quit-key, and you'll be able to replicate that behavior with the following command:

xmacroplay "$DISPLAY" < myrecording

(end of quote)
