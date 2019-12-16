![alt text](https://bitbucket.org/unitxtra/efp/raw/690a192cf7ce9420cad999ad113b1b4246d9c2fd/elasticframe.png)

# ElasticFrameProtocol

TODO: Write a project description

## Installation

Requires cmake version >= **3.10** and **C++11**

###Release
```sh
cmake -DCMAKE_BUILD_TYPE=Release
make
```

###Debug
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

Pleas help us build other platforms. 

---


## Usage

The EFP class can be made a reciever or sender. This is configured during creation as decribed below.

##Sender

```cpp
// The callback function referenced as 'data sender'
void sendData(const std::vector<uint8_t> &subPacket) {
//send the subPacket data 
//UDP.send(subPacket);
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
myEFPSender.packAndSend(myData, ElasticFrameContent::adts, 0, 'ANXB', 2, NO_FLAGS);

```

##Reciever

```cpp

// The callback function referenced as 'data reciever'
// This callback will be called from a separate thread owned by EFP
// rPacket contains a pointer to the data and the size of the data
// content is containing the content descriptor
// broken is true if data in the superFrame is missing
// pts contains the pts value used in the superFrame
// code contains the code sent (if used)
// stream is the stream number to associate to the data
// flags contains the flags used by the superFrame
void gotData(ElasticFrameProtocol::pFramePtr &rPacket, ElasticFrameContent content, bool broken, uint64_t pts, uint32_t code, uint8_t stream, uint8_t flags) {
// use the data in your application
// if you spend too long time here you log the queue between EFP and you. This can lead to data loss
if EFP runs out of buffer space. 
}

// Create your receiver
ElasticFrameProtocol myEFPReceiver();

// Register the callback
myEFPReceiver.receiveCallback = std::bind(&gotData,
											std::placeholders::_1,
											std::placeholders::_2,
                                    		std::placeholders::_3,
                                    		std::placeholders::_4,
                                    		std::placeholders::_5,
                                    		std::placeholders::_6,
                                    		std::placeholders::_7);

// Start the reciever worker
myEFPReceiver.startReceiver(5, 2);

// Receive a EFP fragment
myEFPReceiver.receiveFragment(subPacket,0);

//When done stop the worker
myEFPReceiver.stopReciever();

```



## Contributing

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## History

TODO: Write history

## Credits

The UnitX team at Edgeware AB

## License

*MIT*

Read *LICENCE.md* for details