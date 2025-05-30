![Logo](elasticframe.png)

# ElasticFrameProtocol

The ElasticFrameProtocol is a bridge between data producers/consumers and the underlying transport protocol.

```
---------------------------------------------------------   /\
| Data type L | Data type L | Data type F | Data type Q |  /  \
---------------------------------------------------------   ||
|                   ElasticFrameProtocol                |   ||
---------------------------------------------------------   ||
| Network layer: UDP, TCP, SRT, RIST, Zixi, SCTP, aso.  |  \  /
---------------------------------------------------------   \/

```

The elasticity comes from the protocols ability to adapt to incoming frame size, type, number of concurrent streams and underlying infrastructure. This layer is kept thin without driving overhead, complexity and delay.


#Reasons for using EFP

For us there are a couple of reasons that sparked the initial design. And why we (and others) use EFP today.

1.    We wanted to create a framing layer decoupled from the underlying transport and agnostic to the payload. The benefit we see is, without re-writing the application and all its features we’re able to change / select the underlying transport anywhere in the solution.

2.    We wanted to create a thin-framing structure providing better mapping to IP protocols compared to MPEG-TS (initially designed for mapping to ATM) and MP4 designed targeting files. EFP lowers the overhead from several % to just about 0.5% overhead.

3.    We wanted to be able to use precise monotonically increasing timestamps. EFP has 64-bit time stamps so one can use absolute TAI time stamps, instead of relative time that wraps every 26 hours as in MPEG-TS.

4.    We wanted features like dynamic load balancing when bonding interfaces, 1+N protection and splitting of elementary data streams without being bound to a specific transport protocols features. We see this as an important feature when building a robust infrastructure. Some transport protocols miss that (part of EFPBond).

5.   We want to correctly detect the data integrity of the transported data. MPEG-TS has 4 bits CC. That is not good enough when transporting data over IP networks. It was designed to detect bit-errors in transport protocols from the 90's, then 4 bits CC works great.

6.    We needed something that enables us to transport elementary data payload dynamically. Something that is using the duplex nature of IP networks for pub/sub applications where we can declare content and subscribe to content in the elementary stream level (Part of EFPSignal).

7.    We needed a framing structure that is capable of inserting timing critical messaging (type0 frames) we need that for transporting exact time. We use it for media-timing (NTP is not possible to use in all locations since UDP port 123 is sometimes blocked).

8.    We wanted a what we think is a simple super-clean C++ interface since we’re a C++ team. We also expose a C interface for other languages to use.

9.    We wanted a solution that is of a license type that can be used in commercial products without us having to expose our code. For example Apple do not allow dynamic linking in iOS applications making GPL licence types unwanted.

10.   We wanted something open source and free of charge.

11.    We wanted something written in a portable language so we can target any system and any architecture.


Your needs might be different than ours. Or you might use already existing protocols that meet the above requirements/features. If that is the case, please let us know as we do not want to re-invent the wheel.
Anyhow, the above requirements are why we designed and implemented EFP.


