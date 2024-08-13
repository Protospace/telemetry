import os, sys
import logging
DEBUG = os.environ.get('DEBUG')
logging.basicConfig(stream=sys.stdout,
    format='[%(asctime)s] %(levelname)s %(module)s/%(funcName)s - %(message)s',
    level=logging.DEBUG if DEBUG else logging.INFO)

import time
import json

import asyncio
import aiohttp
from hoymiles_wifi.dtu import DTU

import secrets

async def process_solar_data(data):
    try:

        serial = data.device_serial_number
        dtu_power = getattr(data, 'dtu_power', 0)
        power = dtu_power // 10
        name = secrets.SOLAR_INFO['name']

        logging.info('DTU Serial: %s, user: %s, power: %s', serial, name, power)

        async with aiohttp.ClientSession() as session:
            data = dict(user=name, power=power)

            if DEBUG:
                url = 'https://api.spaceport.dns.t0.vc/stats/solar_data/'
            else:
                url = 'https://api.my.protospace.ca/stats/solar_data/'

            await session.post(url, json=data, timeout=10)

            logging.info('Sent to portal URL: %s', url)
    except BaseException as e:
        logging.error('Problem sending json to portal:')
        logging.exception(e)

async def get_hoymiles_data():
    try:
        dtu = DTU(secrets.SOLAR_INFO['dtu_ip'])
        response = await dtu.async_get_real_data_new()

        if response:
            logging.debug('DTU response: %s', response)
            return response
        else:
            raise
    except BaseException as e:
        logging.error('Problem getting data from DTU:')
        logging.exception(e)
        return False

async def main():
    while True:
        data = await get_hoymiles_data()
        if data:
            await process_solar_data(data)
        else:
            logging.info('Bad data.')

        await asyncio.sleep(180)


if __name__ == '__main__':
    logging.info('')
    logging.info('==========================')
    logging.info('Booting up...')

    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
