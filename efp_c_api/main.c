#include <stdio.h>

#ifndef _WIN64
#include <unistd.h>
#endif

#include "elastic_frame_protocol_c_api.h"

#define TEST_MTU 300
#define TEST_DATA_SIZE 10000

uint8_t data[TEST_DATA_SIZE];
uint64_t efp_object_handle_receive;

void send_data_callback(const uint8_t* data, size_t size, uint8_t stream_id) {
  int16_t result=efp_receive_fragment(efp_object_handle_receive, data, size, 0);
  if (result < 0) {
    printf("Error %d sending\n", result);
  } else if (result > 0) {
    printf("Notification %d sending\n", result);
  }
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
  printf("Got this data:\n");
  printf("mFrameSize: %zu\n",size);
  printf("mDataContent: %d\n",data_content);
  printf("mBroken: %d\n",broken);
  printf("mPts: %llu\n",pts);
  printf("mDts: %llu\n",dts);
  printf("mCode: %d\n",code);
  printf("mStreamID: %d\n",stream_id);
  printf("mSource: %d\n",source);
  printf("mFlags: %d\n\n",flags);

  int test_failed=0;

  if (size != TEST_DATA_SIZE) {
    printf("Test failed. Size mismatch.\n");
    test_failed = 1;
  }

  for(int x = 0 ; x < TEST_DATA_SIZE ; x++){
    if (data[x] != (uint8_t)x) {
      printf("Test failed. Data vector missmatch.\n");
      test_failed = 1;
    }
  }

  if (!test_failed) {
    printf("C-API tests passed.\n");
  }
}

int main() {
  printf("EFP Version %d.%d \n",efp_get_version() >> 8, efp_get_version() & 0x00ff);

  //EFP Send
  printf("Create sender.\n");
  uint64_t efp_object_handle_send = efp_init_send(TEST_MTU, &send_data_callback);
  if (!efp_object_handle_send) {
    printf("Fatal. Failed creating EFP sender");
    return 1;
  }
  printf("Sender created.\n");

  //EFP Recieve
  printf("Create reciever.\n");
  efp_object_handle_receive = efp_init_receive(10,5,&receive_data_callback);
  if (!efp_object_handle_receive) {
    printf("Fatal. Failed creating EFP reciever");
    return 1;
  }
  printf("Reciever created.\n");

  //Prepare data
  for(int x = 0 ; x < TEST_DATA_SIZE ; x++){
    data[x] = (uint8_t)x;
  }

  printf("\nTransmit data.\n\n");
  int16_t result = efp_send_data(efp_object_handle_send,&data[0],TEST_DATA_SIZE,10,100,100,100,2,0);
  if (result < 0) {
    printf("Error %d sending\n", result);
  } else if (result > 0) {
    printf("Notification %d sending\n", result);
  }
  sleep (1);

  result = efp_end(efp_object_handle_send);
  if (result < 0) {
    printf("Error %d efp_end\n", result);
  } else if (result > 0) {
    printf("Notification %d efp_end\n", result);
  }
  result = efp_end(efp_object_handle_receive);
  if (result < 0) {
    printf("Error %d efp_end\n", result);
  } else if (result > 0) {
    printf("Notification %d efp_end\n", result);
  }

  return 0;
}