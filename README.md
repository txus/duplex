# duplex [![Build Status](https://secure.travis-ci.org/txus/duplex.png)](http://travis-ci.org/txus/duplex)

Duplex is a simple TCP proxy that, apart from forwarding the incoming traffic
to a remote host, transparently replays it to a third host, normally for
testing purposes.

Based on ![Krzystztof Klis' proxy library](https://github.com/kklis/proxy).

# Install

    git clone git://github.com/txus/duplex.git
    cd duplex
    make
    make install

# Usage

    duplex -l local_port -h remote_host -p remote_port -H replay_host -P replay_port

Where:

* `local_port`: The local port to listen to.
* `remote_host`: The remote host to forward to.
* `remote_port`: The remote port to forward to.
* `replay_host`: The "testing" host to replay the traffic on.
* `replay_port`: The port of that "testing" host.

For example, this will listen for traffic on local port `8080`, forward
everything to `127.0.0.1:10000` and at the same time replay it on
`127.0.0.1:10001`:

    duplex -l 8080 -h 127.0.0.1 -p 10000 -H 127.0.0.1 -P 10001

# Who's this

This was made by Josep M. Bach (Txus) under the MIT license. I'm
![@txustice][twitter] on twitter (where you should probably follow me!).

Special credits to ![Krzystztof Klis][kklis] for his ![proxy][proxy] library,
upon which `duplex` is based.

[twitter]: https://twitter.com/txustice
[proxy]: https://github.com/kklis/proxy
[kklis]: https://github.com/kklis