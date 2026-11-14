#pragma once
#include <cstdint>

// Host-side KMBox serial pacing vs EAC / BE / ACE expectations.
// Kernel BYOVD IOCTLs are unrelated. ACE dump overlap is HID/usbccgp/VID_
// on the game PC; these knobs only limit serial burst and move chunk size.

enum class KmboxAcProfile : int {
    Eac = 0,
    Be,
    Ace,
    RobRun, // alias tuning for Rob Run (EAC EOS class titles)
};

struct KmboxAcTuning {
    int maxDeltaPerMove = 20;
    int moveCurveSteps = 10;
    int segmentAbove = 8;
    int minMsBetweenMove = 5;
    int buttonPollStaleMs = 90;
    int maxClampPerAxis = 120;
    bool segmentedMove = true;
};

KmboxAcTuning KmboxAcTuningFor(KmboxAcProfile profile);
