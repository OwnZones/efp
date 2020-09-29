package main

/*
//This code is bridging the exported golang methods and EFP callbacks
#include "stdio.h"
#include <stdint.h>
#include "elastic_frame_protocol_c_api.h"

//The exported golang callback functions
extern void sendDataEFP(const uint8_t*, size_t, uint8_t, void* ctx);
extern void gotEmbeddedDataEFP(uint8_t *data, size_t size, uint8_t data_type, uint64_t pts, void* ctx);
extern void gotDataEFP(uint8_t *data,
                       size_t size,
                       uint8_t data_content,
                       uint8_t broken,
                       uint64_t pts,
                       uint64_t dts,
                       uint32_t code,
                       uint8_t stream_id,
                       uint8_t source,
                       uint8_t flags,
					   void* ctx);

//init sender
uint64_t initEFPSender(uint64_t mtu, void* ctx) {
	return efp_init_send(mtu, &sendDataEFP, ctx);
}

//init receiver
uint64_t initEFPReciever(uint32_t bucketTimeout, uint32_t holTimeout, void* ctx, uint32_t mode) {
	return efp_init_receive(bucketTimeout, holTimeout, &gotDataEFP, &gotEmbeddedDataEFP, ctx, mode);
}
 */
import "C"


