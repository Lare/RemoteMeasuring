# RemoteMeasuring
This Arduino code reads temperature from onewire and sends it throgh a REST-api to server.
If network connection is not available, values are stored in local SD-card with timestamp.
When the network connection recovers, all data from SD-card will be moved through REST-api to server automatically.

Hardware required:
- Arduino Mega
- Ethernet2 shield
- DS1307RTC compatible RTC-chip
- At least one Dallas DS1820 compatible temperature sensor
- Suitable pull up resistor, for example 4.7k
