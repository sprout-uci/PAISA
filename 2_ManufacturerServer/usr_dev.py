import socket
import secrets
import codecs
from mbedtls import pk
from hashlib import sha256
from datetime import datetime
from random import random
import pyshorteners
from urllib import request
from time import time

IP     = ''
Port   = 10101
BufferLen  = 1024
NonceLen = 32
TsLen = 14
DevIdSize = 4
MTUrlSize = 1

# Create a datagram socket
UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# Bind to address and ip
UDPServerSocket.bind((IP, Port))
print("UDP server up and listening")

def procPacket(msg):
    # msg: [devNonce(32) || curTS(14) || signature(variable) || M_T_URL(22) || mTUrlLen(4)]

    nonce = msg[:NonceLen]
    ts = msg[NonceLen:NonceLen+TsLen]
    mTUrlLen = int.from_bytes(msg[-MTUrlSize:], 'little')
    mTUrl = msg[-(MTUrlSize+mTUrlLen):-MTUrlSize]
    sig = msg[NonceLen+TsLen:-(MTUrlSize+mTUrlLen)]

    #print('[', len(nonce),']nonce: ', nonce)
    #print('[', len(ts),']ts: ', ts)
    #print('[', len(sig), ']sig: ', sig)

    #print('')

    return mTUrl, sig, ts, nonce

def verifyMessage(msg, pubKeyBuf, sig):
    ecc = pk.ECC()
    ecc = ecc.from_buffer(pubKeyBuf.encode())

    t = time()
    ret = ecc.verify(msg, sig)
    print("[Time][Verify] %.6f sec" %(time()-t))
    #print('encoded msg: ', codecs.encode(msg, 'hex'))
    #print('encoded sig: ', codecs.encode(sig, 'hex'))
    return ret

def verifyTime(ts):
    ts = datetime(int(ts[4:8]), int(ts[0:2]), int(ts[2:4]), int(ts[8:10]), int(ts[10:12]), int(ts[12:14]))
    now = datetime.now()

    #print(ts)
    #print(now)
    print("Time dif: ", now-ts)

    return now >= ts

def getMUrl(mTUrl):
    type_bitly = pyshorteners.Shortener(api_key='c57082754ee8840757d05b0007c66d75ac0fd87a')
    return type_bitly.bitly.expand(mTUrl).encode()

def parseFileWithUrl(url):
    data = {}
    doc = request.urlopen(url)
    for line in doc:
        data[line.decode().split(':')[0]] = line.decode().split(':')[1].strip()

    return data

def makeSigBody(data, mUrl, mTUrl, ts, devNonce):
	# sig: [devNonce(32) || curTS(14) - from Dev || devId(4) || hashed_M_T_URL(32) || hashed_M_URL(32)]
    sigBody = devNonce + ts + int(data['dev_id']).to_bytes(4, 'little') + sha256(mUrl).digest() + sha256(mTUrl).digest()
    
    pubKeyBuf = data['public_key'].replace('\\r', '').replace('\\n', '\n')
    if pubKeyBuf[-1:] != '\n':
        pubKeyBuf += '\n'

    return sigBody, pubKeyBuf
    
    

# Listen for incoming datagrams
while(True):
    bytesAddressPair = UDPServerSocket.recvfrom(BufferLen)
    message = bytesAddressPair[0]
    address = bytesAddressPair[1]

    #print(message)

    if message[-6:] == b"BRDEND":
        message = message[:-6]
        mTUrl, sig, ts, devNonce= procPacket(message)
        ret = verifyTime(ts)
        if ret != True:
            # TODO: handle failure case
            exit(1)
        
        mUrl = getMUrl(mTUrl.decode())
        data = parseFileWithUrl(mTUrl.decode())
        sigBody, pubKeyBuf = makeSigBody(data, mUrl, mTUrl, ts, devNonce)
        ret = verifyMessage(sigBody, pubKeyBuf, sig)
        print("Verify", "Success" if ret == True else "Failed")
        if ret != True:
            # TODO: handle failure case
            exit(1)

        # Successfully verified
        exit(0)
