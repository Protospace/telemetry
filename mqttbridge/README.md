# MQTT Bridge

This bridges messages between Protospace's MQTT broker and the member portal Spaceport.

Runs on the webhost VPS (protospace.ca).

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
(env) $ python main.py
```

Persist with supervisor:

```
$ sudo apt install supervisor
$ sudo touch /etc/supervisor/conf.d/mqttbridge.conf
$ sudoedit /etc/supervisor/conf.d/mqttbridge.conf
```

Edit the file:

```
[program:mqttbridge]
user=tanner
directory=/home/tanner/telemetry/mqttbridge
command=/home/tanner/telemetry/mqttbridge/env/bin/python -u main.py
stopsignal=INT
stopasgroup=true
killasgroup=true
autostart=true
autorestart=true
stderr_logfile=/var/log/mqttbridge.log
stderr_logfile_maxbytes=10MB
stdout_logfile=/var/log/mqttbridge.log
stdout_logfile_maxbytes=10MB
```

Reread the config:

```
$ sudo supervisorctl reread; sudo supervisorctl update
```

You can control it with these commands:

```
$ sudo superisorctl status mqttbridge
$ sudo superisorctl stop mqttbridge
$ sudo superisorctl start mqttbridge
$ sudo superisorctl restart mqttbridge
```
