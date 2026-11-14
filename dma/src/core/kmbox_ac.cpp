#include "kmbox_ac.h"

KmboxAcTuning KmboxAcTuningFor(KmboxAcProfile profile) {
    switch (profile) {
    case KmboxAcProfile::Be:
        // BE usermode stack in-repo has no HID strings; keep conservative serial
        // pacing like vendor B+ ~300 calls/sec ceiling.
        return {
            14, 12, 6, 6, 100, 100, true,
        };
    case KmboxAcProfile::Ace:
        // ACE-CORE names hidusb/hidparse/VID_&PID — stock gadget only; avoid
        // 8K and serial floods that look unlike a normal full-speed HID path.
        return {
            18, 10, 8, 5, 85, 110, true,
        };
    case KmboxAcProfile::RobRun:
    case KmboxAcProfile::Eac:
    default:
        // EAC EOS kernel dump has no HID inventory; Rob Run is EAC. Limit
        // km.move bursts (vendor docs: B+ serial white-screen if too fast).
        return {
            20, 10, 8, 5, 90, 120, true,
        };
    }
}
