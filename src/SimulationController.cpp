#include "../include/SimulationController.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <functional>
#include <random>

namespace AirControlX {

// Constructor
SimulationController::SimulationController(int simulationDuration)
    : m_simulationDuration(simulationDuration)
    , m_currentTime(0.0)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_isCompleted(false)
{
    // Initialize random number generator with a seed
    std::random_device rd;
    m_randomEngine = std::mt19937(rd());
}

void SimulationController::createRunways() {
    // Create the 3 runways specified in the requirements
    auto runwayA = std::make_shared<Runway>(RunwayId::RWY_A);
    auto runwayB = std::make_shared<Runway>(RunwayId::RWY_B);
    auto runwayC = std::make_shared<Runway>(RunwayId::RWY_C);
    
    m_runways.push_back(runwayA);
    m_runways.push_back(runwayB);
    m_runways.push_back(runwayC);
    
    logEvent("Created runways: RWY_A, RWY_B, RWY_C", 0);
}

bool SimulationController::ensureCargoFlightPresent() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // First check if there's already an active cargo flight
    for (const auto& flight : m_flights) {
        auto aircraft = flight->getAircraft();
        if (aircraft && aircraft->getType() == AircraftType::CARGO) {
            // Only count flights that aren't completed or canceled
            auto status = flight->getStatus();
            if (status != Flight::Status::COMPLETED && 
                status != Flight::Status::CANCELED && 
                status != Flight::Status::DIVERTED) {
                return true; // Active cargo flight exists
            }
        }
    }
    
    // No active cargo flight found, try to create one
    for (const auto& airline : m_airlines) {
        // Look for cargo airlines first, then try airlines that can handle cargo
        if (airline->getPrimaryType() == AircraftType::CARGO || 
            airline->getPrimaryType() == AircraftType::COMMERCIAL) {
            
            // Generate a random direction that's more likely to use RWY_C
            std::uniform_int_distribution<int> dist(0, 3);
            FlightDirection direction;
            
            // Try to pick a direction that works well for RWY_C
            int attempts = 0;
            do {
                switch (dist(m_randomEngine)) {
                    case 0: direction = FlightDirection::NORTH; break;
                    case 1: direction = FlightDirection::SOUTH; break;
                    case 2: direction = FlightDirection::EAST; break;
                    case 3: direction = FlightDirection::WEST; break;
                    default: direction = FlightDirection::NORTH; break;
                }
                attempts++;
            } while (!Runway::isValidRunwayForDirection(RunwayId::RWY_C, direction) && attempts < 4);
            
            // Create a cargo aircraft
            auto aircraft = airline->getPrimaryType() == AircraftType::CARGO ?
                airline->createAircraft(direction, false) :  // Cargo airline will make cargo aircraft
                airline->createAircraft(direction, AircraftType::CARGO);  // Explicitly request cargo type
            
            if (aircraft && aircraft->getType() == AircraftType::CARGO) {
                // Create a cargo flight scheduled for immediate execution
                auto flight = std::make_shared<Flight>(aircraft, m_currentTime, false);
                m_flights.push_back(flight);
                
                // Try to assign RWY_C first
                auto runwayC = getRunway(RunwayId::RWY_C);
                if (runwayC && runwayC->isAvailable() && 
                    flight->assignRunway(runwayC)) {
                    m_statistics.runwayAssignments++;
                    m_statistics.runwayUsage[RunwayId::RWY_C]++;
                    logEvent("Created and assigned cargo flight " + flight->getId() + 
                           " to RWY-C", 0);
                } else {
                    // If RWY_C is not available, try other valid runways
                    for (const auto& runway : m_runways) {
                        if (runway->getId() != RunwayId::RWY_C && 
                            runway->isAvailable() && 
                            runway->canUseForDirection(direction) && 
                            runway->canUseForAircraftType(AircraftType::CARGO)) {
                            
                            if (flight->assignRunway(runway)) {
                                m_statistics.runwayAssignments++;
                                m_statistics.runwayUsage[runway->getId()]++;
                                logEvent("Created and assigned cargo flight " + 
                                       flight->getId() + " to " + 
                                       RunwayIdToString(runway->getId()) + 
                                       " (RWY-C unavailable)", 1);
                                break;
                            }
                        }
                    }
                }
                
                // Update statistics
                m_statistics.totalFlights++;
                m_statistics.aircraftTypeCount[AircraftType::CARGO]++;
                
                // Activate the flight immediately
                flight->activate(m_currentTime);
                
                logEvent("Created new cargo flight " + flight->getId() + 
                        " to ensure cargo presence requirement", 0);
                return true;
            }
        }
    }
    
