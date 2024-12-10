# Script that talks to Bambu P1S printer over mqtt to get status
# and then sends to Spaceport.
#
# Note: ACCESS_CODE may change after firmware updates

import os, logging
DEBUG = os.environ.get('DEBUG')
logging.basicConfig(
        format='[%(asctime)s] %(levelname)s %(module)s/%(funcName)s - %(message)s',
        level=logging.DEBUG if DEBUG else logging.INFO)
logging.getLogger('aiohttp').setLevel(logging.DEBUG if DEBUG else logging.WARNING)

import time
import json
import asyncio
import aiohttp
from aiohttp import web
import sys

app = web.Application()

from pybambu import BambuClient

if len(sys.argv) != 6:
    print('Missing arguments. Run like so:')
    print('python main.py NAME SERIAL PRINTER_IP ACCESS_CODE LISTEN_PORT')
    print('Example: python main.py p1s1 123412341234123 172.17.18.999 12345679 8080')
    exit(1)

_, NAME, SERIAL, PRINTER_IP, ACCESS_CODE, LISTEN_PORT = sys.argv

client = BambuClient(
    device_type='P1S',
    serial=SERIAL,
    host=PRINTER_IP,
    username='bblp',
    access_code=ACCESS_CODE,
)

printer = dict(
    info=dict(),
    temperature=dict(),
)


def event_handler(event):
    if 'jpeg_received' in event:
        return

    for attr in ['print_percentage', 'gcode_state', 'remaining_time', 'current_layer', 'total_layers', 'online']:
        printer['info'][attr] = getattr(client._device.info, attr)

    for attr in ['bed_temp', 'target_bed_temp', 'nozzle_temp', 'target_nozzle_temp']:
        printer['temperature'][attr] = getattr(client._device.temperature, attr)

async def listen_printer():
    await client.connect(callback=event_handler)

async def portal_send():
    async with aiohttp.ClientSession() as session:
        while True:
            sleep_time = 5 if DEBUG else 60
            await asyncio.sleep(sleep_time)

            logging.info('Sending to portal...')
            logging.debug('JSON data:\n%s', json.dumps(printer, indent=4))

            try:
                url = 'https://api.spaceport.dns.t0.vc/stats/{}/printer3d/'.format(NAME)
                await session.post(url, json=printer, timeout=10)
            except KeyboardInterrupt:
                break
            except BaseException as e:
                logging.error('Problem sending printer data to dev portal %s:', url)
                logging.exception(e)

            try:
                url = 'https://api.my.protospace.ca/stats/{}/printer3d/'.format(NAME)
                await session.post(url, json=printer, timeout=10)
            except KeyboardInterrupt:
                break
            except BaseException as e:
                logging.error('Problem sending printer data to portal %s:', url)
                logging.exception(e)

            logging.debug('Done sending.')

async def index(request):
    return web.Response(text='bambu bridge', content_type='text/html')

async def pic(request):
    return web.Response(body=client._device.p1p_camera.get_jpeg(), content_type='image/jpeg')


async def main():
    app.router.add_get('/', index)
    app.router.add_get('/' + NAME + '/pic', pic)

    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', LISTEN_PORT)
    await site.start()

loop = asyncio.get_event_loop()
loop.create_task(listen_printer())
loop.create_task(portal_send())
loop.create_task(main())
loop.run_forever()

