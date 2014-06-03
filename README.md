Apache module for setting socket options
==========

[![License](http://img.shields.io/badge/license-Apache%201.1-green.svg)](http://www.apache.org/licenses/LICENSE-1.1)

This module provides a set socket options.
The following socket option lists are available in this module:

* TCP_DEFER_ACCEPT
* SO_SNDTIMEO
* SO_RCVTIMEO
* SO_SNDBUF
* SO_RCVBUF

This program has been rewritten.([original](http://arctic.org/~dean/mod_sockopts/libapache-mod-sockopts-1.0/mod_sockopts.c))

## Dependencies
* Apache1
* Apxs

## Installation

```
shell> git clone git://github.com/vozlt/mod_sockopts.git
```

```
shell> cd mod_sockopts
```

```
shell> apxs -iac mod_sockotps.c
```

## Configuration(httpd.conf)

```ApacheConf
LoadModule sockopts_module    libexec/mod_sockopts.so

AddModule mod_sockopts.c

<IfModule mod_sockopts.c>
    # TCP_DEFER_ACCEPT
    SoTcpDeferAccept    20

    # SO_SNDTIMEO - not effective(socket is not closed and be continued the data transfering)
    # SoSoSndTimeo      5

    # SO_RCVTIMEO - not effective(socket is not closed and be continued the data transfering)
    # SoSoRcvTimeo      5

    # SO_SNDBUF
    # SoSoSndBuf        512

    # SO_RCVBUF
    # SoSoRcvBuf        512
</IfModule>
```

##### SoTcpDeferAccept
````ApacheConf
# Syntax
SoTcpDeferAccept        {SECONDS}
````

```
shell> vi linux-2.6.32-431.el6/net/ipv4/tcp.c
```
````C
331 /* Convert seconds to retransmits based on initial and max timeout */
332 static u8 secs_to_retrans(int seconds, int timeout, int rto_max)
333 {
334     u8 res = 0;
335 
336     if (seconds > 0) {
337         int period = timeout;
338 
339         res = 1;
340         while (seconds > period && res < 255) {
341             res++;
342             timeout <<= 1;
343             if (timeout > rto_max)
344                 timeout = rto_max;
345             period += timeout;
346         }
347     }
348     return res;
349 }
.
.
.
2104 /*
2105  *  Socket option code for TCP.
2106  */
2107 static int do_tcp_setsockopt(struct sock *sk, int level,
2108         int optname, char __user *optval, unsigned int optlen)
2109 {
.
.
.
2252     case TCP_DEFER_ACCEPT:
2253         /* Translate value in seconds to number of retransmits */
2254         icsk->icsk_accept_queue.rskq_defer_accept =
2255             secs_to_retrans(val, TCP_TIMEOUT_INIT / HZ,
2256                     TCP_RTO_MAX / HZ);
2257         break;
````

## Author
YoungJoo.Kim <[http://superlinuxer.com](http://superlinuxer.com)>
