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
    , m_activeCargoFlights(0)
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

int SimulationController::countActiveCargoFlights() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int count = 0;
    for (const auto& flight : m_flights) {
        auto aircraft = flight->getAircraft();
        if (aircraft && aircraft->getType() == AircraftType::CARGO) {
            // Only count flights that aren't completed or canceled
            auto status = flight->getStatus();
            if (status != Flight::Status::COMPLETED && 
                status != Flight::Status::CANCELED && 
                status != Flight::Status::DIVERTED) {
                count++;
            }
        }
    }
    
    return count;
}

std::shared_ptr<Flight> SimulationController::createCargoFlight() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
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
                
                // Update statistics
                m_statistics.totalFlights++;
                m_statistics.aircraftTypeCount[AircraftType::CARGO]++;
                
                logEvent("Created new cargo flight " + flight->getId(), 0);
                return flight;
            }
        }
    }
    
    logEvent("Failed to create cargo flight - no suitable airline found", 2);
    return nullptr;
}

bool SimulationController::assignRunwayToCargo(std::shared_ptr<Flight> flight) {
    if (!flight || !flight->getAircraft() || 
        flight->getAircraft()->getType() != AircraftType::CARGO) {
        return false;
    }
    
    auto direction = flight->getAircraft()->getDirection();
    
    // Try to assign RWY_C first (cargo priority)
    auto runwayC = getRunway(RunwayId::RWY_C);
    if (runwayC && runwayC->isAvailable() && 
        runwayC->canUseForDirection(direction) &&
        flight->assignRunway(runwayC)) {
        
        m_statistics.runwayAssignments++;
        m_statistics.runwayUsage[RunwayId::RWY_C]++;
        logEvent("Assigned cargo flight " + flight->getId() + " to RWY-C", 0);
        return true;
    }
    
    // If RWY_C is not available, try other valid runways
    for (const auto& runway : m_runways) {
        if (runway->getId() != RunwayId::RWY_C && 
            runway->isAvailable() && 
            runway->canUseForDirection(direction) && 
            runway->canUseForAircraftType(AircraftType::CARGO)) {
            
            if (flight->assignRunway(runway)) {
                m_statistics.runwayAssignments++;
                m_statistics.runwayUsage[runway->getId()]++;
                logEvent("Assigned cargo flight " + flight->getId() + " to " + 
                       RunwayIdToString(runway->getId()) + 
                       " (RWY-C unavailable)", 1);
                return true;
            }
        }
    }
    
    // No runway available, add to denied flights queue
    addDeniedFlightForRescheduling(flight);
    logEvent("Cargo flight " + flight->getId() + " denied runway assignment, added to queue", 1);
    return false;
}

void SimulationController::addDeniedFlightForRescheduling(std::shared_ptr<Flight> flight) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (flight) {
        m_deniedFlights.push(flight);
        m_statistics.deniedFlights++;
        logEvent("Flight " + flight->getId() + " added to denied flights queue for rescheduling", 1);
    }
}

int SimulationController::processDeniedFlights() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int rescheduledCount = 0;
    int attemptedCount = 0;
    int maxAttemptsPerCycle = 5; // Limit attempts per cycle to prevent blocking
    
    while (!m_deniedFlights.empty() && attemptedCount < maxAttemptsPerCycle) {
        auto flight = m_deniedFlights.front();
        m_deniedFlights.pop();
        attemptedCount++;
        
        // Skip flights that are already completed or canceled
        if (flight->getStatus() == Flight::Status::COMPLETED || 
            flight->getStatus() == Flight::Status::CANCELED || 
            flight->getStatus() == Flight::Status::DIVERTED) {
            continue;
        }
        
        // Check if flight already has a runway
        if (flight->getAssignedRunway()) {
            continue;
        }
        
        bool assigned = false;
        
        // Handle cargo flights with special priority
        if (flight->getAircraft() && flight->getAircraft()->getType() == AircraftType::CARGO) {
            assigned = assignRunwayToCargo(flight);
        } else {
            // For regular flights, use standard assignment
            // For regular flights, use standard assignment
            assigned = assignRunwayToFlight(flight);
        }
        
        if (assigned) {
            // For cargo flights, update the counter since this was a rescheduled assignment
            if (flight->getAircraft() && flight->getAircraft()->getType() == AircraftType::CARGO) {
                // Only increment if this is the first time the flight gets a runway
                // (we already counted it when created, but it was denied assignment)
                if (flight->getStatus() == Flight::Status::SCHEDULED) {
                    m_activeCargoFlights++;
                }
            }
            if (flight->getStatus() == Flight::Status::SCHEDULED && 
                m_currentTime >= flight->getScheduledTime()) {
                flight->activate(m_currentTime);
            }
            
            rescheduledCount++;
            m_statistics.rescheduledFlights++;
            logEvent("Successfully rescheduled flight " + flight->getId(), 0);
        } else {
            // Re-queue the flight for later attempts if still valid
            m_deniedFlights.push(flight);
            logEvent("Failed to reschedule flight " + flight->getId() + ", re-queued", 1);
        }
    }
    
    return rescheduledCount;
}

