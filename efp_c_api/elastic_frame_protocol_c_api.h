//
// Created by Anders Cedronius on 2020-02-16.
//

#ifndef EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H
#define EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H

#include <stdint.h>

///Generate the uint32_t 'code' out of 4 characters provided
#define EFP_CODE(c0, c1, c2, c3) (((c0)<<24) | ((c1)<<16) | ((c2)<<8) | (c3))

uint16_t efp_get_version();
int efp_init_send(int mtu, void (*f)(const uint8_t*, size_t, uint8_t));
int efp_init_receive(uint32_t bucketTimeout, uint32_t holTimeout,  void (*f)(uint8_t*, size_t, uint8_t, uint8_t, uint64_t, uint64_t, uint32_t, uint8_t, uint8_t, uint8_t));
int16_t efp_end(int efp_object);

int16_t efp_send_data(int efp_object,
                      const uint8_t *rPacket,
                      size_t packetSize,
                      uint8_t dataContent,
                      uint64_t pts,
                      uint64_t dts,
                      uint32_t code,
                      uint8_t streamID,
                      uint8_t flags);

int16_t efp_receive_fragment(int efp_object,
                             const uint8_t* pSubPacket,
                             size_t packetSize,
                             uint8_t fromSource);



#endif //EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H
