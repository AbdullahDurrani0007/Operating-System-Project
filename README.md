# AirControlX - Automated Air Traffic Control System

## Overview

**AirControlX** is a simulation of an automated Air Traffic Control System (ATCS) for a multi-runway international airport. It demonstrates core operating system concepts including process management, threading, synchronization (mutexes, semaphores), scheduling, and inter-process communication (IPC). The system also features real-time analytics, violation tracking, airline management, and graphical simulation of aircraft movements.

## Features

* Multi-airline and multi-flight simulation with six distinct airlines
* Support for commercial, cargo, military, and emergency flights
* Dynamic runway allocation (RWY-A, RWY-B, RWY-C)
* Realistic flight phases and speed monitoring
* AVN (Airspace Violation Notice) generation and processing
* Airline portal and simulated payment handling
* Real-time simulation timer and fault handling
* Thread-safe and synchronized aircraft operations
* Optional graphical simulation using SFML/OpenGL

## Project Structure

This project is divided into three modules:

### Module 1: System Rules & Restrictions

* Aircraft classification and scheduling
* Runway management and restrictions
* Speed monitoring and AVN triggering
* Basic simulation loop and timer

### Module 2: Core Functionalities

* Flight entry and scheduling queues
* Runway allocation based on direction and priority
* Priority and delay handling
* Real-time aircraft status updates

### Module 3: Subsystems & Integration

* ATCS Controller, AVN Generator, and Airline Portal as separate processes
* StripePay simulation for payment processing
* Detailed AVN records and analytics dashboard
* Graphical simulation of aircraft movement and status

## Build Instructions

1. **Dependencies**: Ensure you have a C++ compiler (e.g., `g++`) and optionally install SFML/OpenGL for graphics.
2. **Compile**:

   * With SFML(Graphics)
   ```bash
   g++ -std=c++17 source_SFML.cpp -lsfml-graphics -lsfml-window -lsfml-system -lpthread -o aircontrolx
   ```
   * Without SFML(Graphics)
    ```bash
   g++ -std=c++17 source.cpp -lpthread -o aircontrolx
   ```
   
3. **Run**:

   ```bash
   ./aircontrolx
   ```

## Notes

* This is a modular project each module builds upon the previous one.
* Flight violations, payments, and simulation time are key aspects in evaluation.

## Authors

* <a href=https://github.com/HanzlahCh>Hanzlah Mehmood Ch</a>
* <a href=https://github.com/AbdullahDurrani0007>Abdullah Durrani</a>
---
