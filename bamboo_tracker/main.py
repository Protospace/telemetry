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
import aiofiles
from aiohttp import web
import sys

import secrets

app = web.Application()


if len(sys.argv) != 4:
    print('Missing arguments. Run like so:')
    print('python main.py NAME SERIAL LISTEN_PORT')
    print('Example: python main.py p1s1 01P09C471500459 8080')
    exit(1)

_, NAME, SERIAL, LISTEN_PORT = sys.argv
SERIAL = SERIAL.lower()

printer = dict(
    info=dict(),
    temperature=dict(),
)

async def printer_status():
    while True:
        sleep_time = 5 if DEBUG else 60
        await asyncio.sleep(sleep_time)

        try:
            headers = {'Authorization': 'Bearer ' + secrets.HOMEASSISTANT_TOKEN}
            url = secrets.HOMEASSISTANT_URL + '/api/states'
            async with aiohttp.ClientSession() as session:
                res = await session.get(url, headers=headers, timeout=10)
                res.raise_for_status()
                res = await res.json()
        except KeyboardInterrupt:
            break
        except BaseException as e:
            printer['info']['online'] = False
            logging.error('Problem getting status from Home Assistant URL %s:', url)
            logging.exception(e)

        entities = [
            f'sensor.p1s_{SERIAL}_print_status',
            f'sensor.p1s_{SERIAL}_print_progress',
            f'sensor.p1s_{SERIAL}_current_layer',
            f'sensor.p1s_{SERIAL}_total_layer_count',
            f'sensor.p1s_{SERIAL}_remaining_time',
            f'sensor.p1s_{SERIAL}_bed_temperature',
            f'sensor.p1s_{SERIAL}_bed_target_temperature',
            f'sensor.p1s_{SERIAL}_nozzle_temperature',
            f'sensor.p1s_{SERIAL}_nozzle_target_temperature',
        ]
        status = {}

        for entry in res:
            if entry['entity_id'] in entities:
                status[entry['entity_id'].split('_',2)[2]] = entry['state']

        logging.debug('Status data:\n%s', json.dumps(status, indent=4))

        if not status:
            raise Exception('Status empty, do you have the correct serial number?')

        # massage data into old integration format
        printer['info']['online'] = True
        printer['info']['gcode_state'] = status['print_status'].upper()
        printer['info']['current_layer'] = status['current_layer']
        printer['info']['total_layers'] = status['total_layer_count']
        printer['info']['print_percentage'] = status['print_progress']
        printer['info']['remaining_time'] = status['remaining_time']

        printer['temperature']['bed_temp'] = status['bed_temperature']
        printer['temperature']['target_bed_temp'] = status['bed_target_temperature']
        printer['temperature']['nozzle_temp'] = status['nozzle_temperature']
        printer['temperature']['target_nozzle_temp'] = status['nozzle_target_temperature']

        logging.info('Sending to portal...')
        logging.debug('JSON data:\n%s', json.dumps(printer, indent=4))

        try:
            url = 'https://api.my.protospace.ca/stats/{}/printer3d/'.format(NAME)
            async with aiohttp.ClientSession() as session:
                await session.post(url, json=printer, timeout=10)
        except KeyboardInterrupt:
            break
        except BaseException as e:
            logging.error('Problem sending printer data to portal %s:', url)
            logging.exception(e)

        try:
            url = 'https://api.spaceport.dns.t0.vc/stats/{}/printer3d/'.format(NAME)
            async with aiohttp.ClientSession() as session:
                await session.post(url, json=printer, timeout=10)
        except KeyboardInterrupt:
            break
        except BaseException as e:
            logging.info('Problem sending printer data to dev portal %s:', url)

        logging.debug('Done sending.')



async def printer_camera():
    while True:
        await asyncio.sleep(1)

        try:
            logging.debug('Getting camera frame...')

            headers = {'Authorization': 'Bearer ' + secrets.HOMEASSISTANT_TOKEN}
            url = secrets.HOMEASSISTANT_URL + f'/api/camera_proxy/camera.p1s_{SERIAL}_camera'
            async with aiohttp.ClientSession() as session:
                res = await session.get(url, headers=headers, timeout=10)
                res.raise_for_status()

                f = await aiofiles.open(NAME + '/pic.jpg', mode='wb')
                try:
                    await f.write(await res.read())
                finally:
                    await f.close()

            logging.debug('Saved snapshot.')
        except KeyboardInterrupt:
            break
        except BaseException as e:
            logging.error('Problem getting camera frame from Home Assistant URL %s:', url)
            logging.exception(e)


async def index(request):
    return web.Response(text='bambu bridge', content_type='text/html')

async def main():
    app.router.add_get('/', index)
    app.add_routes([web.static('/' + NAME, NAME)])

    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', LISTEN_PORT)
    await site.start()

def task_died(future):
    if os.environ.get('SHELL'):
        logging.error('Bambu tracker task died!')
    else:
        logging.error('Bambu tracker task died! Waiting 60s and exiting...')
        time.sleep(60)
    exit()

loop = asyncio.get_event_loop()
loop.create_task(printer_status()).add_done_callback(task_died)
loop.create_task(printer_camera()).add_done_callback(task_died)
loop.create_task(main())
loop.run_forever()

