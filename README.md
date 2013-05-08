webserver
=========

Web Server for CS3214, project 5

This is an implementation of the most basic web server possible.  It is mostly
inspired by the Tiny Web Server given by the CS App textbook, but more robust.

Group Members:
    Pat Lewis       (patl1)
    Ryan Merkel     (orionf22)


The basic idea is as follows:
    The server is set up.  A socket is created, and its options set.  It is then
    bound to the TCP protocol and a listener is set to it. In an infinite loop
    (because it should listen forever), the 'accept' function is continuously
    used to accept traffic.  The traffic is set to a duplicate socket so that we
    can keep the original open to continue listening for traffic.  This accepted
    traffic is submitted to the threadpool.  Since we can submit multiple 
    threads to the pool, this allows multiple client support.  

Widget Support:
    Theoretically, the widgets should work.  We have not done much testing on 
    them.  To run, you will need to specify the port (i.e. $sysstatd -p 22123).

Other stuff:
    The 'loadavg' and 'meminfo' stuff should also work, but it is slightly 
    untested. We are very hopeful.

