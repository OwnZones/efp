![alt text](https://bitbucket.org/unitxtra/efp/raw/690a192cf7ce9420cad999ad113b1b4246d9c2fd/elasticframe.png)

# ElasticFrameProtocol

The ElasticFrameProtocol is acting as a bridge between elementary data and the underlying transport protocol.

```
---------------------------------------------------------   /\
| Data type 1 | Data type 2 | Data type 3 | Data type 4 |  /  \
---------------------------------------------------------   ||
|                   ElasticFrameProtocol                |   ||
---------------------------------------------------------   ||
| Network layer: UDP, TCP, SRT, RIST aso.               |  \  /
---------------------------------------------------------   \/

```

The elasticity comes from the protocols ability to adapt to incoming frame size, type, number of concurrent streams and underlying infrastructure. Due to it’s elastic behavior the layer between the transport layer and producers/consumers of the data can be kept thin without driving overhead, complexity and delay. 

Please read -> **ElasticFrameProtocol.pdf** for more information.


## Installation

Requires cmake version >= **3.10** and **C++11**

**Release:**

```sh
cmake -DCMAKE_BUILD_TYPE=Release
make
```

***Debug:***

```sh
cmake -DCMAKE_BUILD_TYPE=Debug
make
```

Output: 

**libefp.a**

The static EFP library 

**efptests**

*efptests* (executable) runs trough the unit tests and returns EXIT_SUCESS if all unit tests pass.

---

**EFP** is built on Ubuntu 18.04 in the bitbucket [pipeline](https://bitbucket.org/unitxtra/efp/addon/pipelines/home).

By us on MacOS using C-Lion.

Please help us build other platforms. 

---


## Usage

The EFP class/library can be made a reciever or sender. This is configured during creation as decribed below.

**Sender:**

```cpp
// The callback function referenced as 'sendCallback'
void sendData(const std::vector<uint8_t> &subPacket) {
// Send the subPacket data 
// UDP.send(subPacket);
}

// The data to be sent
std::vector<uint8_t> myData;

// Create your sender passing the MTU of the underlying protocol and set EFP to mode sender
ElasticFrameProtocol myEFPSender(MTU, ElasticFrameProtocolModeNamespace::sender);

// Register your callback sending the packets
// The callback will be called on the same thread calling 'packAndSend'
myEFPSender.sendCallback = std::bind(&sendData, std::placeholders::_1);

// Send the data
// param1 = The data
// param2 = The data type
// param3 = PTS
// param4 = CODE (if the data type param2 (uint8_t) msb is set then CODE must be used
// See the header file detailing what CODE should be set to
// param5 = Stream number (uint8_t) a unique value for that EFP-Stream
// param6 = FLAGS (used for various signaling in the protocol) 
myEFPSender.packAndSend(myData, ElasticFrameContent::h264, 0, 'ANXB', 2, NO_FLAGS);

```

**Reciever:**

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
// mCode contains the code sent (if used)
// mStream is the stream number to associate to the data
// mFlags contains the flags used by the superFrame
void gotData(ElasticFrameProtocol::pFramePtr &rFrame)
{
			// Use the data in your application 
}

// Create your receiver
ElasticFrameProtocol myEFPReceiver();

// Register the callback
myEFPReceiver.receiveCallback = std::bind(&gotData, std::placeholders::_1);

// Start the reciever worker
myEFPReceiver.startReceiver(5, 2);

// Receive a EFP fragment
myEFPReceiver.receiveFragment(subPacket,0);

// When done stop the worker
myEFPReceiver.stopReciever();

```



## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Make your additions and write a UnitTest testing it/them.
4. Commit your changes: `git commit -am 'Add some feature'`
5. Push to the branch: `git push origin my-new-feature`
6. Submit a pull request :D

## History

When working with media workflows, both live and non-live, we use framing protocols such as MP4 and MPEG-TS, often transported over HTTP (TCP). Some of the protocols used for media transport are also tied to a certain underlying transport mechanism (RTMP, HLS, WebRTC…), and some are agnostic to the underlying transport (MP4, MPEG-TS…). The protocols tied to an underlying transport type forces the user to the behavior of that protocol’s properties, for example, TCP when using RTMP. If you use MP4 as framing agnostic to the underlying transport and then transport the data using a protocol where you might lose data and the delivery might be out of order, there is no mechanism to correct for that in the MP4-box domain.  

For those situations, MPEG-TS has traditionally been used and is a common multiplexing standard for media. However, MPEG-TS, was designed in the mid ’90s for the transport of media over ATM networks and was later also heavily used in the serial ASI interface. MPEG-TS solved a lot of transport problems in the 1990’s where simplex transport was common and data integrity looked different. However, MPEG-TS has not changed since then, it does not match modern IP protocols well and it has a high protocol overhead. Some of today’s underlying transport protocols also lose data and there might be out of order delivery of data. MPEG-TS was not built to handle that type of delivery behavior. 

There has been work done in the MPEG group to modernize media/data framing using MMT (MPEG Media Transport) for better adaption against underlying transport. MMT is currently used in the ATSC 3.0 standard but has not gained popularity in the data center/cloud/internet domain. 

Another common solution to cover for a protocol’s shortcomings is to stack protocols and framing structures on top of each other. However, this drives complexity to the solution, ads overhead and sometimes delay. Many implementations are closed source and, if they aren’t, they are often of license types that are unwanted in commercial products.  

Now with the rise of protocols such as RIST, Zixi, and SRT we wanted to fully utilize the transport containers with as little overhead as possible, so we implemented a thin network adaptation layer that allows us to easily use different transport protocols where they make most sense maintaining a well-defined data delivery pipeline to and from the data producers/consumers. 

That’s why we developed ElasticFrameProtocol, we are so enthusiastic about where RIST, Zixi, and SRT is taking the future of broadcast. There are new open source projects putting these building blocks together, creating new ways of working and transporting media all the time.   

Please feel free to use, clone / fork and contribute to this new way of interconnecting media services between datacenters, internet and private networks in your next project or lab. 


---

Please read -> **ElasticFrameProtocol.pdf** for detailed information.

---

## Examples



1. [EFP + SRT Client/Server](https://bitbucket.org/unitxtra/cppsrtframingexample/src/master/)


## Next steps

* DTS has been requested.. Is it needed? Should we implement it? 
* Add opportunistic embedded data
* Write more examples
* Write more unit-tests

## Credits

The UnitX team at Edgeware AB

## License

*MIT*

Read *LICENCE.md* for details