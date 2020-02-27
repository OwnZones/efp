#!/bin/sh

    awk '
/CutPointBridgeOFF/ { p = 0 }
p == 1 { print }
/CutPointBridgeON/ { p = 1 }
' ElasticFrameProtocol.h > efpbridge.h

    awk '
/CutPointBridgeON/ { p = 1 }
p == 0 { print }
/CutPointBridgeOFF/ { p = 0 }
' ElasticFrameProtocol.h > efpbridgeinternal.h
