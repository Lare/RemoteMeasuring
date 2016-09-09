# RemoteMeasuring
This Arduino code reads temperature from onewire and sends it throgh a REST-api to server.
If network connection is not available, values are stored in local SD-card with timestamp
When the network connection recovers, all data from SD-card will be moved through REST-api to server automatically.

