# CC2500-Bridge

This repository will contain a collection of projects that all relate to using the CC2500 or similar to receive signals from a Dexcom G4.

Currently, the first project is to create a small and practical receiver that can be placed in the house by using a Wemos D1 mini which will relay the data to a nightscout site. 

The second stage is to use a nRF52 BLE chip which implements the CGMS bluetooth profile, and uses less power than the xBridge-wixel. This will first be based on ready mobules, but might eventually be implemented on a custom PCB.

I might also look into using the multiprotocol chip CC2541 in order to reduce size and power consumption even further. 
