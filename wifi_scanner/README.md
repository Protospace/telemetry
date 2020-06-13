# Wifi Scanner

## Setup

tshark:

```
$ sudo apt update
$ sudo apt install tshark
# select allow
$ sudo usermod -aG wireshark `whoami`
```

Python:

```
$ sudo apt install python3 python3-pip python-virtualenv python3-virtualenv
$ virtualenv -p python3 env
$ source env/bin/activate
(env) $ pip install .
```

## Usage

Get the name of your wifi dongle:

```
$ ip addr
```

It's probably `wlan1` if your Raspberry Pi has Wifi, `wlan0` if it doesn't.

```
(env) $ python -m wifi_scanner -a wlan0
```


## Acknowledgements

Forked from https://github.com/schollz/howmanypeoplearearound to add code that sends the results to a server.