Please read -> [**ElasticFrameProtocol**](https://github.com/OwnZones/efp/blob/master/docs/ElasticFrameProtocol.pdf) for more information.

## Notification about version 0.3

Version 0.3 adds a run-to-completion mode, see unit test 20 for implementation details.

The internal delivery mechanism has changed to absolute relative time-outs instead of counters, this changes the behaviour (to the better) compared to older version of EFP when using EFP over lossy infrastructure.

Version 0.3 also implements an optional context to be used in all callbacks. Please see Unit test 19 for details.

**This version changes the API for the receiver!!**

X == milliseconds before timing out non-complete frames.

Y == milliseconds before moving the head forward in HOL mode. (set to 0 for disabling HOL)

ElasticFrameProtocolReceiver myEFPReceiver(X, Y, (optional context), (optional set run to completion mode));



## Current badge status

(Click the badge to get to the underlying information)

**Build**

[![efp_base_macos](https://github.com/OwnZones/efp/workflows/efp_base_macos/badge.svg)](https://github.com/OwnZones/efp/actions?query=workflow%3Aefp_base_macos) **(MacOS build)**

[![efp_base_win](https://github.com/OwnZones/efp/workflows/efp_base_win/badge.svg)](https://github.com/OwnZones/efp/actions?query=workflow%3Aefp_base_win) **(Windows 10 build)**

[![efp_base_ubuntu](https://github.com/OwnZones/efp/workflows/efp_base_ubuntu/badge.svg)](https://github.com/OwnZones/efp/actions?query=workflow%3Aefp_base_ubuntu) **(Ubuntu build)**

**Code quality**

[![CodeFactor](https://www.codefactor.io/repository/github/OwnZones/efp/badge)](https://www.codefactor.io/repository/github/OwnZones/efp)

**Code scanning alerts**

[![CodeQL](https://github.com/OwnZones/efp/workflows/CodeQL/badge.svg?branch=master)](https://github.com/OwnZones/efp/security/code-scanning)

[![deepcode](https://www.deepcode.ai/api/gh/badge?key=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJwbGF0Zm9ybTEiOiJnaCIsIm93bmVyMSI6IlVuaXQtWCIsInJlcG8xIjoiZWZwIiwiaW5jbHVkZUxpbnQiOmZhbHNlLCJhdXRob3JJZCI6MjE5MTYsImlhdCI6MTU5NzkzMzY5MX0.VMWvZfxEBy8Ib23oONlN65tNZUrubUqQt6eUnMIiWrA)](https://www.deepcode.ai/app/gh/OwnZones/efp/_/dashboard?utm_content=gh%2FOwnZones%2Fefp)

**Tests**

[![unit_tests](https://github.com/OwnZones/efp/workflows/unit_tests/badge.svg?branch=master)](https://github.com/OwnZones/efp/actions?query=workflow%3Aunit_tests) **(Unit tests running on Ubuntu)**

**Issues**

[![Percentage of issues still open](http://isitmaintained.com/badge/open/OwnZones/efp.svg)](http://isitmaintained.com/project/OwnZones/efp "Percentage of issues still open")

[![Average time to resolve an issue](http://isitmaintained.com/badge/resolution/OwnZones/efp.svg)](http://isitmaintained.com/project/OwnZones/efp "Average time to resolve an issue")

## Installation

Requires cmake version >= **3.10** and **C++17**

**Release:**

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

***Debug:***

```sh
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build . --config Debug
```

Output:

**(platform specific)efp.(platform specific)** (Linux/MacOS -> libefp.a)

The static EFP library

**(platform specific)efp_shared.(platform specific)**

The dynamic EFP library

**runUnitTestsEFP**

*runUnitTestsEFP* (executable) runs through the unit tests and returns EXIT_SUCCESS if all unit tests pass.

The unit test framework is built using Google Test.
On Linux, install the following package to be able to run it:
`sudo apt install libgtest-dev`
---

**EFP** Is built on Ubuntu, Windows10 and MacOS every commit by us.


---


## Usage

The EFP class/library can be made a receiver or sender. This is configured during creation as described below.

The unit test

**Sender:**

```cpp
// The send fragment callback -> 'sendCallback'
void sendData(const std::vector<uint8_t> &subPacket, uint8_t lStreamID, (optional context.. see below)) {
// Send the fragment data
// UDP.send(subPacket);
}

// The data to be sent using EFP
std::vector<uint8_t> lData;

// Create your sender passing the MTU of the underlying protocol.
// You may also provide additional callback context please see unit test 19.
ElasticFrameProtocolSender myEFPSender(MTU, (optional context));

// Register your callback sending the packets
// The callback will be called on the same thread calling 'packAndSend'
//optionally also a std::placeholders::_2 if you want the EFP streamID
myEFPSender.sendCallback = std::bind(&sendData, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 (optional if context is used));

// Send the data
// param1 = The data
// param2 = The data type
// param3 = PTS
// param4 = DTS
// param5 = CODE (if the data type param2 (uint8_t) msb is set then CODE must be used
// See the header file detailing what CODE should be set to
// param6 = Stream number (uint8_t) a unique value for that EFP-Stream
// param7 = FLAGS (used for various signalling in the protocol)
// param8 = Optional lambda (See unit test 18)
myEFPSender.packAndSend(myData, ElasticFrameContent::h264, 0, 0, EFP_CODE('A', 'N', 'X', 'B'), 2, NO_FLAGS, (optional lambda));

//If you got your data as a pointer there is also the method 'packAndSendFromPtr' so you don't have to copy your data into a vector first.


```

**Receiver:**

```cpp

// The callback function referenced as 'receiveCallback'
// This callback will be called from a separate thread owned by EFP
// rFrame is a pointer to the data object
// Containing:
// pFrameData pointer to the data
// mFrameSize Size of the data
// mDataContent is containing the content descriptor
// mBroken is true if data in the superFrame is missing
// mPts contains the pts value used in the superFrame
// mDts contains the pts value used in the superFrame
// mCode contains the code sent (if used)
// mStreamID is the value passed to the receiveFragment method
// mStream is the stream number to associate to the data
// mFlags contains the flags used by the superFrame
void gotData(ElasticFrameProtocol::pFramePtr &rFrame, (optional context))
{
			// Use the data in your application
}

// Create your receiver
// Passing fragment time out and HOL time out if wanted else set HOL to 0
// 50 == 50 ms before timing out not receiving all fragments for a frame
// 20 == 20 ms before moving the head in HOL mode forward.
ElasticFrameProtocolReceiver myEFPReceiver(50, 20);

// Register the callback
myEFPReceiver.receiveCallback = std::bind(&gotData, std::placeholders::_1, std::placeholders::_2 (optional));

// Receive a EFP fragment
myEFPReceiver.receiveFragment(subPacket, uint8_t number (is the mStreamID for stream separation) );

//If you got your data as a pointer there is also the method 'receiveFragmentFromPtr' so you don't need to copy your data into a vector first.

```

## Using EFP in your CMake project

* **Step1**

Add this in your CMake file.

```
#Include EFP
include(ExternalProject)
ExternalProject_Add(project_efp
        GIT_REPOSITORY https://github.com/OwnZones/efp.git
        GIT_SUBMODULES ""
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efp
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/efp
        GIT_PROGRESS 1
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/efp --config ${CMAKE_BUILD_TYPE} --target efp
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(efp STATIC IMPORTED)
set_property(TARGET efp PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/efp/libefp.a)
add_dependencies(efp project_efp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/efp/)
```

* **Step2**

Link your library or executable.

```
target_link_libraries((your target) efp (the rest you want to link))
```

* **Step3**

Add header file to your project.

```
#include "ElasticFrameProtocol.h"
```

You should now be able to use EFP in your project and use any CMake supported IDE

## Plug-in

EFP is all about framing data and checking the integrity of the content. For other functionality EFP uses plug-ins. Available plug-ins are listed below.

[**EFPBonding**](https://github.com/OwnZones/efpbond)

EFPBond makes it possible for all streams to use multiple underlying transport interfaces for protection or to increase the capacity.

[**EFPSignal**](https://github.com/OwnZones/efpsignal)

EFPSignal adds signalling, content declaration and dynamic/static subscription to EFP-Streams.

## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Make your additions and write a UnitTest testing it/them.
4. Commit your changes: `git commit -am 'Add some feature'`
5. Push to the branch: `git push origin my-new-feature`
6. Submit a pull request :D

## History

When working with media workflows, both live and non-live, we use framing protocols such as MP4 and MPEG-TS, often transported over HTTP (TCP). Some of the protocols used for media transport are also tied to a certain underlying transport mechanism (RTMP, HLS, WebRTC…), and some are agnostic to the underlying transport (MP4, MPEG-TS…). The protocols tied to an underlying transport type forces the user to the behavior of that protocol’s properties, for example, TCP when using RTMP. If you use MP4 as framing agnostic to the underlying transport and then transport the data using a protocol where you might lose data and the delivery might be out of order, there is no mechanism to correct for that in the MP4-box domain.

For those situations, MPEG-TS has traditionally been used and is a common multiplexing standard for media. However, MPEG-TS, was designed in the mid ’90s for the transport of media over ATM networks and was later also heavily used in the serial ASI interface. MPEG-TS solved a lot of transport problems in the 1990’s where simplex transport was common and data integrity looked different. However, MPEG-TS has not changed since then, it does not match modern IP protocols well and it has a high protocol overhead. Some of today’s underlying transport protocols also lose data and there might be out of order delivery of data. MPEG-TS was not built to handle that type of delivery behaviour. Another deficiency of
MPEG-TS is its 33-bits time stamps which wrap every 26 hours and are used to carry a system reference time. Similarly, RTP has 32 bits time stamps. EFP uses 64 bits and can therefore carry monotonically increasing time stamps like TAI with high precision.

There has been work done in the MPEG group to modernize media/data framing using MMT (MPEG Media Transport) for better adaption against underlying transport. MMT is currently used in the ATSC 3.0 standard but has not gained popularity in the data center/cloud/internet domain.

Another common solution to cover for a protocol’s shortcomings is to stack protocols and framing structures on top of each other. However, this drives complexity to the solution, ads overhead and sometimes delay. Many implementations are closed source and, if they aren’t, they are often of license types that are unwanted in commercial products.

Now with the rise of protocols such as RIST, Zixi, and SRT we wanted to fully utilize the transport containers with as little overhead as possible, so we implemented a thin network adaptation layer that allows us to easily use different transport protocols where they make most sense maintaining a well-defined data delivery pipeline to and from the data producers/consumers.

That’s why we developed ElasticFrameProtocol, we are so enthusiastic about where RIST, Zixi, and SRT is taking the future of broadcast.
There are new open source projects putting these building blocks together, creating new ways of working and transporting media all the time.  We would like to simplify the way of building media solutions even more by open sourcing the layer on top of the transport protocols so that you can focus on developing great services instead.

Please feel free to use, clone / fork and contribute to this new way of interconnecting media services between datacenters, internet and private networks in your next project or lab.


## Examples


1. A client/server using EFP over SRT

[EFP + SRT Client/Server](https://github.com/agilecontent/efp_srt_example)

2. A example showing how to use the EBPBond plug-in

[EFP + EFPBond + SRT](https://github.com/agilecontent/efp_srt_bonding_example)

3. A simple example showing how to map UDP -> MPEG-TS -> EFP

[UDP -> MPEG-TS -> EFP](https://github.com/agilecontent/ts2efp)


## Next steps

* Add opportunistic embedded data
* Write more examples
* Write more unit-tests

## Credits

The Ateliere Live development team.

## License

*MIT*

Read *LICENSE* for details
