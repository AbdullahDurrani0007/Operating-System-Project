#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include <string>
#include <vector>
#include <random>
#include <memory>
#include <mutex>
#include "Constants.h"

namespace AirControlX {

// Forward declarations
class Airline;

/**
 * @class Aircraft
 * @brief Represents an aircraft in the air traffic control system.
 *
 * The Aircraft class handles aircraft properties, status, speed monitoring,
 * flight phase transitions, and airspace violation notices (AVNs).
 */
class Aircraft {
public:
    /**
     * Aircraft constructor
     * @param id Unique identifier for the aircraft
     * @param type Type of aircraft (COMMERCIAL, CARGO, EMERGENCY)
     * @param direction Flight direction (NORTH, SOUTH, EAST, WEST)
     * @param airline Pointer to the associated airline
     */
    Aircraft(std::string id, AircraftType type, FlightDirection direction, std::shared_ptr<Airline> airline);
    
    /**
     * Destructor
     */
    ~Aircraft() = default;

    /**
     * Gets the aircraft's unique identifier
     * @return Aircraft identifier
     */
    std::string getId() const;
    
    /**
     * Gets the aircraft type
     * @return Type of aircraft
     */
    AircraftType getType() const;
    
    /**
     * Gets the current flight phase
     * @return Current flight phase
     */
    FlightPhase getCurrentPhase() const;
    
    /**
     * Gets the current speed in km/h
     * @return Current speed
     */
    double getCurrentSpeed() const;
    
    /**
     * Gets the flight direction
     * @return Direction of flight
     */
    FlightDirection getDirection() const;
    
    /**
     * Checks if this is an arrival flight
     * @return True if arrival, false if departure
     */
    bool isArrival() const;
    
    /**
     * Gets the airline associated with this aircraft
     * @return Pointer to the airline
     */
    std::shared_ptr<Airline> getAirline() const;
    
    /**
     * Assigns a runway to this aircraft
     * @param runwayId ID of the assigned runway
     */
    void assignRunway(RunwayId runwayId);
    
    /**
     * Gets the assigned runway ID
     * @return ID of the assigned runway, or nullopt if not assigned
     */
    std::optional<RunwayId> getAssignedRunway() const;
    
    /**
     * Transitions to the next flight phase
     * @return True if transition was successful
     */
    bool transitionToNextPhase();
    
    /**
     * Updates the aircraft's speed within the limits of the current phase
     * @param speedDelta Change in speed to apply (km/h)
     * @return True if speed was updated successfully
     */
    bool updateSpeed(double speedDelta);
    
    /**
     * Sets the aircraft's speed to a specific value
     * @param newSpeed New speed value (km/h)
     * @return True if speed was set successfully
     */
    bool setSpeed(double newSpeed);
    
    /**
     * Checks if the current speed is within the limits for the current phase
     * @return True if speed is valid, false if a violation exists
     */
    bool isSpeedValid() const;
    
    /**
     * Issues an Airspace Violation Notice (AVN) for this aircraft
     * @param reason Description of the violation
     */
    void issueAVN(const std::string& reason);
    
    /**
     * Checks if this aircraft has any active AVNs
     * @return True if there are active AVNs
     */
    bool hasActiveAVN() const;
    
    /**
     * Gets the list of active AVNs
     * @return Vector of violation descriptions
     */
    std::vector<std::string> getActiveAVNs() const;
    
    /**
     * Simulates a ground fault (like brake failure, hydraulic leak)
     * @return True if a fault was simulated
     */
    bool simulateGroundFault();
    
    /**
     * Checks if this aircraft has a ground fault
     * @return True if a ground fault exists
     */
    bool hasGroundFault() const;
    
    /**
     * Updates the aircraft state for one simulation step
     * @param deltaTime Time step in seconds
     */
    void update(double deltaTime);
    
    /**
     * Generates a string representation of the aircraft state
     * @return String description of aircraft state
     */
    std::string toString() const;

private:
    std::string m_id;                           // Unique identifier
    AircraftType m_type;                        // Aircraft type
    FlightDirection m_direction;                // Flight direction
    FlightPhase m_currentPhase;                 // Current flight phase
    double m_currentSpeed;                      // Current speed in km/h
    std::shared_ptr<Airline> m_airline;         // Associated airline
    std::optional<RunwayId> m_assignedRunway;   // Assigned runway (if any)
    bool m_hasGroundFault;                      // Flag for ground fault
    std::vector<std::string> m_activeAVNs;      // List of active violations
    mutable std::mutex m_mutex;                 // Mutex for thread safety
    
    /**
     * Sets the initial phase based on direction
     */
    void setInitialPhase();
    
    /**
     * Sets the initial speed based on the current phase
     */
    void setInitialSpeed();
    
    /**
     * Gets the next phase in the sequence
     * @return The next flight phase
     */
    FlightPhase getNextPhase() const;
    
    /**
     * Validates if a phase transition is allowed
     * @param nextPhase The phase to transition to
     * @return True if transition is valid
     */
    bool isValidTransition(FlightPhase nextPhase) const;
    
    /**
     * Generates a random speed within the allowed range for a phase
     * @param phase The flight phase
     * @return Random speed within limits
     */
    double generateRandomSpeedForPhase(FlightPhase phase) const;
};

} // namespace AirControlX

#endif // AIRCRAFT_H

