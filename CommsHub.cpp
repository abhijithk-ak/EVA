#include "CommsHub.h"
#include "Logger.h"

namespace {
    bool isMovementChar(char c) {
        switch (c) {
            case 'F': case 'B': case 'L': case 'R':
            case 'G': case 'I': case 'H': case 'J': case 'S':
                return true;
            default:
                return (c >= '0' && c <= '9');
        }
    }
}

CommsHub::CommsHub(ModeEngine &modeEngineRef, ClockService &clockRef, MovementEngine &movementRef)
    : modeEngine(modeEngineRef), clock(clockRef), movement(movementRef),
      rcSpeed(MOVE_DEFAULT_SPEED), lastRcCommandTime(0) {}

void CommsHub::begin() {
    bt.begin(BT_DEVICE_NAME);
    EVA_LOGF("[CommsHub] Bluetooth Serial '%s' ready\n", BT_DEVICE_NAME);
}

bool CommsHub::isConnected() {
    return bt.hasClient();
}

void CommsHub::update() {
    while (bt.available()) {
        handleByte((char)bt.read());
    }
    rcSafetyWatchdog();
}

void CommsHub::handleByte(char c) {
    if (c == '\n' || c == '\r') {
        if (lineBuffer.length() > 0) {
            executeTextCommand(lineBuffer);
            lineBuffer = "";
        }
        return;
    }

    // A raw movement character is always an immediate RC command when
    // no command line is currently being assembled. This avoids the
    // common case where a single app packet arrives with more bytes
    // queued behind it, which previously got misclassified as text.
    if (lineBuffer.length() == 0 && isMovementChar(c)) {
        executeMovementChar(c);
        return;
    }

    lineBuffer += c;
    if (lineBuffer.length() > 40) {
        lineBuffer = ""; // guard against a runaway/garbled line
    }
}

void CommsHub::executeMovementChar(char c) {
    // Drive input only ever reaches the motors while RC mode is
    // actually active - a stray character in any other mode is
    // silently ignored rather than accidentally moving EVA.
    if (modeEngine.getCurrentId() != MODE_RC) return;

    lastRcCommandTime = millis();

    switch (c) {
        case 'F': movement.drive(MoveState::FORWARD, rcSpeed); break;
        case 'B': movement.drive(MoveState::BACKWARD, rcSpeed); break;
        case 'L': movement.drive(MoveState::PIVOT_LEFT, rcSpeed); break;
        case 'R': movement.drive(MoveState::PIVOT_RIGHT, rcSpeed); break;
        case 'G': movement.drive(MoveState::CURVE_LEFT, rcSpeed); break;
        case 'I': movement.drive(MoveState::CURVE_RIGHT, rcSpeed); break;
        case 'H': movement.drive(MoveState::PIVOT_LEFT, rcSpeed); break;
        case 'J': movement.drive(MoveState::PIVOT_RIGHT, rcSpeed); break;
        case 'S': movement.stop(); break;
        default:
            if (c >= '0' && c <= '9') {
                rcSpeed = map(c - '0', 0, 9, 120, 255);
            }
            break;
    }
}

void CommsHub::executeTextCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    String upper = cmd;
    upper.toUpperCase();

    if (upper == "MODE EVA")        { modeEngine.switchTo(MODE_EVA); }
    else if (upper == "MODE PET")   { modeEngine.switchTo(MODE_PET); }
    else if (upper == "MODE CLOCK") { modeEngine.switchTo(MODE_CLOCK); }
    else if (upper == "MODE SLEEP") { modeEngine.switchTo(MODE_SLEEP); }
    else if (upper == "MODE RC")    { modeEngine.switchTo(MODE_RC); }
    else if (upper.startsWith("TIME ")) {
        bool ok = clock.setManualTimeHHMM(cmd.substring(5));
        EVA_LOGF("[CommsHub] TIME set %s\n", ok ? "OK" : "FAILED (expected HH:MM)");
    }
    else if (upper == "ALARM OFF") {
        clock.disableAlarm();
    }
    else if (upper.startsWith("ALARM ")) {
        // Expected: "ALARM HH:MM ON"
        String rest = cmd.substring(6);
        int sp = rest.indexOf(' ');
        if (sp > 0) {
            String hhmm = rest.substring(0, sp);
            String state = rest.substring(sp + 1);
            state.trim();
            state.toUpperCase();
            if (hhmm.length() == 5 && hhmm.charAt(2) == ':' && state == "ON") {
                uint8_t hh = (uint8_t)hhmm.substring(0, 2).toInt();
                uint8_t mm = (uint8_t)hhmm.substring(3, 5).toInt();
                if (hh <= 23 && mm <= 59) {
                    clock.setAlarm(hh, mm, true);
                }
            }
        }
    }
    else {
        EVA_LOGF("[CommsHub] Unknown command: %s\n", cmd.c_str());
    }
}

void CommsHub::rcSafetyWatchdog() {
    // If the Bluetooth link drops mid-drive, stop rather than keep
    // driving forever on the last received instruction.
    if (modeEngine.getCurrentId() == MODE_RC && movement.isMoving() &&
        (millis() - lastRcCommandTime > 1500)) {
        movement.stop();
    }
}
