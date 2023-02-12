import os, logging
DEBUG = os.environ.get('DEBUG')
logging.basicConfig(
        format='[%(asctime)s] %(levelname)s %(module)s/%(funcName)s - %(message)s',
        level=logging.DEBUG if DEBUG else logging.INFO)

import asyncore
from smtpd import SMTPServer

import email
from email import policy
import io
import zipfile
import xmltodict
import requests
import time
import json

PROTOCOIN_PRINTER_URL = 'https://api.spaceport.dns.t0.vc/protocoin/printer_report/'

class mySMTPServer(SMTPServer):
    def process_message(self, peer, mailfrom, rcpttos, data, **kwargs):
        logging.info('Received printer accounting email')
        message = email.message_from_bytes(data, policy=policy.default)

        if message['subject'] != 'Accounting report. Printer Serial Number: CN05R3H03S':
            logging.error('Email subject mismatch')
            raise

        if not message.is_multipart():
            logging.error('Message is not multipart')
            raise

        attachments = list(message.iter_attachments())
        accounting_attachment = attachments[0]

        if not accounting_attachment.is_attachment():
            logging.error('Accounting is not an attachment')
            raise

        accounting_buffer = accounting_attachment.get_content()
        accounting_zip = zipfile.ZipFile(io.BytesIO(accounting_buffer))
        accounting_xml = accounting_zip.read('accounting.xml')
        accounting = xmltodict.parse(accounting_xml)

        dump_file_name = 'data/' + str(int(time.time())) + '.json'
        with open(dump_file_name, 'w') as dump_file:
            json.dump(accounting, dump_file, indent=4)
        logging.info('Dump saved to %s', dump_file_name)

        accounting_info = accounting['AccountingInfo']['ACCOUNTING_INFO']

        print_info = dict(
            job_name=accounting_info['JOB_NAME']['@value'],
            uuid=accounting_info['UUID']['@value'],
            timestamp=accounting_info['TIMESTAMP']['@value'],
            job_status=accounting_info['JOB_STATUS']['@value'],
            user_name=accounting_info['USER_NAME']['@value'],
            source=accounting_info['SOURCE']['@value'],
            paper_name=accounting_info['MEDIA_INFO']['NAME']['@media-name'],
            paper_sqi=accounting_info['MEDIA_INFO']['QUANTITY']['@value'],
            ink_ul=accounting_info['INK_INFO']['INK_USED']['@value'],
        )

        logging.info('Sending to portal:\n' + str(print_info))

        r = requests.post(PROTOCOIN_PRINTER_URL, json=print_info, timeout=10)
        r.raise_for_status()

        logging.info('Done.')
        return


def run():
    _ = mySMTPServer(('0.0.0.0', 1025), None)
    try:
        asyncore.loop()
    except KeyboardInterrupt:
        pass

if __name__ == '__main__':
    run()
