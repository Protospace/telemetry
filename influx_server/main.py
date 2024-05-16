from flask import Flask, abort, request
from flask_cors import CORS
from influxdb import InfluxDBClient
from datetime import datetime, timedelta
import pytz

app = Flask(__name__)
CORS(app)
client = InfluxDBClient('localhost', 8086, database='telegraf')

SENSORS = [
    ('air', 0, 'pm25'),
    ('air', 0, 'temp'),
    ('air', 1, 'pm25'),
    ('air', 1, 'temp'),
    ('sound', 0, 'db'),
]

TIMEZONE = pytz.timezone('America/Edmonton')

@app.route('/')
def index():
    return '''<pre>
Protospace IoT API Docs
=======================

This API allows you to get data from Protospace sensors.


Sensors
-------

Air:
    0: Classroom ceiling PM2.5 particulate in ug/m3 and temperature sensor.
    1: Wood shop ceiling PM2.5 particulate in ug/m3 and temperature sensor.


Routes
------

<b>GET /sensors/{kind}/{num}/{measurement}</b>

Get the last value recorded for that sensor.

Current measurements:
    /sensors/air/0/temp
    /sensors/air/0/pm25
    /sensors/air/1/temp
    /sensors/air/1/pm25

Examples:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp">https://ps-iot.dns.t0.vc/sensors/air/0/temp</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25">https://ps-iot.dns.t0.vc/sensors/air/0/pm25</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/1/temp">https://ps-iot.dns.t0.vc/sensors/air/1/temp</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/1/pm25">https://ps-iot.dns.t0.vc/sensors/air/1/pm25</a>


<b>GET /sensors/{kind}/{num}/{measurement}/today</b>

Get all sensor readings from today so far (Calgary timezone).

Examples:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp/today">https://ps-iot.dns.t0.vc/sensors/air/0/temp/today</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today">https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today</a>


<b>GET /sensors/{kind}/{num}/{measurement}/{duration}</b>

Get all sensor readings from the specified duration.

Current durations:
    day (last 24 hours)
    week (last 7 days)
    month (last 30 days)

Examples:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp/day">https://ps-iot.dns.t0.vc/sensors/air/0/temp/day</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp/week">https://ps-iot.dns.t0.vc/sensors/air/0/temp/week</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp/month">https://ps-iot.dns.t0.vc/sensors/air/0/temp/month</a>


<b>GET /sensors/{kind}/{num}/{measurement}/{date}</b>

Get all sensor readings from the specified date (Calgary timezone).

Date format:
    YYYY-MM-DD

Example:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/temp/2021-06-26">https://ps-iot.dns.t0.vc/sensors/air/0/temp/2021-06-26</a>


Parameters
----------

These query parameters apply to the previous three routes.

<b>?window={n}</b>

Average samples into windows of n minutes. The default is 15 minutes and the minimum is 1 minute.

Examples:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25/2021-06-26?window=10">https://ps-iot.dns.t0.vc/sensors/air/0/pm25/2021-06-26?window=10</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today?window=60">https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today?window=60</a>

<b>?moving_average={n}</b>

Apply a moving average of the previous n samples to each window. The minimum is 2.

Examples:
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25/2021-06-26?moving_average=10">https://ps-iot.dns.t0.vc/sensors/air/0/pm25/2021-06-26?moving_average=10</a>
    <a href="https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today?window=1&moving_average=100">https://ps-iot.dns.t0.vc/sensors/air/0/pm25/today?window=1&moving_average=100</a>
</pre>'''

@app.route('/<string:domain>/<string:kind>/<int:num>/<string:measurement>')
def sensors(domain, kind, num, measurement):
    if (kind, num, measurement) not in SENSORS:
        abort(404)

    if domain not in ['sensors', 'test']:
        abort(404)

    topic = '{}/{}/{}/{}'.format(domain, kind, num, measurement)
    q = 'select last(value) as value from mqtt_consumer where "topic" = \'' + topic + '\''
    result = list(client.query(q).get_points())[0]

    return result

@app.route('/<string:domain>/<string:kind>/<int:num>/<string:measurement>/<string:lookup>')
def sensors_history(domain, kind, num, measurement, lookup):
    if (kind, num, measurement) not in SENSORS:
        abort(404)

    if domain not in ['sensors', 'test']:
        abort(404)

    window = request.args.get('window', 15)
    moving_average = request.args.get('moving_average', None)

    topic = '{}/{}/{}/{}'.format(domain, kind, num, measurement)

    try:
        parse_date = datetime.strptime(lookup, '%Y-%m-%d')
        start = TIMEZONE.localize(parse_date)
        end = start + timedelta(days=1)
    except ValueError:
        now = datetime.now(tz=TIMEZONE)

        if lookup == 'today':
            start = now.replace(hour=0, minute=0, second=0, microsecond=0)
            end = now
        elif lookup == 'day':
            start = now - timedelta(days=1)
            end = now
        elif lookup == 'week':
            start = now - timedelta(days=7)
            end = now
        elif lookup == 'month':
            start = now - timedelta(days=30)
            end = now
        else:
            abort(404)

    start = int(start.timestamp())
    end = int(end.timestamp())


    if window and moving_average:
        q = 'select moving_average(mean("value"),{}) as value from mqtt_consumer where "topic" = \'{}\' and time >= {}s and time < {}s group by time({}m) fill(none)'.format(moving_average, topic, start, end, window)
    elif window:
        q = 'select mean("value") as value from mqtt_consumer where "topic" = \'{}\' and time >= {}s and time < {}s group by time({}m) fill(none)'.format(topic, start, end, window)
    elif moving_average:
        q = 'select moving_average("value", {}) as value from mqtt_consumer where "topic" = \'{}\' and time >= {}s and time < {}s'.format(moving_average, topic, start, end)
    else:
        q = 'select value from mqtt_consumer where "topic" = \'{}\' and time >= {}s and time < {}s'.format(moving_average, topic, start, end)

    result = list(client.query(q).get_points())

    return dict(result=result)


if __name__ == '__main__':
    app.run(port=6900)
