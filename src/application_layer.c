// Application layer protocol implementation

#include <string.h>
#include "application_layer.h"
#include "link_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayer connectionParameters;
    LinkLayerRole role;

    connectionParameters.baudRate = baudRate;
    connectionParameters.nRetransmissions = nTries;
    connectionParameters.timeout = timeout;
    connectionParameters.serialPort = serialPort;

    char str1[] = "tx";
    if (strcmp(role, str1) == 0) {
        role = LlTx;
    }
    else {
        role = LlRx;
    }

    connectionParameters.role = role;

    if (role == LlTx) {

        llopen(connectionParameters);
    }
    else {
        int check = 0;
        unsigned char* res = malloc(sizeof(char) * 4000);
        while (check == 0) {
            check = llread(res, serialPort);
        }
    }

    return;
}
