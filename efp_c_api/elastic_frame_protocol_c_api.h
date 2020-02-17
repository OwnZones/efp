//
// Created by Anders Cedronius on 2020-02-16.
//

#ifndef EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H
#define EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H

#include <stdint.h>

///Generate the uint32_t 'code' out of 4 characters provided
#define EFP_CODE(c0, c1, c2, c3) (((c0)<<24) | ((c1)<<16) | ((c2)<<8) | (c3))

/**
* efp_get_version
*
* @return 0xaabb where aa == major version and bb == minor version
*/
uint16_t efp_get_version();


/**
* efp_init_send
*
* @mtu Send MTU
* @*f Pointer to the send fragment function.
*/
uint64_t efp_init_send(uint64_t mtu, void (*f)(const uint8_t*, size_t, uint8_t));

/**
* efp_init_send
*
* @bucketTimeout Timout for the bucket in x * 10ms
* @holTimeout Timout for the hol in x * 10ms
* @*f Pointer to the got superframe.
*/
uint64_t efp_init_receive(uint32_t bucket_timeout, uint32_t hol_timeout,  void (*f)(uint8_t*, size_t, uint8_t, uint8_t, uint64_t, uint64_t, uint32_t, uint8_t, uint8_t, uint8_t));

/**
* efp_end
*
* @efp_object efp_object to end
*/
int16_t efp_end(uint64_t efp_object);

/**
* efp_end
*
* @efp_object efp_object to end
*/
int16_t efp_send_data(uint64_t efp_object,
                      const uint8_t *data,
                      size_t size,
                      uint8_t data_content,
                      uint64_t pts,
                      uint64_t dts,
                      uint32_t code,
                      uint8_t stream_id,
                      uint8_t flags);

int16_t efp_receive_fragment(uint64_t efp_object,
                             const uint8_t* fragment,
                             size_t size,
                             uint8_t from_source);



#endif //EFP_EFP_C_API_ELASTIC_FRAME_PROTOCOL_C_API_H
