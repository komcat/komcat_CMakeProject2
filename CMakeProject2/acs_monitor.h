// acs_monitor.h
#pragma once

#include "ACSC.h"

class ACSMonitor {
public:
    ACSMonitor();
    ~ACSMonitor();

    void RenderUI();

private:
    HANDLE hComm;
    bool isConnected;
    char ipAddress[64];
    bool connectionAttempted;
    bool connectionSuccessful;
    bool motorEnabledX, motorEnabledY, motorEnabledZ;
    double xPos, yPos, zPos;
    double updateInterval = 0.1; // seconds between updates
    double lastUpdateTime = 0.0;

};