void SimulationController::deniedFlightLoop() {
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
        
        // Process denied flights
        processDeniedFlights();
        
        // Sleep to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

bool SimulationController::ensureCargoFlightPresent() {
    // Count active cargo flights
    int cargoCount = countActiveCargoFlights();
    
    // If we already have exactly one cargo flight, we're good
    if (cargoCount == 1) {
        return true;
    }
    
    // If we have more than one cargo flight, log a warning but don't do anything
    // (This shouldn't happen with proper implementation)
    if (cargoCount > 1) {
        logEvent("Warning: Multiple active cargo flights detected (" + 
                std::to_string(cargoCount) + "), expected exactly one", 1);
        return true;
    }
    
    // If we have no cargo flights, create one
    if (cargoCount == 0) {
        // Create a new cargo flight
        auto flight = createCargoFlight();
        if (!flight) {
            return false;
        }
        
        // Try to assign a runway with cargo prioritization
        // Try to assign a runway with cargo prioritization
        if (assignRunwayToCargo(flight)) {
            // Activate the flight immediately
            flight->activate(m_currentTime);
            m_activeCargoFlights++;
            logEvent("Created and activated new cargo flight " + flight->getId() + 
                    " to ensure cargo presence requirement", 0);
            return true;
        } else {
            // Failed to assign runway, but keep track of the flight anyway
            // It will be picked up by the denied flight handler later
            logEvent("Created cargo flight " + flight->getId() + 
                    " but could not assign runway immediately", 1);
            return true;
        }
    }
    
    return false;

int SimulationController::scheduleNewFlights() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int scheduledCount = 0;
    
    // Try to schedule flights for each airline
    for (const auto& airline : m_airlines) {
        // Try scheduling for North (International Arrivals)
        if (airline->scheduleFlightIfNeeded(m_currentTime, FlightDirection::NORTH)) {
            auto aircraft = airline->getAllAircraft().back();
            bool isEmergency = shouldBeEmergency(FlightDirection::NORTH);
            
            // Create the flight
            auto flight = std::make_shared<Flight>(aircraft, m_currentTime, isEmergency);
            m_flights.push_back(flight);
            
            // Update statistics
            m_statistics.totalFlights++;
            m_statistics.aircraftTypeCount[aircraft->getType()]++;
            
            // Track cargo flights specifically
            if (aircraft->getType() == AircraftType::CARGO) {
                m_activeCargoFlights++;
            }
            
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
            
            // Track cargo flights specifically
            if (aircraft->getType() == AircraftType::CARGO) {
                m_activeCargoFlights++;
            }
            
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
            
            // Track cargo flights specifically
            if (aircraft->getType() == AircraftType::CARGO) {
                m_activeCargoFlights++;
            }
            
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
            
            // Track cargo flights specifically
            if (aircraft->getType() == AircraftType::CARGO) {
                m_activeCargoFlights++;
            }
            
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
        updateStatistics();
        
        // Validate cargo flight count against actual state to ensure consistency
        int actualCargoCount = countActiveCargoFlights();
        if (actualCargoCount != m_activeCargoFlights) {
            logEvent("Warning: Cargo flight counter mismatch. Counter: " + 
                    std::to_string(m_activeCargoFlights) + ", Actual: " + 
                    std::to_string(actualCargoCount) + ". Correcting.", 1);
            m_activeCargoFlights = actualCargoCount;
        }
        
        // Make sure we have exactly one cargo flight
        if (actualCargoCount == 0) {
            logEvent("No active cargo flights detected in monitoring loop, creating one", 1);
            ensureCargoFlightPresent();
        } else if (actualCargoCount > 1) {
            logEvent("Multiple cargo flights detected: " + std::to_string(actualCargoCount), 1);
            // We don't cancel existing flights, just log the situation
        }
        
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
        auto prevStatus = flight->getStatus();
        flight->update(deltaTime, m_currentTime);
        
        // Check if a cargo flight just completed or was canceled
        if (flight->getAircraft() && flight->getAircraft()->getType() == AircraftType::CARGO) {
            auto currentStatus = flight->getStatus();
            if ((prevStatus != Flight::Status::COMPLETED && prevStatus != Flight::Status::CANCELED && 
                 prevStatus != Flight::Status::DIVERTED) &&
                (currentStatus == Flight::Status::COMPLETED || currentStatus == Flight::Status::CANCELED || 
                 currentStatus == Flight::Status::DIVERTED)) {
                // A cargo flight just completed or was canceled, decrement the counter
                m_activeCargoFlights--;
                logEvent("Cargo flight " + flight->getId() + " is no longer active, updating counter", 0);
            }
        }
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

bool SimulationController::shouldBeEmergency(FlightDirection direction) {
    // Check if this flight should be an emergency based on probability
    auto it = EMERGENCY_PROBABILITY.find(direction);
    if (it != EMERGENCY_PROBABILITY.end()) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(m_randomEngine) < it->second;
    }
    return false;
}

bool SimulationController::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Reset state
    m_currentTime = 0.0;
    m_isRunning = false;
    m_isPaused = false;
    m_isCompleted = false;
    m_activeCargoFlights = 0;
    
    m_airlines.clear();
    m_runways.clear();
    m_flights.clear();
    m_deniedFlights = std::queue<std::shared_ptr<Flight>>();
    
    // Reset statistics
    m_statistics = Statistics{};
    
    // Initialize components
    createAirlines();
    createRunways();
    
    // Create the speed monitor
    m_speedMonitor = std::make_shared<SpeedMonitor>();
    
    // Ensure at least one cargo flight is present
    ensureCargoFlightPresent();
    
    logEvent("Simulation initialized successfully", 0);
    return true;
}

bool SimulationController::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_isRunning) {
        logEvent("Simulation is already running", 1);
        return false;
    }
    
    if (m_isCompleted) {
        // Reset if completed
        if (!reset()) {
            logEvent("Failed to reset completed simulation", 2);
            return false;
        }
    }
    
    // Set running flag
    m_isRunning = true;
    m_isPaused = false;
    
    // Start simulation thread
    m_simulationThread = std::thread(&SimulationController::simulationLoop, this);
    
    // Start flight generation thread
    m_flightGenerationThread = std::thread(&SimulationController::flightGenerationLoop, this);
    
    // Start monitoring thread
    m_monitoringThread = std::thread(&SimulationController::monitoringLoop, this);
    
    // Start denied flight processing thread
    m_deniedFlightThread = std::thread(&SimulationController::deniedFlightLoop, this);
    
    logEvent("Simulation started", 0);
    return true;
}

