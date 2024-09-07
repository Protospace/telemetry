# Hoymiles Solar Bridge

This bridges solar production data between the Hoymiles Solar DTU and the member portal Spaceport.

Runs on a Raspberry Pi Zero W on the solar user's home network.

# Setup

Install on Debian 11 / 12:

```
$ cp secrets.py.example secrets.py
$ vim secrets.py
```

Set up Python:

```
$ sudo apt install python3 python3-pip python3-virtualenv
$ virtualenv -p python3 env
$ . env/bin/activate
(env) $ pip install -r requirements.txt
```

Test running:

```
(env) $ DEBUG=true python main.py
```

Persist with supervisor:

```
$ sudo apt install supervisor
$ sudo touch /etc/supervisor/conf.d/hoymiles_bridge.conf
$ sudoedit /etc/supervisor/conf.d/hoymiles_bridge.conf
```

Edit the file:

```
[program:hoymiles_bridge]
user=tanner
directory=/home/tanner/telemetry/hoymiles_bridge
command=/home/tanner/telemetry/hoymiles_bridge/env/bin/python -u main.py
stopsignal=INT
stopasgroup=true
killasgroup=true
autostart=true
autorestart=true
stderr_logfile=/var/log/hoymiles_bridge.log
stderr_logfile_maxbytes=10MB
stdout_logfile=/var/log/hoymiles_bridge.log
stdout_logfile_maxbytes=10MB
```

Reread the config:

```
$ sudo supervisorctl reread; sudo supervisorctl update
```

You can control it with these commands:

```
$ sudo supervisorctl status hoymiles_bridge
$ sudo supervisorctl stop hoymiles_bridge
$ sudo supervisorctl start hoymiles_bridge
$ sudo supervisorctl restart hoymiles_bridge
```
