package main

/*
#include "stdio.h"
#include <stdint.h>
#include "elastic_frame_protocol_c_api.h"

extern void sendDataEFP(const uint8_t*, size_t, uint8_t);
extern void gotDataEFP(uint8_t *data,
                       size_t size,
                       uint8_t data_content,
                       uint8_t broken,
                       uint64_t pts,
                       uint64_t dts,
                       uint32_t code,
                       uint8_t stream_id,
                       uint8_t source,
                       uint8_t flags);

void send_data_callbackGO(const uint8_t* data, size_t size, uint8_t stream_id) {
	sendDataEFP(data, size, stream_id);
}

void receive_data_callback (uint8_t *data,
                       size_t size,
                       uint8_t data_content,
                       uint8_t broken,
                       uint64_t pts,
                       uint64_t dts,
                       uint32_t code,
                       uint8_t stream_id,
                       uint8_t source,
                       uint8_t flags) {
	gotDataEFP(data, size, data_content, broken, pts, dts, code, stream_id, source, flags);
}

uint64_t initEFPSender(uint64_t mtu) {
	return efp_init_send(mtu,&send_data_callbackGO);
}

uint64_t initEFPReciever(uint32_t bucketTimeout, uint32_t holTimeout) {
	return efp_init_receive(bucketTimeout,holTimeout,&receive_data_callback);
}
 */
import "C"


