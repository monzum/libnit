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
1. In terminal 1:
    1.1. $ source sandboxer.sh
    1.2. $ cd sandbox
    1.3. $ python smart_shim_proxy.py
2. In terminal 2:
    2.1. $ make
    2.2. $ source load_shim_proxy.sh
    2.3. Run any network application, for example 'wget'
       $ wget www.google.com
