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
    1.1. $ ./sandboxer.sh
2. In terminal 2:
    2.1. $ ./test1.sh