bool SimulationController::assignRunwayToFlight(std::shared_ptr<Flight> flight) {
    if (!flight) {
        return false;
    }
    
    auto aircraft = flight->getAircraft();
    if (!aircraft) {
        return false;
    }
    
    // Get flight characteristics
    auto direction = aircraft->getDirection();
    auto type = aircraft->getType();
    bool isEmergency = flight->isEmergency();
    
    // For cargo flights, use the specialized cargo assignment
    if (type == AircraftType::CARGO) {
        return assignRunwayToCargo(flight);
    }
    
    // Track if assignment was successful
    bool assigned = false;
    
    // Handle emergency flights with priority
    if (isEmergency) {
        // For emergency, try all valid runways in sequence until we find an available one
        for (const auto& runway : m_runways) {
            // Ensure RWY-C exclusivity for cargo/emergency
            if (runway->getId() == RunwayId::RWY_C && type != AircraftType::CARGO && type != AircraftType::EMERGENCY) {
                continue; // Skip RWY-C for non-cargo, non-emergency flights
            }
            
            if (runway->isAvailable() && 
                runway->canUseForDirection(direction) && 
                runway->canUseForAircraftType(type)) {
                
                if (flight->assignRunway(runway)) {
                    m_statistics.runwayAssignments++;
                    m_statistics.runwayUsage[runway->getId()]++;
                    logEvent("Emergency flight " + flight->getId() + 
                           " assigned to " + RunwayIdToString(runway->getId()), 0);
                    assigned = true;
                    break;
                }
            }
        }
    } else {
        // For normal flights, try to find the most appropriate runway
        for (const auto& runway : m_runways) {
            // Ensure RWY-C exclusivity for cargo/emergency
            if (runway->getId() == RunwayId::RWY_C && type != AircraftType::CARGO && type != AircraftType::EMERGENCY) {
                continue; // Skip RWY-C for non-cargo, non-emergency flights
            }
            
            if (runway->isAvailable() && 
                runway->canUseForDirection(direction) && 
                runway->canUseForAircraftType(type)) {
                
                if (flight->assignRunway(runway)) {
                    m_statistics.runwayAssignments++;
                    m_statistics.runwayUsage[runway->getId()]++;
                    logEvent("Flight " + flight->getId() + 
                           " assigned to " + RunwayIdToString(runway->getId()), 0);
                    assigned = true;
                    break;
                }
            }
        }
    }
    
    // If assignment failed, add to denied flights queue
    if (!assigned) {
        addDeniedFlightForRescheduling(flight);
        logEvent("Failed to assign runway to flight " + flight->getId(), 1);
    }
    
    return assigned;
}
