import socket
import secrets
import codecs
import time
from mbedtls import pk
from datetime import datetime
from random import random

IP     = ''
PORT   = 10001
BUFFER_SIZE  = 1024

# Create a datagram socket
UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)

# Bind to address and ip
UDPServerSocket.bind((IP, PORT))
print("UDP server for temperature up and listening")


# Listen for incoming datagrams
while(True):
    bytesAddressPair = UDPServerSocket.recvfrom(BUFFER_SIZE)
    message = bytesAddressPair[0]
    address = bytesAddressPair[1]

    print("[%s] Received temperature is:" %time.ctime(time.time(), message)
