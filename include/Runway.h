#ifndef RUNWAY_H
#define RUNWAY_H

#include <string>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>
#include <chrono>
#include "Constants.h"

namespace AirControlX {

// Forward declarations
class Aircraft;

/**
 * @class Runway
 * @brief Represents a runway in the air traffic control system.
 *
 * The Runway class handles runway operations, including synchronization mechanisms
 * to ensure only one aircraft can use a runway at a time, usage tracking,
 * and enforcement of direction-based restrictions.
 */
class Runway {
public:
    /**
     * Enum representing the current status of a runway
     */
    enum class Status {
        AVAILABLE,      // Runway is free for use
        IN_USE,         // Runway is currently being used by an aircraft
        MAINTENANCE,    // Runway is under maintenance and unavailable
        WEATHER_CLOSED  // Runway is closed due to weather conditions
    };

    /**
     * Runway constructor
     * @param id Identifier for this runway
     */
    explicit Runway(RunwayId id);
    
    /**
     * Destructor
     */
    ~Runway() = default;
    
    /**
     * Gets the runway ID
     * @return Runway identifier
     */
    RunwayId getId() const;
    
    /**
     * Gets the current status of the runway
     * @return Current runway status
     */
    Status getStatus() const;
    
    /**
     * Checks if the runway is currently available
     * @return True if available, false otherwise
     */
    bool isAvailable() const;
    
    /**
     * Attempts to assign an aircraft to this runway (thread-safe)
     * @param aircraft Pointer to the aircraft requesting the runway
     * @return True if assignment was successful, false otherwise
     */
    bool assignAircraft(std::shared_ptr<Aircraft> aircraft);
    
    /**
     * Releases the runway from the currently assigned aircraft (thread-safe)
     * @param aircraft Pointer to the aircraft releasing the runway
     * @return True if release was successful, false if the aircraft wasn't assigned to this runway
     */
    bool releaseAircraft(std::shared_ptr<Aircraft> aircraft);
    
    /**
     * Gets the aircraft currently using this runway
     * @return Pointer to the assigned aircraft, or nullptr if none
     */
    std::shared_ptr<Aircraft> getAssignedAircraft() const;
    
    /**
     * Checks if the runway can be used for a specific flight direction
     * @param direction Flight direction
     * @return True if the runway can be used for this direction
     */
    bool canUseForDirection(FlightDirection direction) const;
    
    /**
     * Checks if the runway can be used for a specific aircraft type
     * @param type Aircraft type
     * @return True if the runway can be used for this aircraft type
     */
    bool canUseForAircraftType(AircraftType type) const;
    
    /**
     * Sets the runway status (e.g., for maintenance or weather closure)
     * @param status New status for the runway
     * @return True if status was changed successfully
     */
    bool setStatus(Status status);
    
    /**
     * Gets the total number of aircraft that used this runway
     * @return Count of aircraft that used this runway
     */
    int getTotalUsageCount() const;
    
    /**
     * Gets the duration for which the runway has been in use
     * @return Total usage time in seconds
     */
    double getTotalUsageTime() const;
    
    /**
     * Updates the runway state for one simulation step
     * @param deltaTime Time step in seconds
     */
    void update(double deltaTime);
    
    /**
     * Gets a string representation of the runway state
     * @return String description
     */
    std::string toString() const;

private:
    RunwayId m_id;                                 // Runway identifier
    Status m_status;                               // Current runway status
    std::shared_ptr<Aircraft> m_assignedAircraft;  // Currently assigned aircraft
    int m_usageCount;                              // Count of aircraft that used this runway
    double m_totalUsageTime;                       // Total time the runway has been in use
    std::chrono::time_point<std::chrono::steady_clock> m_lastAssignmentTime; // Time of last assignment
    
    mutable std::mutex m_mutex;                    // Mutex for thread-safe operations
    
    /**
     * Checks if the given runway ID is valid for the specified direction
     * according to project requirements
     * @param id Runway ID
     * @param direction Flight direction
     * @return True if valid combination
     */
    static bool isValidRunwayForDirection(RunwayId id, FlightDirection direction);
    
    /**
     * Checks if the given runway ID is valid for the specified aircraft type
     * according to project requirements
     * @param id Runway ID
     * @param type Aircraft type
     * @return True if valid combination
     */
    static bool isValidRunwayForAircraftType(RunwayId id, AircraftType type);
};

} // namespace AirControlX

#endif // RUNWAY_H

