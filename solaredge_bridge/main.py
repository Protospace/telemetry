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

import secrets

async def process_solar_data(user, data):
    try:
        site_id = user['site_id']
        power = data['overview']['currentPower']['power']
        name = user['name']

        logging.info('Site ID: %s, user: %s, power: %s', site_id, name, power)

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

async def get_solaredge_data(user):
    try:
        async with aiohttp.ClientSession() as session:
            data = dict(api_key=user['api_key'])

            url = 'https://monitoringapi.solaredge.com/site/{}/overview'.format(user['site_id'])
            res = await session.get(url, params=data, timeout=10)
            data = await res.json()

            logging.info('Got SolarEdge data: %s', data)
            return data
    except BaseException as e:
        logging.error('Problem getting json from SolarEdge:')
        logging.exception(e)
        return False

async def main():
    while True:
        for user in secrets.SOLAREDGE_USERS:
            data = await get_solaredge_data(user)
            if not data:
                logging.info('Bad data, skipping.')
                continue

            await process_solar_data(user, data)

        await asyncio.sleep(180)


if __name__ == '__main__':
    logging.info('')
    logging.info('==========================')
    logging.info('Booting up...')

    loop = asyncio.get_event_loop()
    loop.run_until_complete(main())
