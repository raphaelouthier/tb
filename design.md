# Historical data types.

## Level 0

Event-driven effective trading data.

Composed of :
- time.
- best bid
- best ask
- cumulated volume of transactions.
- weighted average value of transactions.

## Level 1

Event-driven cumulated orderbook updates data.

Composed of :
- time.
- price tick
- total volume of pending orders at this tick. 

## Level 2

Event-driven order feed.

Composed of :
- time.
- order ID.
- trader ID.
- order type.
- order value.
- order volume.

# Time

## Timebase.

Time base is the nanosecond.

we can last 593 years with this timebase. It should be enough.


## Representing milliseconds.

one second : 1 000 : u16.
one minute : 60 000 : u16
one hour   : 3 600 000 : u32
one day    : 86 400 000 : u32
one year   : 31 622 400 000 : u64

# Storage considerations.

## bandwidth of the order feed

1 update per ms.

update 

-> 1000 updates per second.

## Update size.

1000 * 60 * 60 * 24 updates per day = 86400000 = 86.4 Mupdates per day.

update composed of :
- value, price tick : f64 or s64 : 8 bytes.
- volume : f64 or s64 : 8 bytes.
- timestamp : number of milliseconds since reference point.
  - if less than one minute : u16 : 2 bytes.
  - if less than 49 days : u32 : 4 bytes.
  - if more than 49 days : u64 : 8 bytes.

## Storage size.

nb = number of orders stored.

Storage layout :
- block 0 : metadata : 1 page.
- block 1 : sync : 1 page.
- block 2 : values : (8 * nb). 
- block 3 : volumes : (8 * nb). 
- block 4 : times : (8 * nb) (worst case).

page size : 64 KiB (worst case). 

Storage absolute size :
- 2 pages + 24 * nb (+ page alignment). 
- 2 * 64KiB + 24 * nb;

Sizes for various storages :
- one second : 64KiB + 24 * 1000 : 151.473 KiB. 
- one minute : 64KiB + 24 * 60000 : 1.498 MiB. 
- one hour : 64KiB + 24 * 3600000 : 82.522 MiB. 
- one day : 64KiB + 24 * 86400000 : 1.931 GiB. 
- one year (366 days) : 64KiB + 24 * 31622400000 : 706.815 GiB. 

## Segment sizes per level

Level 1 : (1 << 26) elements, around 5/6 day of data at 1ms data rate.

## Index size

Index is using segment lib -> cannot be resized on normal operation mode.

Level 1 : Objective : 50 years of data. 50 * 365 * (6/5) = 21900. -> 22K elements.


# Architecture

## Storage system.

### Objectives :
- allow storage of historical data of level 0, 1, and 2.
- allow multiple marketpalces and multiple instruments per marketplace. 
  - allow the same instrument to have different data feeds per marketplace.
- allow at most one fetcher per (marketplace,instrument).
- allow an arbitrary number of readers per (marketplace,instrument).
- allow simultaneous write(1)/read(N).
- support non-time-indexed levels.
- support and abstract fragmented storages.

### Components  

#### storage index

Describes the list of supported (marketplace,instrument) 

A directory-like structure does the job. No need for SW support.

#### (Marketplace/Instrument/level) (mil) index.

Describes :
- if anyone is writing the index. 
- if anyone is registered as a writer for this instrument.
- the covered time range for each storage file.

#### Storage file.

Contains raw data in accordance with what the MIL index states. 

Processed by level-dependent subsystems which support their own operations.

## Order data fetcher.

Fetches data from the marketplace and stores it into disk. 

Single entity for the whole storage system, multiple writers not supported.

### Objectives :
- interacts with a single marketplace.
- fetches data for a set of instruments.
- capable of starting / stopping to fetch data for instruments dynamically without reboot required.

### Components.

#### order feed connector

Provider-specific piece of code that receives market data feed, processes it, and forwards the result to the storage interface.

#### Storage interface (write). 

Receives formatted order data from the connector and stores them on disk in compliance with the storage format.

## Agent

Receives market data updates and reacts to them by passing and managing orders.

### Objectives.
- support arbitrary strategies.
- be non-time-based.
- support backtesting.

### Non objectives.
- be fault tolerant. Any fault should cause cancel of all pending orders, liquidation of all assets and restart from scratch.
- have a per-strategy order management. Predictions should be 

