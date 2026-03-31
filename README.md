# Embedded AI Benchmarking — Supplementary Materials

**Project:** A Comparative Study of Embedded AI Processors for Real-Time Vision Tracking in UAV and Assistive Monitoring Applications  
**Author:** Abdulrahman Al-Kuwari | Student ID: 24018696  
**Institution:** School of Engineering, University of the West of England (UWE), Bristol  
**Supervisor:** Dr. Dan Withey  
**Academic Year:** 2025/26

---

## About This Repository

This repository contains supplementary materials for the above Individual Project, including raw serial CSV data logs from all benchmarking runs and video recordings of the physical test setup.

---

## Platforms Tested

| Platform | Processor | Status |
|---|---|---|
| HuskyLens | Kendryte K210 (KPU) | ✅ Fully tested |
| Seeed XIAO ESP32-S3 Sense | ESP32-S3 (Xtensa LX7) | ✅ Fully tested |
| ESP32-S3 Zero + OV2640 | ESP32-S3 (Xtensa LX7) | 🔄 Testing in progress |
| Google Coral Dev Board Micro | Edge TPU (NXP RT1176) | ❌ USB flashing failure — documented as finding |

---

## Vision Tasks

Each platform was evaluated across four tasks under three conditions (Static / Negative Control / Dynamic, ~60 s each):

1. Face Detection
2. Object Classification (3 classes)
3. Object Tracking
4. Colour Block Detection (Red / Green / Blue)

---

## Repository Structure
```
/data
  /huskylens_csv       — Raw PuTTY serial logs for all HuskyLens runs
  /xiao_csv            — Raw PuTTY serial logs for all XIAO runs
/videos
  /huskylens           — Video recordings of HuskyLens test runs
  /xiao_esp32s3        — Video recordings of XIAO test runs
/firmware
  /huskylens_esp32s3zero  — Arduino sketch for HuskyLens I2C serial logging
  /xiao_esp32s3           — Arduino sketches for XIAO vision tasks
```

---

## Serial Log Format

All CSV files follow the standardised format used across all platforms:
```
time_ms, det_count, id/class_id, x, y, w, h
```

A valid detection is any row where `id >= 0` and `x, y, w, h >= 0`.  
No-detection rows are logged as `-1` across all fields.

---

## Reference

This repository is cited as a supplementary reference in the project report.  
All video recordings and CSV data logs are made available to support reproducibility and examiner review.
