 _     _   _             _     _
| |   (_) | |           (_) _| |_
| |    _  | |_   _____   _ |_   _|
| |   | | |   \ |     | | |  | |_
| |_  | | | ()| | ( ) | | |  | | \
|___| |_| |___/ |_| |_| |_|  \___/

      Monzur Muhammad 
      monzum@cs.washington.edu



Introduction:
-------------
Libnit is an interpostion mechanism that allows users to
interpose on network calls on Linux operating system in 
order to manipulate the network calls for legacy applications
without having to modify the application. This project was 
originally developed in conjunction with the AFFIX project
to allow the use of AFFIXs with legacy applications. However
Libnit can be used for other functionalities. The Libnit
itself is a simple that interposes on network POSIX calls
and redirects them to a proxy of choice.



Test Usage with AFFIXs:
-----------------------
1. Create a new directory and copy over all the files in the
   lind/ and repy/ directory. 

2. Copy over the two files 
   smart_shim_proxy.py and posix_call_definition.py in the 
   new directory. 

3. Open up a new terminal and change to the new directory
   that was created. Then run the file smart_shim_proxy.py
   $ python smart_shim_proxy.py

4. From the first terminal run the script load_shim_proxy.sh
   using the 'source' command.
   $ source load_shim_proxy.sh

5. Run any network application, for example 'wget'
   $ wget www.google.com



Development usage:
------------------
1. In terminal 1:
    1.1. $ ./shim_sandboxer.sh
2. In terminal 2:
    2.1. $ ./test_sandboxer.sh
