#ifndef SIMULATION_CONTROLLER_H
#define SIMULATION_CONTROLLER_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <random>

#include "Constants.h"
#include "Aircraft.h"
#include "Airline.h"
#include "Runway.h"
#include "Flight.h"
#include "SpeedMonitor.h"

namespace AirControlX {

/**
 * @class SimulationController
 * @brief Central controller for the air traffic control simulation.
 *
 * The SimulationController manages the entire simulation, coordinating between
 * airlines, aircraft, runways, and flights. It handles timing, thread management,
 * statistics gathering, and overall system state.
 */
class SimulationController {
public:
    /**
     * SimulationController constructor
     * @param simulationDuration Duration of the simulation in seconds (defaults to 300 = 5 minutes)
     */
    explicit SimulationController(int simulationDuration = SIMULATION_DURATION);
    
    /**
     * Destructor - ensures all threads are properly terminated
     */
    ~SimulationController();
    
    /**
     * Initializes the simulation components
     * @return True if initialization was successful
     */
    bool initialize();
    
    /**
     * Starts the simulation
     * @return True if simulation started successfully
     */
    bool start();
    
    /**
     * Pauses the simulation
     * @return True if simulation was paused
     */
    bool pause();
    
    /**
     * Resumes a paused simulation
     * @return True if simulation was resumed
     */
    bool resume();
    
    /**
     * Stops the simulation gracefully
     * @return True if simulation was stopped
     */
    bool stop();
    
    /**
     * Resets the simulation to initial state
     * @return True if simulation was reset
     */
    bool reset();
    
    /**
     * Checks if the simulation is currently running
     * @return True if running
     */
    bool isRunning() const;
    
    /**
     * Checks if the simulation has completed
     * @return True if completed
     */
    bool isCompleted() const;
    
    /**
     * Gets the current simulation time
     * @return Current time in seconds since start
     */
    double getCurrentTime() const;
    
    /**
     * Gets the total simulation duration
     * @return Duration in seconds
     */
    int getSimulationDuration() const;
    
    /**
     * Gets the remaining simulation time
     * @return Remaining time in seconds
     */
    double getRemainingTime() const;
    
    /**
     * Gets all airlines in the simulation
     * @return Vector of airline pointers
     */
    std::vector<std::shared_ptr<Airline>> getAirlines() const;
    
    /**
     * Gets all runways in the simulation
     * @return Vector of runway pointers
     */
    std::vector<std::shared_ptr<Runway>> getRunways() const;
    
    /**
     * Gets all active flights in the simulation
     * @return Vector of flight pointers
     */
    std::vector<std::shared_ptr<Flight>> getActiveFlights() const;
    
    /**
     * Gets all flights with a specific status
     * @param status Flight status to filter by
     * @return Vector of flight pointers
     */
    std::vector<std::shared_ptr<Flight>> getFlightsByStatus(Flight::Status status) const;
    
    /**
     * Gets all aircraft with a specific type
     * @param type Aircraft type to filter by
     * @return Vector of aircraft pointers
     */
    std::vector<std::shared_ptr<Aircraft>> getAircraftByType(AircraftType type) const;
    
    /**
     * Gets all violation records from the speed monitor
     * @return Vector of violation records
     */
    std::vector<ViolationRecord> getAllViolations() const;
    
    /**
     * Gets statistics about the simulation
     * @return Map of statistic name to value
     */
    std::unordered_map<std::string, double> getStatistics() const;
    
    /**
     * Gets a specific runway by ID
     * @param id Runway ID
     * @return Pointer to the runway, or nullptr if not found
     */
    std::shared_ptr<Runway> getRunway(RunwayId id) const;
    
    /**
     * Gets a specific airline by name
     * @param name Airline name
     * @return Pointer to the airline, or nullptr if not found
     */
    std::shared_ptr<Airline> getAirline(const std::string& name) const;
    
