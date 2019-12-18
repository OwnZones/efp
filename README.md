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
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## History

When working with media workflows both live and non-live we use different framing protocols such as MP4 and MPEG-TS, often transported over HTTP (TCP). Some of the protocols used for media transport are also tied to a certain underlying transport mechanism (RTMP, HLS, WebRTC…) , some are agnostic (MP4, MPEG-TS…). The protocols tied to an underlying transport mechanism forces the user to the behavior of that protocol’s properties, for example TCP when using RTMP. 
If you use MP4 as framing that is agnostic to the underlying transport and then select a protocol that delivers the data with low latency where you might lose data and the delivery might be out of order there is no mechanism to correct for that in the MP4-box domain. 

MPEG-TS is a common multiplexing standard for media. MPEG-TS on the other hand was designed in the mid 90’s for transport of media over ATM networks and was later also heavily used in the serial ASI interface. MPEG-TS solved a lot of transport problems present in the 1990’s world. 
However, MPEG-TS has not changed since then, and do not match modern IP protocols MTU well and has a high payload overhead. Some more modern underlying transport protocols also loses data and there might be out of order delivery of data. MPEG-TS was not built to handle that type of delivery behavior.

A common solution to cover for a protocols shortcoming is to stack protocols and framing structures on top of each other. That is one solution, however it drives complexity to the solution, ads overhead and sometimes delay. Many implementations are closed source and if not they could be of license types not wanted in your project.

Now with the rise of protocols such as RIST and SRT we wanted to fully utilize the transport containers with as little overhead as possible, we also implemented a thin network adaptation layer so that we can easily use the different transport protocols where they make most sense and maintain a well-defined data delivery pipeline to and from the data producers/consumers.

That’s why we developed the ElasticFrameProtocol. Please feel free to clone / fork and contribute to this new way of interconnecting media services between datacenters, internet and private networks.

---

Please read -> **ElasticFrameProtocol.pdf** for more information.

---

## Next steps

* DTS has been requested.. Is it needed? Should we implement it?
* Write examples
* Write more unit-tests

## Credits

The UnitX team at Edgeware AB

## License

*MIT*

Read *LICENCE.md* for details