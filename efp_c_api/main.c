#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN64
#include <windows.h>
#else

#include <unistd.h>

#endif

#include "elastic_frame_protocol_c_api.h"

#define TEST_MTU 300
#define TEST_DATA_SIZE 10000

char *embedd_me = "This is a embedded string";

uint8_t data[TEST_DATA_SIZE];
uint64_t efp_object_handle_receive;

int drop_counter = 0;
int first_frame_broken_counter = 0;

void send_data_callback(const uint8_t *data, size_t size, uint8_t stream_id) {

    drop_counter++;
    if (drop_counter == 5) {
        //Drop the ^ fragment
        return;
    }

    int16_t result = efp_receive_fragment(efp_object_handle_receive, data, size, 0);
    if (result < 0) {
        printf("Error %d sending\n", result);
    } else if (result > 0) {
        printf("Notification %d sending\n", result);
    }
}

void receive_embedded_data_callback(uint8_t *data, size_t size, uint8_t data_type, uint64_t pts) {
    printf("Got embedded data: %zu bytes size and of type %d pts: %llu\n", size, data_type, pts);
    //In this example we know it's a string, print it.
    printf("Data: %s \n\n", data);
}

void receive_data_callback(uint8_t *data,
                           size_t size,
                           uint8_t data_content,
                           uint8_t broken,
                           uint64_t pts,
                           uint64_t dts,
                           uint32_t code,
                           uint8_t stream_id,
                           uint8_t source,
                           uint8_t flags) {

    if (first_frame_broken_counter == 0 && broken) {
        printf("The first frame is broken. We know that. Let's not parse it since we don't know the integrity\n");
        first_frame_broken_counter++;
        return;
    } else if (first_frame_broken_counter == 0 && !broken) {
        printf("Test failed. First frame not broken.\n");
        return;
    }

    printf("Got this data:\n");
    printf("mFrameSize: %zu\n", size);
    printf("mDataContent: %d\n", data_content);
    printf("mBroken: %d\n", broken);
    printf("mPts: %llu\n", pts);
    printf("mDts: %llu\n", dts);
    printf("mCode: %d\n", code);
    printf("mStreamID: %d\n", stream_id);
    printf("mSource: %d\n", source);
    printf("mFlags: %d\n\n", flags);

    int test_failed = 0;

    if (size != TEST_DATA_SIZE) {
        printf("Test failed. Size mismatch.\n");
        test_failed = 1;
    }

    for (int x = 0; x < TEST_DATA_SIZE; x++) {
        if (data[x] != (uint8_t) x) {
            printf("Test failed. Data vector missmatch.\n");
            test_failed = 1;
        }
    }

    if (!test_failed) {
        printf("C-API tests passed.\n");
    }
}

int main() {
    printf("EFP Version %d.%d \n", efp_get_version() >> 8, efp_get_version() & 0x00ff);

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
    efp_object_handle_receive = efp_init_receive(10, 5, &receive_data_callback, &receive_embedded_data_callback);
    if (!efp_object_handle_receive) {
        printf("Fatal. Failed creating EFP reciever");
        return 1;
    }
    printf("Reciever created.\n");

    //Prepare data
    for (int x = 0; x < TEST_DATA_SIZE; x++) {
        data[x] = (uint8_t) x;
    }

    printf("\nEmbedd data.\n\n");
    size_t alloc_size = efp_add_embedded_data(NULL, (uint8_t *) embedd_me, &data[0], strlen(embedd_me) + 1,
                                              TEST_DATA_SIZE, 1, 1);
    uint8_t *sendThisData = (uint8_t *) malloc(alloc_size);
    efp_add_embedded_data(sendThisData, (uint8_t *) embedd_me, &data[0], strlen(embedd_me) + 1, TEST_DATA_SIZE, 1, 1);

    printf("\nTransmit data.\n\n");

    //So this frame contains inline embedded payload. This is signaled using INLINE_PAYLOAD in C++ in the C-APi there are no defines so we need
    //to follow the C++ api 0b00010000 == 16

    //The first time we send the data. We will drop a fragment.
    //The expected behaviour is that the embedded data callback should not be triggered and the
    //EFP (receive_data_callback) should be called with broken set.
    int16_t result = efp_send_data(efp_object_handle_send, sendThisData, alloc_size, 10, 100, 100,
                                   EFP_CODE('A', 'V', 'C', 'C'), 2, 16);
    if (result < 0) {
        printf("Error %d sending\n", result);
    } else if (result > 0) {
        printf("Notification %d sending\n", result);
    }

    //This time we don't drop anything.. We should receive the embedded data and the EFP frame with broken == 0
    result = efp_send_data(efp_object_handle_send, sendThisData, alloc_size, 10, 100, 100, EFP_CODE('A', 'V', 'C', 'C'),
                           2, 16);
    if (result < 0) {
        printf("Error %d sending\n", result);
    } else if (result > 0) {
        printf("Notification %d sending\n", result);
    }

    free(sendThisData);

#ifndef _WIN64
    sleep(1);
#else
    Sleep(1000);
#endif

    result = efp_end_send(efp_object_handle_send);
    if (result < 0) {
        printf("Error %d efp_end\n", result);
    } else if (result > 0) {
        printf("Notification %d efp_end\n", result);
    }
    result = efp_end_receive(efp_object_handle_receive);
    if (result < 0) {
        printf("Error %d efp_end\n", result);
    } else if (result > 0) {
        printf("Notification %d efp_end\n", result);
    }

    return 0;
}