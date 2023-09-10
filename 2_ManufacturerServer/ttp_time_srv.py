import socket
import secrets
import codecs
from mbedtls import pk
from datetime import datetime
from random import random
from time import time

IP     = ''
PORT   = 10000
BUFFER_SIZE  = 1024
DEV_PUB_FILE = 'keys/dev_0_pub.pem'
M_SRV_PRV_FILE = 'keys/ttp_sec.pem'
NONCE_SIZE = 32
TIME_SIZE = 4
ID_SIZE = 4

# Create a datagram socket
UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# Bind to address and ip
UDPServerSocket.bind((IP, PORT))
print("UDP server up and listening")

def parse_syncReq(msg):
    #print('[', len(msg), ']msg: ', msg)

    id_dev = msg[ : ID_SIZE]
    n1_dev = msg[ID_SIZE : ID_SIZE+NONCE_SIZE]
    time_prev = msg[ID_SIZE+NONCE_SIZE : ID_SIZE+NONCE_SIZE+TIME_SIZE]
    sig = msg[ID_SIZE+NONCE_SIZE+TIME_SIZE : ]
    sig_body = msg[ : ID_SIZE+NONCE_SIZE+TIME_SIZE]

    #print('[', len(n1_dev),']n1_dev: ', n1_dev)
    #print('[', len(time_prev),']time_prev: ', time_prev)
    #print('[', len(id_dev),']id_dev: ', id_dev)
    #print('[', len(sig), ']sig: ', sig)

    #print('')

    return id_dev, n1_dev, time_prev, sig, sig_body


def parse_syncAck(msg):
    id_dev = msg[ : ID_SIZE]
    n2_dev = msg[ID_SIZE : ID_SIZE+NONCE_SIZE]
    n1_m_srv = msg[ID_SIZE+NONCE_SIZE : ID_SIZE+NONCE_SIZE*2]
    time_cur = msg[ID_SIZE+NONCE_SIZE*2 : ID_SIZE+NONCE_SIZE*2+TIME_SIZE]
    sig = msg[ID_SIZE+NONCE_SIZE*2+TIME_SIZE : ]
    sig_body = msg[ : ID_SIZE+NONCE_SIZE*2+TIME_SIZE]

    return sig_body, sig


def verify_message(msg, sig):
    ecc = pk.ECC()
    ecc = ecc.from_file(DEV_PUB_FILE)

    t = time()
    ret = ecc.verify(msg, sig)
    #print("[Time][Verify] %.6f sec" %(time()-t))
    return ret

def signMessage(msg):
    ecc = pk.ECC()
    t = time()
    ecc = ecc.from_file(M_SRV_PRV_FILE)
    #print("[Time][Sign] %.6f sec" %(time()-t))

    return ecc.sign(msg)

def verify_time(time_prev):
    time_prev = int.from_bytes(time_prev, 'little');
    now = time()

    return now > time_prev

def prepare_syncResp(id_dev, n1_dev):
    n1_m_srv = secrets.token_bytes(nbytes=NONCE_SIZE)
    now = time()
    time_cur = int(now).to_bytes(4, 'little')
    msg = id_dev + n1_dev + n1_m_srv + time_cur

    #print('[', len(msg), '] msg: ', msg)
    #print('[Before] encoded msg: ', codecs.encode(msg, 'hex'))

    msg += signMessage(msg)
    msg = len(msg).to_bytes(4, 'little') + msg

    #print('[', len(msg), '] msg: ', msg)
    #print('[After] encoded msg: ', codecs.encode(msg, 'hex'))
    return msg

# Listen for incoming datagrams
while(True):
    bytesAddressPair = UDPServerSocket.recvfrom(BUFFER_SIZE)
    message = bytesAddressPair[0]
    address = bytesAddressPair[1]

    t = time()
    if message[-6:] == b"MSGEND":
        message = message[:-6]
        id_dev, n1_dev, time_prev, sig, sig_body = parse_syncReq(message)
        ret = verify_time(time_prev)
        if ret != True:
            print("[SyncReq] Verify Time ", "Success" if ret == True else "Failed")
            exit(1)
        
        ret = verify_message(sig_body, sig)
        print("[SyncReq] Verify Message", "Success" if ret == True else "Failed")
        if ret != True:
            exit(1)

        message = prepare_syncResp(id_dev, n1_dev)

        #print("message: ", message)
        #print("address: ", address)

        # Sending a reply to client
        res = UDPServerSocket.sendto(message, address)
        #print("[R]%.6f sec" %(time()-t))
        print("[SyncResp] Send Message", "Success" if ret == True else "Failed")
        if ret != True:
            exit(1)

    elif message[-6:] == b"ACKEND":
        message = message[:-6]
        sig_body, sig = parse_syncAck(message)
        ret = verify_message(sig_body, sig)
        print("[SyncAck] Verify", "Success" if ret == True else "Failed")
        #print("[A]%.6f sec" %(time()-t))
        pass

