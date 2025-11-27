# Script that gets Prusa XL printer status from Home Assistant
# and then sends to Spaceport.

import os, logging
DEBUG = os.environ.get('DEBUG')
logging.basicConfig(
        format='[%(asctime)s] %(levelname)s %(module)s/%(funcName)s - %(message)s',
        level=logging.DEBUG if DEBUG else logging.INFO)
logging.getLogger('aiohttp').setLevel(logging.DEBUG if DEBUG else logging.WARNING)

logging.info('Boot up...')

import time
import json
import asyncio
import aiohttp
import sys
import ssl

import secrets


sslcontext = ssl.create_default_context()
sslcontext.check_hostname = False
sslcontext.verify_mode = ssl.CERT_NONE


if len(sys.argv) != 2:
    print('Missing arguments. Run like so:')
    print('python main.py NAME')
    print('Example: python main.py prusa_xl')
    exit(1)

_, NAME, = sys.argv

printer = dict(
    info=dict(),
)

async def printer_status():
    while True:
        sleep_time = 5 if DEBUG else 60
        await asyncio.sleep(sleep_time)

        try:
            headers = {'Authorization': 'Bearer ' + secrets.HOMEASSISTANT_TOKEN}
            url = secrets.HOMEASSISTANT_URL + '/api/states'
            async with aiohttp.ClientSession() as session:
                res = await session.get(url, headers=headers, timeout=10, ssl=sslcontext)
                res.raise_for_status()
                res = await res.json()
        except KeyboardInterrupt:
            break
        except BaseException as e:
            printer['info']['online'] = False
            logging.error('Problem getting status from Home Assistant URL %s:', url)
            logging.exception(e)

        status = {}
        prefix = 'sensor.' + NAME   # sensor.prusa_xl

        for entry in res:
            if entry['entity_id'].startswith(prefix):
                status[entry['entity_id']] = entry['state']

        logging.debug('Status data:\n%s', json.dumps(status, indent=4))

        if not status:
            raise Exception('Status empty, do you have the correct serial number?')

        printer['info']['online'] = True
        printer['info']['state'] = status['sensor.prusa_xl'].title()
        printer['info']['print_speed'] = status['sensor.prusa_xl_print_speed']
        printer['info']['material'] = status['sensor.prusa_xl_material']
        printer['info']['progress'] = status['sensor.prusa_xl_progress']
        printer['info']['filename'] = status['sensor.prusa_xl_filename']
        printer['info']['print_start'] = status['sensor.prusa_xl_print_start']
        printer['info']['print_finish'] = status['sensor.prusa_xl_print_finish']

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



def task_died(future):
    if os.environ.get('SHELL'):
        logging.error('Prusa tracker task died!')
    else:
        logging.error('Prusa tracker task died! Waiting 60s and exiting...')
        time.sleep(60)
    exit()

loop = asyncio.get_event_loop()
loop.create_task(printer_status()).add_done_callback(task_died)
loop.run_forever()