    logEvent("Failed to create cargo flight - no suitable airline found", 2);
    return false;
            m_statistics.aircraftTypeCount[aircraft->getType()]++;
            
            if (isEmergency) {
                m_statistics.emergencyFlights++;
                logEvent("Emergency flight " + flight->getId() + " scheduled from NORTH", 0);
            } else {
                logEvent("Flight " + flight->getId() + " scheduled from NORTH", 0);
            }
            
            scheduledCount++;
        }
        
        // Try scheduling for South (Domestic Arrivals)
        if (airline->scheduleFlightIfNeeded(m_currentTime, FlightDirection::SOUTH)) {
            auto aircraft = airline->getAllAircraft().back();
            bool isEmergency = shouldBeEmergency(FlightDirection::SOUTH);
            
            // Create the flight
            auto flight = std::make_shared<Flight>(aircraft, m_currentTime, isEmergency);
            m_flights.push_back(flight);
            
            // Update statistics
            m_statistics.totalFlights++;
            m_statistics.aircraftTypeCount[aircraft->getType()]++;
            
            if (isEmergency) {
                m_statistics.emergencyFlights++;
                logEvent("Emergency flight " + flight->getId() + " scheduled from SOUTH", 0);
            } else {
                logEvent("Flight " + flight->getId() + " scheduled from SOUTH", 0);
            }
            
            scheduledCount++;
        }
        
        // Try scheduling for East (International Departures)
        if (airline->scheduleFlightIfNeeded(m_currentTime, FlightDirection::EAST)) {
            auto aircraft = airline->getAllAircraft().back();
            bool isEmergency = shouldBeEmergency(FlightDirection::EAST);
            
            // Create the flight
            auto flight = std::make_shared<Flight>(aircraft, m_currentTime, isEmergency);
            m_flights.push_back(flight);
            
            // Update statistics
            m_statistics.totalFlights++;
            m_statistics.aircraftTypeCount[aircraft->getType()]++;
            
            if (isEmergency) {
                m_statistics.emergencyFlights++;
                logEvent("Emergency flight " + flight->getId() + " scheduled from EAST", 0);
            } else {
                logEvent("Flight " + flight->getId() + " scheduled from EAST", 0);
            }
            
            scheduledCount++;
        }
        
        // Try scheduling for West (Domestic Departures)
        if (airline->scheduleFlightIfNeeded(m_currentTime, FlightDirection::WEST)) {
            auto aircraft = airline->getAllAircraft().back();
            bool isEmergency = shouldBeEmergency(FlightDirection::WEST);
            
            // Create the flight
            auto flight = std::make_shared<Flight>(aircraft, m_currentTime, isEmergency);
            m_flights.push_back(flight);
            
            // Update statistics
            m_statistics.totalFlights++;
            m_statistics.aircraftTypeCount[aircraft->getType()]++;
            
            if (isEmergency) {
                m_statistics.emergencyFlights++;
                logEvent("Emergency flight " + flight->getId() + " scheduled from WEST", 0);
            } else {
                logEvent("Flight " + flight->getId() + " scheduled from WEST", 0);
            }
            
            scheduledCount++;
        }
    }
    
    return scheduledCount;
}

