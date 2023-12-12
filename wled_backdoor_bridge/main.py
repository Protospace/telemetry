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
from aiomqtt import Client, TLSParameters


tls_params = TLSParameters(
    ca_certs='/etc/ssl/certs/ISRG_Root_X1.pem',
)
    
import secrets


async def sign_send():
    async with aiohttp.ClientSession() as session:
        preset = {
            'playlist': {
                'ps': [3],
                'dur': [3],
                'repeat': 1,
                'end': 1
            }
        }

        logging.info('Sending to sign...')
        logging.debug('JSON data:\n%s', json.dumps(preset, indent=4))

        try:
            url = 'http://172.17.18.181/json'
            #url = 'http://wled-ps108sign.local/json'
            await session.post(url, json=preset, timeout=10)
        except BaseException as e:
            logging.error('Problem sending json to sign %s:', url)
            logging.exception(e)


async def process_mqtt(message):
    text = message.payload.decode()
    topic = message.topic.value
    logging.info('MQTT topic: %s, message: %s', topic, text)

    if DEBUG:
        listen_topic = 'dev_spaceport/door/scan'
    else:
        listen_topic = 'spaceport/door/scan'

    if topic != listen_topic:
        logging.info('Invalid topic, returning')
        return
    
    #valid topic
    await sign_send()
    


async def fetch_mqtt():
    await asyncio.sleep(3)
    
    if DEBUG:
        listen_topic = 'dev_spaceport/#'
    else:
        listen_topic = 'spaceport/#'
    
    async with Client(hostname="webhost.protospace.ca", 
    port=8883,
    username='reader',
    password=secrets.MQTT_READER_PASSWORD, tls_params=tls_params) as client:
        async with client.messages() as messages:
            await client.subscribe(listen_topic)
            async for message in messages:
                loop = asyncio.get_event_loop()
                loop.create_task(process_mqtt(message))


if __name__ == '__main__':
    logging.info('')
    logging.info('==========================')
    logging.info('Booting up...')

    loop = asyncio.get_event_loop()
    loop.run_until_complete(fetch_mqtt())



