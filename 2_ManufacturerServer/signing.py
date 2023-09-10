import socket
import secrets
from codecs import encode, decode
from mbedtls import pk
from hashlib import sha256
from datetime import datetime
from random import random
import pyshorteners
from urllib import request
from time import time

signBody = "device_id:19682938\n" + \
    "device_status:active\n" + \
    "device_type:blinking led\n" + \
    "sensors:null\n" + \
    "actuators:led\n" + \
    "network:wifi\n" + \
    "purpose:null\n" + \
    "manufacturer:paisa\n" + \
    "full_specification_link:null\n" + \
    "user_manual_link:null\n" + \
    "location:null\n" + \
    "description:sample application, blinking led\n" + \
    "certificate_of_device:-----BEGIN CERTIFICATE-----\nMIICIjCCAccCFGZPVLCboKoM68PB4YmEIHl4QA8FMAoGCCqGSM49BAMCMIGHMQsw\nCQYDVQQGEwJVUzELMAkGA1UECAwCQ0ExDzANBgNVBAcMBklydmluZTESMBAGA1UE\nCgwJVUMgSXJ2aW5lMRwwGgYDVQQLDBNUcnVzdGVkIFRoaXJkIFBhcnR5MQwwCgYD\nVQQDDANUVFAxGjAYBgkqhkiG9w0BCQEWC3R0cEB1Y2kuZWR1MB4XDTIzMDIxNzAw\nMzUwMVoXDTI0MDIxNzAwMzUwMVowgZ0xCzAJBgNVBAYTAlVTMQswCQYDVQQIDAJD\nQTEPMA0GA1UEBwwGSXJ2aW5lMRIwEAYDVQQKDAlVQyBJcnZpbmUxDzANBgNVBAsM\nBlNQUk9VVDEoMCYGA1UEAwwfR29vZ2xlIE5lc3QgTGVhcm5pbmcgVGhlcm1vc3Rh\ndDEhMB8GCSqGSIb3DQEJARYSdGhlcm1vc3RhdEB1Y2kuZWR1MFkwEwYHKoZIzj0C\nAQYIKoZIzj0DAQcDQgAEuQnbuq0OifGY0Fb9TlVw+Y8wXX28TiW+Yq38CIx5sVgh\nlTjBmuFhm0yBJr5L88OHBd9ymb3S5idXq0EStfbv3TAKBggqhkjOPQQDAgNJADBG\nAiEAo1TfNcMSJwWzwrNgj/7O+BSTZOLxzdYJLADfjxpeBvMCIQDLOyyTXzWZAIbI\nGh4COT12hCo6ENgUx6LtH5L+40V1Ew==\n-----END CERTIFICATE-----\n"

SecKeyOfSrvFileName = 'keys/ttp_sec.pem'
PubKeyOfSrvFileName = 'keys/ttp_pub.pem'


def signMessage(msg):
    ecc = pk.ECC()
    ecc = ecc.from_file(SecKeyOfSrvFileName)

    return ecc.sign(msg)

def verifyMessage(msg, sig):
    ecc = pk.ECC()
    ecc = ecc.from_file(PubKeyOfSrvFileName)

    t = time()
    ret = ecc.verify(msg, sig)
    print("[Time][Verify] %.6f sec" %(time()-t))
    return ret

print(signBody)
sig = encode(signMessage(signBody.encode()), 'base64')
print(sig)
