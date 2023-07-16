#pragma once

static double const MAX_RANGE = 2.0 * 1852.0;

#include <Windows.h>
#include <cstdint>
#include <time.h>

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

enum LampStatus
{
    LampOK         = 0,
    AzimuthFault   = 1,
    ElevationFault = 2,
    FocusFault     = 4,
    TempSensorFail = 8,
    Daylight       = 16,
    PowerLoss      = 32,
    NoLampFound    = 0x80
};

struct Ctx {
    uint8_t ctlProtectMask;
    HINSTANCE instance;
    HWND display, reqBrgValue, reqElevValue, actBrgValue, actElevValue, reqBrgValueLbl, reqElevValueLbl, actBrgValueLbl, actElevValueLbl;
    HWND actRngValue, actRngValueLbl, reqRngValue, reqRngValueLbl, console, portCtlButton, portSelector, instantModeSwitch;
    HWND lampOk, azimuthFault, elevationFault, focusFault, tempSensorFail, daylight, powerLoss;
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
    bool instantMode;
    clock_t lastCorrection;
    HANDLE locker, reader;
    std::vector<std::string> incomingStrings;
    bool keepRunning;
    uint8_t requestedFocus;
    uint8_t actualFocus;

    Ctx (
        uint8_t _ctlProtectMask,
        HINSTANCE _instance,
        double _mastHeight,
        double _actualBrg,
        double _actualElev,
        uint8_t _actualFocus,
        double _requestedBrg,
        double _requestedElev,
        uint8_t _requestedFocus
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
    locker (CreateMutex (0, 0, "LampSimLocker")),
    reader (0),
    keepRunning (false),
    requestedFocus (_requestedFocus),
    actualFocus (_actualFocus),
    portCtlButton (0),
    portSelector (0),
    instantModeSwitch (0),
    instantMode (false),

    outputFlags (OutputFlags::COPY_TO_CONCOLE /*| OutputFlags::FAKE_MODE*/) {
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
            unlock ();

            CloseHandle (locker);
        }
        if (reader) {
            if (WaitForSingleObject (reader, 1000) != WAIT_OBJECT_0) TerminateThread (reader, 0);
            CloseHandle (reader);
        }
        for (int i = 0; i < 7; DeleteObject (objects [i++]));
    }
    
    void protect (CtlProtectFlags flag) {
        ctlProtectMask |= flag;
    }
    void unprotect (CtlProtectFlags flag) {
        ctlProtectMask &= (~flag);
    }
    void lock () {
        if (locker) WaitForSingleObject (locker, INFINITE);
    }
    void unlock () {
        if (locker) ReleaseMutex (locker);
    }
};

void sendLampSentence (double brg, double elevation, uint32_t status, Ctx *ctx);
void getSerialPortsList (std::vector<std::string>& ports);
bool openPort (Ctx *ctx);
void startReader (Ctx *ctx);
void addToConsole (char *text, Ctx *ctx);