void SimulationController::simulationLoop() {
    // Set up timing variables
    auto lastFrameTime = std::chrono::steady_clock::now();
    
    // Continue until simulation is complete or stopped
    while (m_isRunning && !m_isCompleted) {
        // Check if paused
        if (m_isPaused) {
            // Wait until resumed
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pauseCondition.wait(lock, [this] { return !m_isPaused || !m_isRunning; });
            
            // If we were stopped while paused, exit
            if (!m_isRunning) {
                break;
            }
            
            // Reset timing for smooth continuation
            lastFrameTime = std::chrono::steady_clock::now();
            continue;
        }
        
        // Calculate delta time (time since last frame)
        auto currentFrameTime = std::chrono::steady_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentFrameTime - lastFrameTime).count() / 1000.0;
        lastFrameTime = currentFrameTime;
        
        // Cap delta time to prevent large jumps
        if (deltaTime > 0.1) {
            deltaTime = 0.1;
        }
        
        // Update the simulation
        updateSimulation(deltaTime);
        
        // Increment current time
        m_currentTime += deltaTime;
        
        // Check if simulation is complete
        if (m_currentTime >= m_simulationDuration) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_isCompleted = true;
            logEvent("Simulation completed after " + std::to_string(m_simulationDuration) + " seconds", 0);
        }
        
        // Throttle simulation speed (optional)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SimulationController::flightGenerationLoop() {
    // Continue until simulation is complete or stopped
    while (m_isRunning && !m_isCompleted) {
        // Check if paused
        if (m_isPaused) {
            // Wait until resumed
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pauseCondition.wait(lock, [this] { return !m_isPaused || !m_isRunning; });
            
            // If we were stopped while paused, exit
            if (!m_isRunning) {
                break;
            }
            
            continue;
        }
        
        // Schedule new flights
        scheduleNewFlights();
        
        // Ensure at least one cargo flight
        ensureCargoFlightPresent();
        
        // Sleep to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SimulationController::monitoringLoop() {
    // Set up timing variables
    auto lastUpdateTime = std::chrono::steady_clock::now();
    
    // Continue until simulation is complete or stopped
    while (m_isRunning && !m_isCompleted) {
        // Check if paused
        if (m_isPaused) {
            // Wait until resumed
            std::unique_lock<std::mutex> lock(m_mutex);
            m_pauseCondition.wait(lock, [this] { return !m_isPaused || !m_isRunning; });
            
            // If we were stopped while paused, exit
            if (!m_isRunning) {
                break;
            }
            
            // Reset timing for smooth continuation
            lastUpdateTime = std::chrono::steady_clock::now();
            continue;
        }
        
        // Process emergency flights with priority
        processEmergencyFlights();
        
        // Handle ground faults
        handleGroundFaults();
        
        // Update statistics
        updateStatistics();
        
        // Validate simulation state
        validateSimulationState();
        
        // Throttle monitoring rate
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void SimulationController::updateSimulation(double deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Update all airlines
    for (const auto& airline : m_airlines) {
        airline->updateAllAircraft(deltaTime);
    }
    
    // Update all flights
    for (const auto& flight : m_flights) {
        flight->update(deltaTime, m_currentTime);
    }
    
    // Update all runways
    for (const auto& runway : m_runways) {
        runway->update(deltaTime);
    }
    
    // Get all active aircraft
    std::vector<std::shared_ptr<Aircraft>> activeAircraft;
    for (const auto& flight : getActiveFlights()) {
        if (auto aircraft = flight->getAircraft()) {
            activeAircraft.push_back(aircraft);
        }
    }
    
    // Update speed monitor with all active aircraft
    m_speedMonitor->update(activeAircraft, m_currentTime);
    
    // Assign runways to scheduled flights
    for (const auto& flight : getFlightsByStatus(Flight::Status::SCHEDULED)) {
        // If it's time to activate
        if (m_currentTime >= flight->getScheduledTime()) {
            // First try to assign a runway
            if (!flight->getAssignedRunway()) {
                assignRunwayToFlight(flight);
            }
            
            // If runway is assigned, activate the flight
            if (flight->getAssignedRunway()) {
                flight->activate(m_currentTime);
            }
        }
    }
}

void SimulationController::createAirlines() {
    // Create airlines based on the AIRLINES constant from Constants.h
    for (const auto& info : AIRLINES) {
        auto airline = std::make_shared<Airline>(
            info.name,
            info.type,
            info.aircrafts,
            info.flights
        );
        
        m_airlines.push_back(airline);
        logEvent("Created airline: " + info.name, 0);
    }
}

void SimulationController::createRunways() {
    // Create the 3 runways specified in the requirements
    auto runwayA = std::make_shared<Runway>(RunwayId::RWY_A);
    auto runwayB = std::make_shared<Runway>(RunwayId::RWY_B);
    auto runwayC = std::make_shared<Runway>(RunwayId::RWY_C);
    
    m_runways.push_back(runwayA);
    m_runways.push_back(runwayB);
    m_runways.push_back(runwayC);
    
    logEvent("Failed to create cargo flight - no suitable airline found", 2);
    return false;
    }
    
    // No active cargo flight found, try to create one
    for (const auto& airline : m_airlines) {
        // Look for cargo airlines first, then try airlines that can handle cargo
        if (airline->getPrimaryType() == AircraftType::CARGO || 
            airline->getPrimaryType() == AircraftType::COMMERCIAL) {
            
            // Generate a random direction that's more likely to use RWY_C
            std::uniform_int_distribution<int> dist(0, 3);
            FlightDirection direction;
            
            // Try to pick a direction that works well for RWY_C
            int attempts = 0;
            do {
                switch (dist(m_randomEngine)) {
                    case 0: direction = FlightDirection::NORTH; break;
                    case 1: direction = FlightDirection::SOUTH; break;
                    case 2: direction = FlightDirection::EAST; break;
                    case 3: direction = FlightDirection::WEST; break;
                    default: direction = FlightDirection::NORTH; break;
                }
                attempts++;
            } while (!Runway::isValidRunwayForDirection(RunwayId::RWY_C, direction) && attempts < 4);
            
            // Create a cargo aircraft
            auto aircraft = airline->getPrimaryType() == AircraftType::CARGO ?
                airline->createAircraft(direction, false) :  // Cargo airline will make cargo aircraft
                airline->createAircraft(direction, AircraftType::CARGO);  // Explicitly request cargo type
            
            if (aircraft && aircraft->getType() == AircraftType::CARGO) {
                // Create a cargo flight scheduled for immediate execution
                auto flight = std::make_shared<Flight>(aircraft, m_currentTime, false);
                m_flights.push_back(flight);
                
                // Try to assign RWY_C first
                auto runwayC = getRunway(RunwayId::RWY_C);
                if (runwayC && runwayC->isAvailable() && 
                    flight->assignRunway(runwayC)) {
                    m_statistics.runwayAssignments++;
                    m_statistics.runwayUsage[RunwayId::RWY_C]++;
                    logEvent("Created and assigned cargo flight " + flight->getId() + 
                           " to RWY-C", 0);
                } else {
                    // If RWY_C is not available, try other valid runways
                    for (const auto& runway : m_runways) {
                        if (runway->getId() != RunwayId::RWY_C && 
                            runway->isAvailable() && 
                            runway->canUseForDirection(direction) && 
                            runway->canUseForAircraftType(AircraftType::CARGO)) {
                            
                            if (flight->assignRunway(runway)) {
                                m_statistics.runwayAssignments++;
                                m_statistics.runwayUsage[runway->getId()]++;
                                logEvent("Created and assigned cargo flight " + 
                                       flight->getId() + " to " + 
                                       RunwayIdToString(runway->getId()) + 
                                       " (RWY-C unavailable)", 1);
                                break;
                            }
                        }
                    }
                }
                
                // Update statistics
                m_statistics.totalFlights++;
                m_statistics.aircraftTypeCount[AircraftType::CARGO]++;
                
                // Activate the flight immediately
                flight->activate(m_currentTime);
                
                logEvent("Created new cargo flight " + flight->getId() + 
                        " to ensure cargo presence requirement", 0);
                return true;
            }
        }
    }
    
    logEvent("Failed to create cargo flight - no suitable airline found", 2);
    return false;
        }
    }
    
    // If no cargo flight exists, create one
    for (const auto& airline : m_airlines) {
        // Look for cargo airlines
        if (airline->getPrimaryType() == AircraftType::CARGO) {
            // Generate a random direction
            std::uniform_int_distribution<int> dist(0, 3);
            FlightDirection direction;
            
            switch (dist(m_randomEngine)) {
                case 0: direction = FlightDirection::NORTH; break;
                case 1: direction = FlightDirection::SOUTH; break;
                case 2: direction = FlightDirection::EAST; break;
                case 3: direction = FlightDirection

