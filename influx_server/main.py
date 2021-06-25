from flask import Flask, abort
from flask_cors import CORS
from influxdb import InfluxDBClient

app = Flask(__name__)
CORS(app)
client = InfluxDBClient('localhost', 8086, database='telegraf')

SENSORS = [
    ('air', 0, 'pm25'),
    ('air', 0, 'temp'),
]

@app.route('/')
def index():
    return 'Hello world'

@app.route('/sensors/<string:kind>/<int:num>/<string:measurement>')
def sensors(kind, num, measurement):
    if (kind, num, measurement) not in SENSORS:
        abort(404)

    topic = 'sensors/{}/{}/{}'.format(kind, num, measurement)
    q = 'select last(value) from mqtt_consumer where "topic" = \'' + topic + '\''
    result = list(client.query(q).get_points())[0]

    return result


if __name__ == '__main__':
    app.run(port=6900)
