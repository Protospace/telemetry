import os, logging
DEBUG = os.environ.get('DEBUG')
logging.basicConfig(
        format='[%(asctime)s] %(levelname)s %(module)s/%(funcName)s - %(message)s',
        level=logging.DEBUG if DEBUG else logging.INFO)

import time
import json
import asyncio
import aiohttp

import secrets
from pybambu import BambuClient

# TODO: support multiple clients
client = BambuClient(
    device_type='P1S',
    serial=secrets.SERIAL,
    host=secrets.PRINTER_IP,
    username='bblp',
    access_code=secrets.ACCESS_CODE,
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
                url = 'https://api.spaceport.dns.t0.vc/stats/p1s1/printer3d/'
                await session.post(url, json=printer, timeout=10)
            except KeyboardInterrupt:
                break
            except BaseException as e:
                logging.error('Problem sending printer data to portal %s:', url)
                logging.exception(e)

            #try:
            #    url = 'https://api.my.protospace.ca/stats/p1s1/printer3d/'
            #    await session.post(url, json=printer, timeout=10)
            #except KeyboardInterrupt:
            #    break
            #except BaseException as e:
            #    logging.error('Problem sending printer data to portal %s:', url)
            #    logging.exception(e)

            logging.debug('Done sending.')


loop = asyncio.get_event_loop()
loop.create_task(listen_printer())
loop.create_task(portal_send())
loop.run_forever()
