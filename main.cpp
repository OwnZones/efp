#include "EdgewareFrameProtocol.h"

#include <iostream>

#define MTU 1456 //SRT-max

EdgewareFrameProtocol myEFPReciever;

int dropCount;

void sendData(const std::vector<uint8_t> &subPacket) {


   // std::cout << "Send data of size ->" << subPacket.size() << " Packet type: " << unsigned(subPacket[0]) << std::endl;


    if (!(++dropCount%300)) {
        std::cout << "Drop!" << std::endl;
        return;
    }
    myEFPReciever.unpack(subPacket);

}

void gotData(const std::vector<uint8_t> &packet, EdgewareFrameContent content, bool broken) {

    std::cout << "Got data size ->" << packet.size() << " Content type: " << unsigned(content) << " Broken->" << broken << std::endl;
}

int main() {

    dropCount=0;

    EdgewareFrameProtocol myEFPPacker(MTU);
    myEFPPacker.sendCallback = std::bind(&sendData, std::placeholders::_1);

    myEFPReciever.recieveCallback = std::bind(&gotData, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    //startUnpacker(
    // timeout not fully recieved frames ms,
    // if there is head-of-line packets blocking ready frames.. )
    myEFPReciever.startUnpacker(10,2);

    for (int i=0;i<40;i++) {

        int dataSize = rand() % 40000 + 70;
        std::vector<uint8_t> mydata(dataSize);
        std::generate(mydata.begin(), mydata.end(), [n = 0] () mutable { return n++; });
        std::cout << "Pack and send: " << mydata.size() << " bytes."  << std::endl;

        EdgewareFrameMessages result = myEFPPacker.packAndSend(mydata, EdgewareFrameContent::adts);

        if (result != EdgewareFrameMessages::noError) {
            std::cout << "Error: " << result << std::endl;
        }
    }

    sleep(1);
    myEFPReciever.stopUnpacker();
    std::cout << "Hello, World!" << std::endl;
    return 0;
}