    /**
     * Assigns a runway to a flight
     * @param flight Pointer to the flight
     * @return True if assignment was successful
     */
    bool assignRunwayToFlight(std::shared_ptr<Flight> flight);
    
    /**
     * Processes emergency flights with priority
     * @return Number of emergency flights processed
     */
    int processEmergencyFlights();
    
    /**
     * Handles ground faults across all aircraft
     * @return Number of faults handled
     */
    int handleGroundFaults();
    
    /**
     * Schedules new flights based on timing and probabilities
     * @return Number of new flights scheduled
     */
    int scheduleNewFlights();
    
    /**
     * Gets a textual status report of the current simulation state
     * @return Status report string
     */
    std::string getStatusReport() const;
    
    /**
     * Gets a string representation of the controller state
     * @return String description
     */
    std::string toString() const;

private:
    int m_simulationDuration;                  // Duration in seconds
    std::atomic<double> m_currentTime;         // Current simulation time
    std::atomic<bool> m_isRunning;             // Flag indicating if simulation is running
    std::atomic<bool> m_isPaused;              // Flag indicating if simulation is paused
    std::atomic<bool> m_isCompleted;           // Flag indicating if simulation has completed
    
    std::vector<std::shared_ptr<Airline>> m_airlines;  // All airlines in the simulation
    std::vector<std::shared_ptr<Runway>> m_runways;    // All runways in the simulation
    std::vector<std::shared_ptr<Flight>> m_flights;    // All flights (active and completed)
    
    std::shared_ptr<SpeedMonitor> m_speedMonitor;      // Speed monitoring system
    
    // Thread management
    std::thread m_simulationThread;            // Main simulation thread
    std::thread m_flightGenerationThread;      // Thread for generating new flights
    std::thread m_monitoringThread;            // Thread for monitoring and statistics
    
    std::mutex m_mutex;                        // Mutex for thread-safe operations
    std::condition_variable m_pauseCondition;  // Condition variable for pause/resume
    
    // Random number generation
    std::mt19937 m_randomEngine;
    
    // Statistics tracking
    struct Statistics {
        int totalFlights;                      // Total flights processed
        int completedFlights;                  // Successfully completed flights
        int emergencyFlights;                  // Emergency flights processed
        int groundFaults;                      // Ground faults handled
        int runwayAssignments;                 // Total runway assignments
        int speedViolations;                   // Total speed violations
        std::unordered_map<RunwayId, int> runwayUsage;  // Usage count by runway
        std::unordered_map<AircraftType, int> aircraftTypeCount;  // Count by aircraft type
    } m_statistics;
    
    /**
     * Main simulation loop
     */
    void simulationLoop();
    
    /**
     * Flight generation loop
     */
    void flightGenerationLoop();
    
    /**
     * Monitoring and statistics loop
     */
    void monitoringLoop();
    
    /**
     * Updates all simulation components for one step
     * @param deltaTime Time step in seconds
     */
    void updateSimulation(double deltaTime);
    
    /**
     * Creates the airlines according to the project specifications
     */
    void createAirlines();
    
    /**
     * Creates the runways according to the project specifications
     */
    void createRunways();
    
    /**
     * Determines if a new flight should be an emergency based on probabilities
     * @param direction Flight direction
     * @return True if should be emergency
     */
    bool shouldBeEmergency(FlightDirection direction);
    
    /**
     * Ensures at least one cargo flight is present in the simulation
     * @return True if a cargo flight was scheduled
     */
    bool ensureCargoFlightPresent();
    
    /**
     * Updates simulation statistics
     */
    void updateStatistics();
    
    /**
     * Validates the simulation state for consistency
     * @return True if state is valid
     */
    bool validateSimulationState() const;
    
    /**
     * Logs a simulation event
     * @param message Event message
     * @param level Log level (0=info, 1=warning, 2=error)
     */
    void logEvent(const std::string& message, int level = 0) const;
};

} // namespace AirControlX

#endif // SIMULATION_CONTROLLER_H

