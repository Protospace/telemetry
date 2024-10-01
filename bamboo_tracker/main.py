# Script that talks to Bambu P1S printer over mqtt to get status
# and then sends to Spaceport.
#
# Note: ACCESS_CODE may change after firmware updates

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

clients = {}
printers = {}

for PRINTER in secrets.PRINTERS:
    client = BambuClient(
        device_type='P1S',
        serial=PRINTER['serial'],
        host=PRINTER['printer_ip'],
        username='bblp',
        access_code=PRINTER['access_code'],
    )
    clients[PRINTER['name']] = client

    printer = dict(
        info=dict(),
        temperature=dict(),
    )
    printers[PRINTER['name']] = printer

    logging.info('Added printer %s, ip: %s, serial: %s', PRINTER['name'], PRINTER['printer_ip'], PRINTER['serial'])


def event_handler(event, name):
    if 'jpeg_received' in event:
        return

    logging.debug('Received data for printer %s', name)

    printer = printers[name]

    for attr in ['print_percentage', 'gcode_state', 'remaining_time', 'current_layer', 'total_layers', 'online']:
        printer['info'][attr] = getattr(client._device.info, attr)

    for attr in ['bed_temp', 'target_bed_temp', 'nozzle_temp', 'target_nozzle_temp']:
        printer['temperature'][attr] = getattr(client._device.temperature, attr)

async def listen_printer():
    for name, client in clients.items():
        await client.connect(callback=lambda event: event_handler(event, name))
        logging.info('Finished connecting to: %s', name)

async def portal_send():
    async with aiohttp.ClientSession() as session:
        while True:
            sleep_time = 5 if DEBUG else 60
            await asyncio.sleep(sleep_time)

            for name, printer in printers.items():
                logging.info('Sending %s printer data to portal...', name)
                logging.debug('JSON data:\n%s', json.dumps(printer, indent=4))

                try:
                    url = 'https://api.spaceport.dns.t0.vc/stats/{}/printer3d/'.format(name)
                    await session.post(url, json=printer, timeout=10)
                except KeyboardInterrupt:
                    break
                except BaseException as e:
                    logging.error('Problem sending printer data to dev portal %s:', url)
                    logging.exception(e)

                #try:
                #    url = 'https://api.my.protospace.ca/stats/{}/printer3d/'.format(name)
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
