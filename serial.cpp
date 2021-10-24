#include <Windows.h>
#include <math.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <thread>
#include "defs.h"

const uint8_t ASCII_BEL = 0x07;
const uint8_t ASCII_BS = 0x08;
const uint8_t ASCII_LF = 0x0A;
const uint8_t ASCII_CR = 0x0D;
const uint8_t ASCII_XON = 0x11;
const uint8_t ASCII_XOFF = 0x13;

uint8_t calcCrc (char *sentence) {
    uint8_t crc = sentence [1];

    for (int i = 2; sentence [i] != '*'; crc ^= sentence [i++]);

    return crc;
}

void finalizeSendSentence (char *sentence, Ctx *ctx) {
    bool fakeMode = ctx->outputFlags & OutputFlags::FAKE_MODE;
    bool copyToConsole = ctx->outputFlags & OutputFlags::COPY_TO_CONCOLE;
    char tail [10];

    sprintf (tail, "%02X\r\n", calcCrc (sentence));
    strcat (sentence, tail);

    unsigned long bytesSent, size;

    //if (copyToConsole) addToConsole (sentence, ctx);

    if (!fakeMode && ctx->port != INVALID_HANDLE_VALUE) {
        size = strlen (sentence);
        if (ctx->locker) ctx->lock ();
        WriteFile (ctx->port, sentence, size, & bytesSent, 0);
        if (ctx->locker) ctx->unlock ();
    }
}

void sendLampSentence (double brg, double elevation, Ctx *ctx) {
    char sentence [100];
    sprintf (sentence, "$,01,%d,%.2f,100,0*", (int) brg, elevation);
    finalizeSendSentence (sentence, ctx);
}

uint8_t htodec (char chr) {
    if (chr >= '0' && chr <= '9') return chr - '0';
    if (chr >= 'A' && chr <= 'F') return chr - 'A' + 10;
    if (chr >= 'a' && chr <= 'f') return chr - 'a' + 10;
    return 0;
}

int splitFields (char *source, std::vector<std::string>& fields) {
    int count = 1;
    uint8_t actualCrc = calcCrc (source);

    fields.clear ();

    for (char *chr = source + 1, *begin = source; *chr; ++ chr) {
        if (*chr == ',') {
            fields.emplace_back (source, chr - begin - 1);
            begin = chr + 1;
        } else if (*chr == '*') {
            fields.emplace_back (source, chr - begin - 1);
            uint8_t crc = htodec (chr [1]) * 16 + htodec (chr [2]);

            if (crc != actualCrc) return 0;

            *chr = '\0';
        }
    }

    return fields.size ();
}

void parseCtlUnitData (char *source, Ctx *ctx) {
    std::vector<std::string> fields;
    int numOfFields = splitFields (source, fields);

    if (numOfFields > 4) {
        int lampID = atoi (fields [0].c_str () + 1);
        if (lampID != 1) {
            printf ("Invalid lamp %d\n", lampID); return;
        }

        ctx->requestedElev = atof (fields [2].c_str ());
        ctx->requestedBrg = atof (fields [1].c_str ());
        ctx->requestedFocus = std::atoi (fields [3].c_str ());
    }
}

void readAvailableData (Ctx *ctx) {
    unsigned long errorFlags, bytesRead;
    COMSTAT commState;
    char buffer [5000];

    do {
        printf ("reading...\n");
        ClearCommError (ctx->port, & errorFlags, & commState);

        bool overflow = (errorFlags & (CE_RXOVER | CE_OVERRUN)) != 0L;

        if (commState.cbInQue > 0 && ReadFile (ctx->port, buffer, commState.cbInQue, & bytesRead, NULL) && bytesRead > 0) {
            buffer [bytesRead] = '\0';

            if (*buffer) {
                if (ctx->outputFlags & OutputFlags::COPY_TO_CONCOLE) {
                    //ctx->incomingStrings.emplace_back (buffer);
                    addToConsole (buffer, ctx);
                }

                parseCtlUnitData (buffer, ctx);
            }

            Sleep (10);
        }
    } while (commState.cbInQue > 0);
}

DWORD readerProc (void *param) {
    Ctx *ctx = (Ctx *) param;
    while (ctx->keepRunning) {
        if ((ctx->outputFlags & OutputFlags::FAKE_MODE) == 0 && ctx->port != INVALID_HANDLE_VALUE) {
            if (ctx->locker) ctx->lock ();
            readAvailableData (ctx);
            if (ctx->locker) ctx->unlock ();
        }

        Sleep (10);
    }
    return 0;
}

void startReader (Ctx *ctx) {
    ctx->reader = CreateThread (0, 0, readerProc, ctx, 0, 0);
}

void getSerialPortsList (std::vector<std::string>& ports) {
    HKEY scomKey;
    int count = 0;
    DWORD error = RegOpenKeyEx (HKEY_LOCAL_MACHINE, "Hardware\\DeviceMap\\SerialComm", 0, KEY_QUERY_VALUE, & scomKey);

    if (error == S_OK) {
        char valueName[100], valueValue[100];
        unsigned long nameSize, valueSize, valueType;
        bool valueFound;

        ports.clear ();

        do {
            nameSize = sizeof (valueName);
            valueSize = sizeof (valueValue);
            valueFound = RegEnumValue (scomKey, (unsigned long) count ++, valueName, & nameSize, 0, & valueType, (unsigned char *) valueValue, & valueSize) == S_OK;

            if (valueFound) ports.push_back (valueValue);
        } while (valueFound);

        RegCloseKey (scomKey);
    }
}

bool openPort (Ctx *ctx) {
    auto selection = SendMessage (ctx->portSelector, CB_GETCURSEL, 0, 0);
    auto portNo = SendMessage (ctx->portSelector, CB_GETITEMDATA, selection, 0);
    char portName [100];
    sprintf (portName, "\\\\.\\COM%Id", portNo);
    
    ctx->port = CreateFile (portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);

    bool result = ctx->port != INVALID_HANDLE_VALUE;

    if (result) {
        DCB dcb;

        SetupComm (ctx->port, 4096, 4096);
        PurgeComm (ctx->port, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

        memset (& dcb, 0, sizeof (dcb));

        GetCommState (ctx->port, & dcb);

        dcb.BaudRate = CBR_115200;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fBinary = 1;
        dcb.fParity = 1;
        dcb.fInX =
        dcb.fOutX = 1;
        dcb.XonChar = ASCII_XON;
        dcb.XoffChar = ASCII_XOFF;
        dcb.XonLim = 100;
        dcb.XoffLim = 100;

        if (!SetCommState (ctx->port, & dcb)) {
            CloseHandle (ctx->port);

            ctx->port = INVALID_HANDLE_VALUE;
            result = false;
        }
    }

    return result;
}
