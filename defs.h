#pragma once

static double const MAX_RANGE = 2.0 * 1852.0;

#include <Windows.h>
#include <cstdint>
#include <time.h>
#include <mutex>
#include <thread>

enum OutputFlags {
    FAKE_MODE = 1,
    COPY_TO_CONCOLE = 2,
};

enum CtlProtectFlags {
    REQ_BRG = 1,
    REQ_ELEV = 2,
    ACT_BRG = 4,
    ACT_ELEV = 8,
    REQ_RNG = 16,
    ACT_RNG = 32,
};

struct Ctx {
    uint8_t ctlProtectMask;
    HINSTANCE instance;
    HWND display, reqBrgValue, reqElevValue, actBrgValue, actElevValue, reqBrgValueLbl, reqElevValueLbl, actBrgValueLbl, actElevValueLbl;
    HWND actRngValue, actRngValueLbl, reqRngValue, reqRngValueLbl, console;
    RECT client;
    uint8_t outputFlags;
    HANDLE port;
    union {
        HGDIOBJ objects [7];
        struct {
            HBRUSH displayBrush, wndBrush, beamBrush, beamBrush2;
            HPEN borderPen, beamPen, beamPen2;
        };
    };
    double actualBrg;
    double actualElev;
    double requestedBrg;
    double requestedElev;
    double mastHeight;
    clock_t lastCorrection;
    std::mutex *locker;
    std::thread *reader;
    bool keepRunning;

    Ctx (
        uint8_t _ctlProtectMask,
        HINSTANCE _instance,
        double _mastHeight,
        double _actualBrg,
        double _actualElev,
        double _requestedBrg,
        double _requestedElev
    ):
    ctlProtectMask (_ctlProtectMask),
    instance (_instance),
    actualBrg (_actualBrg),
    actualElev (_actualElev),
    requestedBrg (_requestedBrg),
    requestedElev (_requestedElev),
    mastHeight (_mastHeight),
    lastCorrection (0),
    port (INVALID_HANDLE_VALUE),
    locker (0),
    reader (0),
    keepRunning (false),
    outputFlags (OutputFlags::COPY_TO_CONCOLE | OutputFlags::FAKE_MODE) {
        borderPen = CreatePen (PS_SOLID, 3, 0);
        displayBrush = CreateSolidBrush (RGB (100, 100, 100));
        wndBrush = CreateSolidBrush (RGB (200, 200, 200));
        beamPen = CreatePen (PS_SOLID, 1, RGB (255, 200, 0));
        beamPen2 = CreatePen (PS_SOLID, 1, RGB (200, 150, 0));
        beamBrush = CreateSolidBrush (RGB (255, 200, 0));
        beamBrush2 = CreateSolidBrush (RGB (200, 150, 0));
    }

    virtual ~Ctx () {
        if (locker) {
            locker->unlock ();

            delete locker; locker = 0;
        }
        if (reader) {
            if (reader->joinable ()) TerminateThread (reader->native_handle (), 0);

            delete reader;
        }
        for (int i = 0; i < 7; DeleteObject (objects [i++]));
    }
    
    void protect (CtlProtectFlags flag) {
        ctlProtectMask |= flag;
    }
    void unprotect (CtlProtectFlags flag) {
        ctlProtectMask &= (~flag);
    }
};

void sendLampSentence (double brg, double elevation, Ctx *ctx);
