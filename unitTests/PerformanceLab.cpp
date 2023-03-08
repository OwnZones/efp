//PerformanceLab just send packets / recieve and profile the code

#include <memory>

#include "ElasticFrameProtocol.h"
#include "UnitTestHelpers.h"

int main() {
    uint8_t streamID = 1;
    std::unique_ptr<ElasticFrameProtocolReceiver> myEFPReceiver = std::make_unique<ElasticFrameProtocolReceiver>(5, 2);
    std::unique_ptr<ElasticFrameProtocolSender> myEFPPacker = std::make_unique<ElasticFrameProtocolSender>(MTU);
    myEFPPacker->sendCallback = [&](const std::vector<uint8_t> &subPacket, uint8_t, ElasticFrameProtocolContext *) {
        ElasticFrameMessages info = myEFPReceiver->receiveFragment(subPacket, 0);
        if (info != ElasticFrameMessages::noError) {
            std::cout << "Failed to receive fragment" << std::endl;
        }
    };
    size_t receivedPackage = 0;
    myEFPReceiver->receiveCallback = [&](ElasticFrameProtocolReceiver::pFramePtr &, ElasticFrameProtocolContext *) {
        if (!(++receivedPackage % 100)) {
            std::cout << "Got packet number " << unsigned(receivedPackage) << std::endl;
        }
    };

    std::vector<uint8_t> mydata;
    uint64_t packetNumber = 0;
    while (true) {
        mydata.clear();
        size_t randSize = rand() % 1000000 + 1;
        mydata.resize(randSize);
        ElasticFrameMessages result = myEFPPacker->packAndSend(mydata, ElasticFrameContent::h264, packetNumber,
                                                               packetNumber, EFP_CODE('A', 'N', 'X', 'B'), streamID,
                                                               NO_FLAGS);
        packetNumber++;
        if (result != ElasticFrameMessages::noError) {
            std::cout << "Failed to send fragment" << std::endl;
        }
    }
    return EXIT_SUCCESS;
}