NL OV Departures Board

# Departure board for Dutch Tram & Bus stop (OVapi)
ESP32 firmware to display upcoming trams &/or busses from a specified stop & direction, using live data from OVapi on an e-paper display.

## Overview

The project was built to answer a simple question reliably: when should I leave home to catch the next tram?

It implements a small, always-ready departure board using an ESP32 & e-paper display, showing upcoming departures & the remaining time until each service leaves.

The device wakes on demand, fetches live data, renders the display, & returns to deep sleep to maximize battery life.

### Features / steps
* Update process is triggered via reset or dedicated button (uses a normally closed switch)
* Wakes up from deep-sleep
* Auto-scans wifi & connects to the strongest of two predefined networks, it attempts to connect to second SSID if first fails
* Synchronizes system time via NTP
* Fetches live departure data over HTTP (HTTP GET with ETag &Last-Modified caching)
* Parses OVapi GTFS-derived JSON data
* Renders the results to an e-paper display (250 × 122 pixel)
* Partially updates display with diagnostics (e.g. shows if Wi-Fi, HTTP, time sync fails)
* Returns to deep sleep

### Hardware & Build Environment

Developed &tested using PlatformIO for:
* LilyGo T5 v2.3.2 dev board (ESP32-D0WDQ6) with built in 2.13" DEPG0213BN black & white e-paper (250 × 122 pixels)

### Dependencies 

Managed via PlatformIO:
* Adafruit GFX Library ^1.12.1
* GxEPD2 ^1.6.4
* ArduinoJson ^7.4.1

Note: DynamicJsonDocument is deprecated in newer ArduinoJson versions. Whilst JsonDocument should be used going forward, at the time of development I was unable to make this work & hence retained the deprecated approach.

### Limitations 

* No runtime configuration or on-device menu system (wifi credentials, stop, & direction are all defined at compile time)
* Only tram & bus services have been tested (no validation for metro, train, or ferry services - also no icon included)
* The codebase prioritizes functionality over any polish. No attempt has been made to clean it up. Parts were adapted from ESP32 examples, other projects & learning resources. Includes contributions from Copilot; some experimental or unused code remains, along with personal notes.

### Enhancement opportunities

* Use a normally closed switch (vs normally open as implemented) to possibly reduce power draw during deep sleep
* Button-activated temporary access-point mode
* Web-based configuration interface for stop &direction selection
* Status LED or haptic feedback for update progress
* Battery fuel-gauge integration
* Support for additional transport types (train, metro, ferry)

## Data & data handling

### Departure Time Logic

The firmware uses `ExpectedArrivalTime` as the effective departure time.

In practice, I observed that trams &buses typically depart 20–40 seconds after arrival. Since the display shows only hours &minutes (seconds are omitted), using the scheduled departure time risked missing a service. Using arrival time introduces a small, deliberate buffer that proved more reliable in day-to-day use.

## Timing Point Code (TPC)

A **Timing Point Code (TPC)** uniquely identifies a physical stop *and* direction.  
Most stops have two TPCs—one for each direction of travel.

Direction is indicated by the `LineDirection` field (typically `1` or `2`).

### Finding a TPC

1. Download `gtfs-nl.zip` from  
   https://gtfs.ovapi.nl/
2. Search for the stop name in `stops.txt`
3. Use the `stop_code` value as the TPC  
   (often zero-padded, e.g. `30000509`)
4. Validate using:  
   `http://v0.ovapi.nl/tpc`
5. Confirm that returned services match the desired stop &direction

Additional validation tools:

* https://drgl.nl – HTML departure boards &journey views

## Data Fields Used

The following fields are extracted &displayed or processed:

* `journeyNumber` (e.g. `387`)
* `LinePublicNumber` (e.g. `19`)
* `ExpectedArrivalTime` (ISO-8601 timestamp)
* `TripStopStatus` (`PLANNED`, `CANCEL`, `DRIVING`)
* `TransportType` (`BUS`, `TRAM`)
* `DestinationName50` (e.g. *Station Sloterdijk*)
* `DestinationCode` (e.g. `SLD`)
* `OperatorCode` (e.g. `GVB`)
* `TimingPointName` (e.g. *Arent Krijtsstraat*)


### License



## Links

### Data Source

* https://gtfs.ovapi.nl  
  OVapi GTFS data (`http://v0.ovapi.nl/tpc` endpoint used by this firmware)

### Other Dutch Public Transport Resources

* https://ndovloket.nl – NDOV Loket project
* https://drgl.nl – Live departure times &timetables (train, bus, tram, metro, ferry)

### Additional References

* Google GTFS Realtime specification  
  https://developers.google.com/transit/gtfs-realtime
* Tram & bus icons created using image2cpp  
  https://javl.github.io/image2cpp
* Thesis by I. Ivankovic that introduced OVapi usage  
  https://cs.vu.nl/~versto/VU-CS-BSc-MSc-Theses/VU-CS-BSc-Thesis-Veno-Ivankovic-2020.pdf

### Related Repositories

* OVAPI Bus Information for Home Assistant  
  https://github.com/william-sy/ovapi
* OVAPI TPC Finder  
  https://github.com/william-sy/ovapi-tpc-finder
* Public transport stop display for M5Paper  
  https://github.com/rosmo/m5paper-ovstops

