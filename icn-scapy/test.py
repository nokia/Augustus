"""
  Lorenzo Saino, Massimo Gallo
 
  Copyright (c) 2016 Alcatel-Lucent, Bell Labs

"""
import icnscapy
from time import sleep

name = "a/b/c/d/e/f"

icnscapy.send_interest(name, chunk_id=1, iface="eth1")
sleep(0.05)
icnscapy.send_data(name, chunk_id=1, payload="this is a payload" ,iface="eth2")
