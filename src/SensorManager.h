#pragma once
/*
 * SensorManager.h
 * ---------------------------------------------------------
 * Wraps the VL53L0X Time-of-Flight sensor and turns raw
 * millimetre readings into a debounced SpatialEvent
 * (CLEAR / OBSTACLE / EDGE).
 *
 * This class only *interprets* — it never touches motors,
 * emotions or lights. BehaviourEngine decides what to do
 * with a SpatialEvent.
 *
 * The sensor is mounted at a downward angle and is used for
 * BOTH obstacle detection and edge/drop-off detection, per
 * the calibration table in the project reference doc. These
 * two thresholds MUST be recalibrated on the physical chassis.
 * ---------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include "Types.h"
#include "Config.h"

class SensorManager {
public:
    SensorManager();

    // Returns false if the sensor failed to initialise (e.g. not
    // wired / wrong address). Callers should treat that as "no
    // spatial awareness available" rather than crashing.
    bool begin();

    // Call every loop(). Internally rate-limited to
    // TOF_POLL_INTERVAL_MS so it never blocks the main loop.
    void update();

    SpatialEvent getEvent() const;
    uint16_t getLastDistanceMm() const;
    bool isHealthy() const;

private:
    Adafruit_VL53L0X tof;
    bool sensorOk;

    unsigned long lastPollTime;
    uint16_t lastDistanceMm;

    SpatialEvent currentEvent;
    uint8_t obstacleStreak;
    uint8_t edgeStreak;

    SpatialEvent classify(uint16_t mm, bool valid);
};
