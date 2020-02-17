#include <stdio.h>
#include <unistd.h>
#include "elastic_frame_protocol_c_api.h"

uint8_t data[10000];
int efp_object_handle_receive;

void send_data_callback(const uint8_t* data, size_t size, uint8_t streamID) {
  //printf("Bombam!\n");
  int16_t result=efp_receive_fragment(efp_object_handle_receive, data, size, 0);
  if (result < 0) {
    printf("Error %d sending\n", result);
  } else if (result > 0) {
    printf("Notification %d sending\n", result);
  }
}

void receive_data_callback (uint8_t *pFrameData,
                       size_t mFrameSize,
                       uint8_t mDataContent,
                       uint8_t mBroken,
                       uint64_t mPts,
                       uint64_t mDts,
                       uint32_t mCode,
                       uint8_t mStreamID,
                       uint8_t mSource,
                       uint8_t mFlags) {
  printf("Got this data:\n");
  printf("mFrameSize: %zu\n",mFrameSize);
  printf("mDataContent: %d\n",mDataContent);
  printf("mPts: %llu\n",mPts);
  printf("mDts: %llu\n",mDts);
  printf("mCode: %d\n",mCode);
  printf("mStreamID: %d\n",mStreamID);
  printf("mSource: %d\n",mSource);
  printf("mFlags: %d\n",mFlags);
}

int main() {
  printf("EFP Version %d.%d \n",efp_get_version() >> 8, efp_get_version() & 0x00ff);

  //EFP Send
  printf("Create sender.\n");
  int efp_object_handle_send = efp_init_send(300, &send_data_callback);
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

  printf("\nTransmit data.\n\n");
  int16_t result = efp_send_data(efp_object_handle_send,&data[0],10000,10,100,100,100,2,0);
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