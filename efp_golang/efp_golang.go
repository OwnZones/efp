package main

/*
#cgo CFLAGS: -g -Wall
#cgo darwin LDFLAGS: -L${SRCDIR}/efp_libs/darwin -lefp -lstdc++
#cgo linux LDFLAGS: -L${SRCDIR}/efp_libs/linux -lefp -lstdc++
#include <stdint.h>
#include "elastic_frame_protocol_c_api.h"
uint64_t initEFPSender(uint64_t mtu);
uint64_t initEFPReciever(uint32_t bucketTimeout, uint32_t holTimeout);
*/
import "C"
import (
	"fmt"
	"time"
	"unsafe"
)

const testSetSize = 10000
var thisData [testSetSize]uint8
var efpReceiveID C.ulonglong

func makeVector(){
	for i := 0; i < testSetSize; i++ {
		thisData[i] = uint8(i)
	}
}

func main() {

	makeVector()

	//Init a EFP sender MTU 300 bytes
	efpSendID := C.initEFPSender(300)

	//Start a EFP reciever (bucket time-out 100ms, HOL timeout 50ms)
	efpReceiveID = C.initEFPReciever(10,5)

	//Create a data block containing a null terminated string"
	stringtoEmbed := append([]byte("Embed this string"), byte(0))
	//Pass nill fot the destination. Then the return will be the size of the data block I need to allocate
	newDataSize := C.efp_add_embedded_data(nil,(*C.uchar)(&stringtoEmbed[0]),(*C.uchar)(&thisData[0]), (C.ulong)(len(stringtoEmbed)), testSetSize, 1, 1)
	fmt.Printf("Allocating : %d bytes for embedded data + payload\n", newDataSize)
	newData := make([]uint8, newDataSize)
	C.efp_add_embedded_data((*C.uchar)(&newData[0]),(*C.uchar)(&stringtoEmbed[0]),(*C.uchar)(&thisData[0]), (C.ulong)(len(stringtoEmbed)), testSetSize, 1, 1)
	//Send data
	//ID of EFP object to address
	//Pointer to data
	//Length of data
    //Data content (Check ElasticFrameProtocol.h ElasticFrameContentDefines)
    //PTS
    //DTS
    //EFP Code (see ElasticFrameContentDefines)
    //EFP Stream ID
    //Flags (See C++ header) 16 == embedded data
	C.efp_send_data(efpSendID,(*C.uchar)(&newData[0]),newDataSize,4,103,100,50,3,16)

	//Wait for 2 seconds before garbage collecting
	time.Sleep(2 * time.Second)

	//Garbage collect
	C.efp_end_send(efpSendID)
	C.efp_end_receive(efpReceiveID)
}

//export sendDataEFP
func sendDataEFP(data *C.uchar, size C.size_t, streamID uint8) {
	fmt.Printf("Send Fragment. \n")
	result := C.efp_receive_fragment(efpReceiveID, data, size, 0);
	 if (result < 0) {
		 fmt.Printf("Send fragment error \n")
	 }
}

//export gotEmbeddedDataEFP
func gotEmbeddedDataEFP(data *C.uchar, size C.size_t, dataType uint8, pts uint64) {
	fmt.Printf("Got embedded data size: %d and data type: %d\n", size, dataType)
	fmt.Printf("PTS: %d\n", pts)
	//In this example we know it's a C-String so we just cast it..
	fmt.Printf("This is the data: %s \n", C.GoString((*C.char)(unsafe.Pointer(data))))
	//But let's say it's the data you want then ->
	goData := C.GoBytes(unsafe.Pointer(data),  C.int(size))
	fmt.Printf("The data I converted is %d bytes\n", len(goData))
}


//export gotDataEFP
func gotDataEFP(data *C.uchar,
	size C.size_t,
	data_content uint8,
	broken uint8,
	pts uint64,
	dts uint64,
	code uint32,
	stream_id uint8,
	source uint8,
	flags uint8) {
	fmt.Printf("Got frame: \n")
	fmt.Printf("Size: %d\n",size)
	fmt.Printf("data_content: %d\n",data_content)
	fmt.Printf("broken: %d\n",broken)
	fmt.Printf("pts: %d\n",pts)
	fmt.Printf("dts: %d\n",dts)
	fmt.Printf("code: %d\n",code)
	fmt.Printf("stream_id: %d\n",stream_id)
	fmt.Printf("source: %d\n",source)
	fmt.Printf("flags: %d\n",flags)

	testsComplete := true

	if broken != 0 {
		fmt.Printf("Recieved frame is broken. \n")
		testsComplete = false
	}

	if size != testSetSize {
		fmt.Printf("Recieved frame size missmatch. \n")
		testsComplete = false
	}

	for i := 0; i < testSetSize; i++ {
		 if thisData[i] != uint8(i) {
			 fmt.Printf("Recieved frame data not correct. \n")
			 testsComplete = false
		 }
	}

	if testsComplete {
		fmt.Printf("Tests completed OK. \n")
	}

}

