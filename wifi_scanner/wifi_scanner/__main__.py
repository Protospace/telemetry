import threading
import sys
import os
import os.path
import platform
import subprocess
import json
import time
import requests
import logging

import netifaces
import click

from wifi_scanner.oui import load_dictionary, download_oui
from wifi_scanner.analysis import analyze_file
from wifi_scanner.colors import *

def getserial():
  # Extract serial from cpuinfo file
  cpuserial = "0000000000000000"
  try:
    f = open('/proc/cpuinfo','r')
    for line in f:
        if line[0:6]=='Serial':
            cpuserial = line[10:26]
    f.close()
  except:
      cpuserial = "ERROR000000000"
 
  return cpuserial

SERIAL = getserial()

SEND_BUFFER = []
IOT_URL = 'http://games.protospace.ca:5000/wifi-scan'

if os.name != 'nt':
    from pick import pick
    import curses

def which(program):
    """Determines whether program exists
    """
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file
    raise

def showTimer(timeleft):
    """Shows a countdown timer"""
    total = int(timeleft) * 10
    for i in range(total):
        sys.stdout.write('\r')
        # the exact output you're looking for:
        timeleft_string = '%ds left' % int((total - i + 1) / 10)
        if (total - i + 1) > 600:
            timeleft_string = '%dmin %ds left' % (
                int((total - i + 1) / 600), int((total - i + 1) / 10 % 60))
        sys.stdout.write("[%-50s] %d%% %15s" %
                         ('=' * int(50.5 * i / total), 101 * i / total, timeleft_string))
        sys.stdout.flush()
        time.sleep(0.1)
    print("")

def fileToMacSet(path):
    with open(path, 'r') as f:
        maclist = f.readlines()
    return set([x.strip() for x in maclist])


