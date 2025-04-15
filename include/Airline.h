#ifndef AIRLINE_H
#define AIRLINE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "Constants.h"
#include "Aircraft.h"

namespace AirControlX {

/**
 * @class Airline
 * @brief Represents an airline company in the air traffic control system.
 *
 * The Airline class manages a fleet of aircraft and handles airline-specific operations
 * including fleet management, flight scheduling, and AVN tracking.
 */
class Airline {
public:
    /**
     * Airline constructor
     * @param name Name of the airline
     * @param type Primary aircraft type operated by the airline
     * @param totalAircrafts Total number of aircrafts in the fleet
     * @param activeFlights Number of aircraft in operation
     */
    Airline(const std::string& name, AircraftType type, int totalAircrafts, int activeFlights);
    
    /**
     * Destructor
     */
    ~Airline() = default;
    
    /**
     * Gets the airline name
     * @return Name of the airline
     */
    std::string getName() const;
    
    /**
     * Gets the primary aircraft type for this airline
     * @return Primary aircraft type
     */
    AircraftType getPrimaryType() const;
    
    /**
     * Gets the total number of aircraft in the fleet
     * @return Total fleet size
     */
    int getTotalAircrafts() const;
    
    /**
     * Gets the number of active flights
     * @return Number of active flights
     */
    int getActiveFlights() const;
    
    /**
     * Creates a new aircraft and adds it to the fleet
     * @param direction Flight direction (determines if it's arrival or departure)
     * @param forceEmergency Force this to be an emergency flight (if applicable)
     * @return Shared pointer to the created aircraft, or nullptr if creation failed
     */
    std::shared_ptr<Aircraft> createAircraft(FlightDirection direction, bool forceEmergency = false);
    
    /**
     * Gets all aircraft currently managed by this airline
     * @return Vector of aircraft pointers
     */
    std::vector<std::shared_ptr<Aircraft>> getAllAircraft() const;
    
    /**
     * Gets all aircraft with active AVNs
     * @return Vector of aircraft with violations
     */
    std::vector<std::shared_ptr<Aircraft>> getAircraftWithViolations() const;
    
    /**
     * Gets the total number of AVNs issued to this airline
     * @return Count of total violations
     */
    int getTotalViolationCount() const;
    
    /**
     * Process AVN payments (billing the airline for violations)
     * @return Total amount processed
     */
    double processAVNPayments();
    
    /**
     * Schedules a flight based on the simulation time
     * @param currentTime Current simulation time in seconds
     * @param direction Direction for the new flight
     * @return True if a flight was scheduled
     */
    bool scheduleFlightIfNeeded(double currentTime, FlightDirection direction);
    
    /**
     * Updates the state of all aircraft in the fleet
     * @param deltaTime Time step in seconds
     */
    void updateAllAircraft(double deltaTime);
    
    /**
     * Handles ground faults for aircraft in the fleet
     * @return Number of faults handled
     */
    int handleGroundFaults();
    
    /**
     * Gets a string representation of the airline state
     * @return String description
     */
    std::string toString() const;

private:
    std::string m_name;                       // Airline name
    AircraftType m_primaryType;               // Primary aircraft type
    int m_totalAircrafts;                     // Total fleet size
    int m_activeFlights;                      // Number of active flights
    int m_violationCount;                     // Total violation count
    double m_lastFlightScheduleTime;          // Time of last flight schedule
    
    // Map of flight ID to aircraft objects
    std::unordered_map<std::string, std::shared_ptr<Aircraft>> m_fleet;
    
    mutable std::mutex m_mutex;               // Mutex for thread safety
    
    /**
     * Generates a unique flight ID for a new aircraft
     * @return Unique flight ID string
     */
    std::string generateFlightId() const;
    
    /**
     * Determines if the aircraft type should be overridden for this flight
     * @param direction Flight direction
     * @param forceEmergency Whether to force emergency status
     * @return Appropriate aircraft type
     */
    AircraftType determineAircraftType(FlightDirection direction, bool forceEmergency) const;
    
    /**
     * Checks if a new flight can be scheduled based on fleet capacity
     * @return True if a new flight can be scheduled
     */
    bool canScheduleFlight() const;
};

} // namespace AirControlX

#endif // AIRLINE_H

