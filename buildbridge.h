#!/bin/sh

    awk '
/CutPointBridgeOFF/ { p = 0 }
p == 1 { print }
/CutPointBridgeON/ { p = 1 }
' ElasticFrameProtocol.h > efpbridge.h