def scan(adapter, scantime, verbose, dictionary, number, nearby, jsonprint, out, allmacaddresses, manufacturers, nocorrection, sort, targetmacs, pcap):
    """Monitor wifi signals to count the number of people around you"""

    # print("OS: " + os.name)
    # print("Platform: " + platform.system())

    if (not os.path.isfile(dictionary)) or (not os.access(dictionary, os.R_OK)):
        download_oui(dictionary)

    oui = load_dictionary(dictionary)

    if not oui:
        print('couldn\'t load [%s]' % dictionary)
        sys.exit(1)

    try:
        tshark = which("tshark")
    except:
        if platform.system() != 'Darwin':
            print('tshark not found, install using\n\napt-get install tshark\n')
        else:
            print('wireshark not found, install using: \n\tbrew install wireshark')
            print(
                'you may also need to execute: \n\tbrew cask install wireshark-chmodbpf')
        sys.exit(1)

    if jsonprint:
        number = True
    if number:
        verbose = False

    if not pcap:
        if len(adapter) == 0:
            if os.name == 'nt':
                print('You must specify the adapter with   -a ADAPTER')
                print('Choose from the following: ' +
                      ', '.join(netifaces.interfaces()))
                sys.exit(1)
            title = 'Please choose the adapter you want to use: '
            try:
                adapter, index = pick(netifaces.interfaces(), title)
            except curses.error as e:
                print('Please check your $TERM settings: %s' % (e))
                sys.exit(1)

        print("Using %s adapter and scanning for %s seconds..." %
              (adapter, scantime))

        if not number:
            # Start timer
            t1 = threading.Thread(target=showTimer, args=(scantime,))
            t1.daemon = True
            t1.start()

        dump_file = '/tmp/tshark-temp'
        # Scan with tshark
        command = [tshark, '-I', '-i', adapter, '-a',
                   'duration:' + scantime, '-w', dump_file]
        if verbose:
            print(' '.join(command))
        run_tshark = subprocess.Popen(
            command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        stdout, nothing = run_tshark.communicate()


        if not number:
            t1.join()
    else:
        dump_file = pcap

    # Read tshark output
    command = [
        tshark, '-r',
        dump_file, '-T',
        'fields', '-e',
        'wlan.sa', '-e',
        'wlan.bssid', '-e',
        'radiotap.dbm_antsignal'
    ]
    if verbose:
        print(' '.join(command))
    run_tshark = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output, nothing = run_tshark.communicate()

    # read target MAC address
    targetmacset = set()
    if targetmacs != '':
        targetmacset = fileToMacSet(targetmacs)

    foundMacs = {}
    for line in output.decode('utf-8').split('\n'):
        if verbose:
            print(line)
        if line.strip() == '':
            continue
        mac = line.split()[0].strip().split(',')[0]
        dats = line.split()
        if len(dats) == 3:
            if ':' not in dats[0] or len(dats) != 3:
                continue
            if mac not in foundMacs:
                foundMacs[mac] = []
            dats_2_split = dats[2].split(',')
            if len(dats_2_split) > 1:
                rssi = float(dats_2_split[0]) / 2 + float(dats_2_split[1]) / 2
            else:
                rssi = float(dats_2_split[0])
            foundMacs[mac].append(rssi)

    if not foundMacs:
        print("Found no signals, are you sure %s supports monitor mode?" % adapter)
        sys.exit(1)

    for key, value in foundMacs.items():
        foundMacs[key] = float(sum(value)) / float(len(value))

    # Find target MAC address in foundMacs
    if targetmacset:
        sys.stdout.write(RED)
        for mac in foundMacs:
            if mac in targetmacset:
                print("Found MAC address: %s" % mac)
                print("rssi: %s" % str(foundMacs[mac]))
        sys.stdout.write(RESET)

    if manufacturers:
        f = open(manufacturers,'r')
        cellphone = [line.rstrip('\n') for line in f.readlines()]
        f.close()
    else:
        cellphone = [
            'Motorola Mobility LLC, a Lenovo Company',
            'GUANGDONG OPPO MOBILE TELECOMMUNICATIONS CORP.,LTD',
            'Huawei Symantec Technologies Co.,Ltd.',
            'Microsoft',
            'HTC Corporation',
            'Samsung Electronics Co.,Ltd',
            'SAMSUNG ELECTRO-MECHANICS(THAILAND)',
            'BlackBerry RTS',
            'LG ELECTRONICS INC',
            'Apple, Inc.',
            'LG Electronics',
            'OnePlus Tech (Shenzhen) Ltd',
            'Xiaomi Communications Co Ltd',
            'LG Electronics (Mobile Communications)']

    cellphone_people = []
    for mac in foundMacs:
        oui_id = 'Not in OUI'
        if mac[:8] in oui:
            oui_id = oui[mac[:8]]
        if verbose:
            print(mac, oui_id, oui_id in cellphone)
        if allmacaddresses or oui_id in cellphone:
            if not nearby or (nearby and foundMacs[mac] > -70):
                cellphone_people.append(
                    {'company': oui_id, 'rssi': foundMacs[mac], 'mac': mac})
    if sort:
        cellphone_people.sort(key=lambda x: x['rssi'], reverse=True)
    if verbose:
        print(json.dumps(cellphone_people, indent=2))

    # US / Canada: https://twitter.com/conradhackett/status/701798230619590656
    percentage_of_people_with_phones = 0.7
    if nocorrection:
        percentage_of_people_with_phones = 1
    num_people = int(round(len(cellphone_people) /
                           percentage_of_people_with_phones))

    if number and not jsonprint:
        print(num_people)
    elif jsonprint:
        print(json.dumps(cellphone_people, indent=2))
    else:
        if num_people == 0:
            print("No one around (not even you!).")
        elif num_people == 1:
            print("No one around, but you.")
        else:
            print("There are about %d people around." % num_people)

    if out:
        with open(out, 'a') as f:
            data_dump = {'cellphones': cellphone_people, 'time': time.time()}
            f.write(json.dumps(data_dump) + "\n")
        if verbose:
            print("Wrote %d records to %s" % (len(cellphone_people), out))
    if not pcap:
        os.remove(dump_file)

    results = {
        'records': cellphone_people,
        'time': int(time.time()),
        'serial': SERIAL,
    }
    return adapter, results



@click.command()
@click.option('-a', '--adapter', default='', help='adapter to use')
def main(adapter):
    scantime = '60'
    verbose = False
    dictionary = 'oui.txt'
    number = True
    nearby = False
    jsonprint = False
    out = ''
    allmacaddresses = True
    manufacturers = ''
    nocorrection = True
    sort = False
    targetmacs = ''
    pcap = ''

    while True:
        adapter, results = scan(adapter, scantime, verbose, dictionary, number,
             nearby, jsonprint, out, allmacaddresses, manufacturers,
             nocorrection, sort, targetmacs, pcap)
        SEND_BUFFER.append(results)

        try:
            while len(SEND_BUFFER):
                r = requests.post(IOT_URL, json=SEND_BUFFER[0], timeout=5)
                r.raise_for_status()
                SEND_BUFFER.pop(0)
        except:
            logging.exception('Problem sending to server:')


if __name__ == '__main__':
    main()
