#include <Windows.h>
#include <math.h>
#include <stdio.h>
#include <cstdint>
#include <vector>
#include <thread>
#include "defs.h"

uint8_t calcCrc (char *sentence) {
    uint8_t crc = sentence [1];

    for (int i = 2; sentence [i] != '*'; crc ^= sentence [i++]);

    return crc;
}

void addToConsole (char *text, Ctx *ctx) {
    auto item = SendMessage (ctx->console, LB_ADDSTRING, 0, (LPARAM) text);

    SendMessage (ctx->console, LB_SETCURSEL, item, 0);
}

void finalizeSendSentence (char *sentence, Ctx *ctx) {
    bool fakeMode = ctx->outputFlags & OutputFlags::FAKE_MODE;
    bool copyToConsole = ctx->outputFlags & OutputFlags::COPY_TO_CONCOLE;
    char tail [10];

    sprintf (tail, "%02X\r\n", calcCrc (sentence));
    strcat (sentence, tail);

    unsigned long bytesSent;

    if (copyToConsole) addToConsole (sentence, ctx);

    if (!fakeMode) {
        if (ctx->locker) ctx->locker->lock ();
        WriteFile (ctx->port, sentence, strlen (sentence), & bytesSent, 0);
        if (ctx->locker) ctx->locker->unlock ();
    }
}

void sendLampSentence (double brg, double elevation, Ctx *ctx) {
    char sentence [100];
    sprintf (sentence, "$PSMACK,01,%.1f,%.2f,100,00*", brg, elevation);
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

void parseLampFeedback (char *source, Ctx *ctx) {
    std::vector<std::string> fields;
    int numOfFields = splitFields (source, fields);

    if (numOfFields > 4) {
        int lampID = atoi (fields [0].c_str () + 1);
        if (lampID != 1) {
            printf ("Invalid lamp %d\n", lampID); return;
        }

        ctx->requestedElev = atof (fields [2].c_str ());
        ctx->requestedBrg = atof (fields [1].c_str ());
    }
}

void readAvailableData (Ctx *ctx) {
    unsigned long errorFlags, bytesRead;
    COMSTAT commState;
    char buffer [5000];

    do {
        ClearCommError (ctx->port, & errorFlags, & commState);

        bool overflow = (errorFlags & (CE_RXOVER | CE_OVERRUN)) != 0L;

        if (commState.cbInQue > 0 && ReadFile (ctx->port, buffer, commState.cbInQue, & bytesRead, NULL) && bytesRead > 0) {
            buffer [bytesRead] = '\0';

            if (*buffer) {
                if (ctx->outputFlags & OutputFlags::COPY_TO_CONCOLE) addToConsole (buffer, ctx);

                parseLampFeedback (buffer, ctx);
            }

            std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
    } while (commState.cbInQue > 0);
}

void readerProc (Ctx *ctx) {
    while (ctx->keepRunning) {
        if ((ctx->outputFlags & OutputFlags::FAKE_MODE) == 0) {
            if (ctx->locker) ctx->locker->lock ();
            readAvailableData (ctx);
            if (ctx->locker) ctx->locker->unlock ();
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}