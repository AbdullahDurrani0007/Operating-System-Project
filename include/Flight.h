#ifndef FLIGHT_H
#define FLIGHT_H

#include <string>
#include <memory>
#include <mutex>
#include <chrono>
#include <optional>
#include <functional>
#include "Constants.h"

namespace AirControlX {

// Forward declarations
class Aircraft;
class Runway;

/**
 * @class Flight
 * @brief Manages a flight operation in the air traffic control system.
 *
 * The Flight class coordinates operations between aircraft and runways,
 * manages flight scheduling, status tracking, phase transitions, emergency handling,
 * and flight plan execution.
 */
class Flight {
public:
    /**
     * Enum representing the status of a flight
     */
    enum class Status {
        SCHEDULED,      // Flight is scheduled but not yet active
        ACTIVE,         // Flight is currently active in the system
        COMPLETED,      // Flight has successfully completed its operation
        CANCELED,       // Flight has been canceled
        DIVERTED,       // Flight has been diverted to another airport
        EMERGENCY       // Flight is in emergency status
    };
    
    /**
     * Flight constructor
     * @param aircraft Pointer to the aircraft associated with this flight
     * @param scheduledTime Scheduled time for this flight (in simulation time)
     * @param isEmergency Flag indicating if this is an emergency flight
     */
    Flight(std::shared_ptr<Aircraft> aircraft, double scheduledTime, bool isEmergency = false);
    
    /**
     * Destructor
     */
    ~Flight() = default;
    
    /**
     * Gets the unique identifier for this flight
     * @return Flight ID
     */
    std::string getId() const;
    
    /**
     * Gets the aircraft associated with this flight
     * @return Pointer to the aircraft
     */
    std::shared_ptr<Aircraft> getAircraft() const;
    
    /**
     * Gets the current status of the flight
     * @return Flight status
     */
    Status getStatus() const;
    
    /**
     * Checks if this is an emergency flight
     * @return True if this is an emergency flight
     */
    bool isEmergency() const;
    
    /**
     * Sets the emergency status of the flight
     * @param isEmergency True to set as emergency, false otherwise
     */
    void setEmergency(bool isEmergency);
    
    /**
     * Attempts to assign a runway to this flight
     * @param runway Pointer to the runway to assign
     * @return True if assignment was successful
     */
    bool assignRunway(std::shared_ptr<Runway> runway);
    
    /**
     * Releases the assigned runway
     * @return True if release was successful
     */
    bool releaseRunway();
    
    /**
     * Gets the assigned runway for this flight
     * @return Pointer to the assigned runway, or nullptr if none
     */
    std::shared_ptr<Runway> getAssignedRunway() const;
    
    /**
     * Activates the flight (transitions from SCHEDULED to ACTIVE)
     * @param currentTime Current simulation time
     * @return True if activation was successful
     */
    bool activate(double currentTime);
    
    /**
     * Completes the flight (transitions to COMPLETED)
     * @return True if completion was successful
     */
    bool complete();
    
    /**
     * Cancels the flight (transitions to CANCELED)
     * @param reason Reason for cancellation
     * @return True if cancellation was successful
     */
    bool cancel(const std::string& reason);
    
    /**
     * Diverts the flight (transitions to DIVERTED)
     * @param reason Reason for diversion
     * @return True if diversion was successful
     */
    bool divert(const std::string& reason);
    
    /**
     * Checks if the flight is ready to proceed to the next phase
     * @param currentTime Current simulation time
     * @return True if ready for next phase
     */
    bool isReadyForNextPhase(double currentTime) const;
    
    /**
     * Transitions the flight's aircraft to the next phase
     * @return True if transition was successful
     */
    bool transitionToNextPhase();
    
    /**
     * Gets the scheduled time for this flight
     * @return Scheduled time (in simulation time)
     */
    double getScheduledTime() const;
    
    /**
     * Gets the activation time for this flight
     * @return Activation time, or 0 if not yet activated
     */
    double getActivationTime() const;
    
    /**
     * Gets the estimated completion time for this flight
     * @return Estimated completion time, or 0 if not applicable
     */
    double getEstimatedCompletionTime() const;
    
    /**
     * Gets the delay for this flight (difference between scheduled and actual times)
     * @param currentTime Current simulation time
     * @return Delay time in seconds, or 0 if not applicable
     */
    double getDelay(double currentTime) const;
    
    /**
     * Handles any ground faults for this flight
     * @return True if a fault was handled
     */
    bool handleGroundFault();
    
    /**
     * Updates the flight state for one simulation step
     * @param deltaTime Time step in seconds
     * @param currentTime Current simulation time
     */
    void update(double deltaTime, double currentTime);
    
    /**
     * Gets a string representation of the flight state
     * @return String description
     */
    std::string toString() const;
    
    /**
     * Creates a flight plan appropriate for the aircraft type and direction
     * @return True if flight plan creation was successful
     */
    bool createFlightPlan();
    
    /**
     * Executes the next step in the flight plan
     * @param currentTime Current simulation time
     * @return True if a step was executed
     */
    bool executeFlightPlanStep(double currentTime);

private:
    std::string m_id;                          // Unique flight identifier
    std::shared_ptr<Aircraft> m_aircraft;      // Associated aircraft
    Status m_status;                           // Current flight status
    bool m_isEmergency;                        // Flag for emergency status
    double m_scheduledTime;                    // Scheduled time for this flight
    double m_activationTime;                   // Time when this flight was activated
    double m_estimatedCompletionTime;          // Estimated time for completion
    std::string m_statusReason;                // Reason for current status (e.g., cancellation reason)
    std::weak_ptr<Runway> m_assignedRunway;    // Weak pointer to assigned runway
    
    // Flight plan is a sequence of operations with timing
    struct FlightPlanStep {
        std::function<bool()> operation;       // Operation to perform
        double relativeTimeOffset;             // Time offset from activation
    };
    
    std::vector<FlightPlanStep> m_flightPlan;  // Sequence of operations for this flight
    size_t m_currentPlanStep;                  // Current step in the flight plan
    
    mutable std::mutex m_mutex;                // Mutex for thread-safe operations
    
    /**
     * Validates if a status transition is allowed
     * @param newStatus Status to transition to
     * @return True if transition is valid
     */
    bool isValidStatusTransition(Status newStatus) const;
    
    /**
     * Creates an arrival flight plan
     * @return True if creation was successful
     */
    bool createArrivalFlightPlan();
    
    /**
     * Creates a departure flight plan
     * @return True if creation was successful
     */
    bool createDepartureFlightPlan();
    
    /**
     * Creates an emergency flight plan
     * @return True if creation was successful
     */
    bool createEmergencyFlightPlan();
};

} // namespace AirControlX

#endif // FLIGHT_H

