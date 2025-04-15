#ifndef SPEED_MONITOR_H
#define SPEED_MONITOR_H

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>
#include "Constants.h"

namespace AirControlX {

// Forward declarations
class Aircraft;
class Flight;
class Airline;

/**
 * @struct ViolationRecord
 * @brief Records details about a speed violation
 */
struct ViolationRecord {
    std::string aircraftId;           // ID of aircraft that violated
    std::string airlineName;          // Name of airline responsible
    FlightPhase phase;                // Flight phase when violation occurred
    double actualSpeed;               // Actual speed at violation time
    double minAllowedSpeed;           // Minimum allowed speed for the phase
    double maxAllowedSpeed;           // Maximum allowed speed for the phase
    double timestamp;                 // Simulation time when violation occurred
    std::string description;          // Detailed description of the violation
};

/**
 * @class SpeedMonitor
 * @brief Monitors aircraft speeds and enforces speed limits for different flight phases.
 *
 * The SpeedMonitor class is responsible for checking aircraft speeds against
 * phase-specific limits, detecting violations, and generating Airspace Violation
 * Notices (AVNs) for non-compliant aircraft.
 */
class SpeedMonitor {
public:
    /**
     * SpeedMonitor constructor
     */
    SpeedMonitor();
    
    /**
     * Destructor
     */
    ~SpeedMonitor() = default;
    
    /**
     * Checks if an aircraft's speed is valid for its current flight phase
     * @param aircraft Pointer to the aircraft to check
     * @return True if speed is valid, false if a violation exists
     */
    bool isSpeedValid(std::shared_ptr<Aircraft> aircraft);
    
    /**
     * Monitors an aircraft's speed and issues AVN if necessary
     * @param aircraft Pointer to the aircraft to monitor
     * @param currentTime Current simulation time
     * @return True if violation was detected and AVN issued
     */
    bool monitorAircraftSpeed(std::shared_ptr<Aircraft> aircraft, double currentTime);
    
    /**
     * Gets the speed limits for a specific flight phase
     * @param phase Flight phase
     * @return Speed limits (min and max)
     */
    SpeedLimits getSpeedLimitsForPhase(FlightPhase phase) const;
    
    /**
     * Gets a description of the speed requirements for a phase
     * @param phase Flight phase
     * @return String description of speed requirements
     */
    std::string getPhaseSpeedRequirements(FlightPhase phase) const;
    
    /**
     * Gets all violation records
     * @return Vector of violation records
     */
    std::vector<ViolationRecord> getAllViolations() const;
    
    /**
     * Gets violation records for a specific aircraft
     * @param aircraftId ID of the aircraft
     * @return Vector of violation records for the aircraft
     */
    std::vector<ViolationRecord> getViolationsForAircraft(const std::string& aircraftId) const;
    
    /**
     * Gets violation records for a specific airline
     * @param airlineName Name of the airline
     * @return Vector of violation records for the airline
     */
    std::vector<ViolationRecord> getViolationsForAirline(const std::string& airlineName) const;
    
    /**
     * Gets the total count of violations
     * @return Total violation count
     */
    int getTotalViolationCount() const;
    
    /**
     * Gets violation counts by airline
     * @return Map of airline name to violation count
     */
    std::unordered_map<std::string, int> getViolationCountsByAirline() const;
    
    /**
     * Gets violation counts by flight phase
     * @return Map of flight phase to violation count
     */
    std::unordered_map<FlightPhase, int> getViolationCountsByPhase() const;
    
    /**
     * Generates an AVN for an aircraft
     * @param aircraft Pointer to the aircraft
     * @param description Description of the violation
     * @param currentTime Current simulation time
     * @return True if AVN was generated successfully
     */
    bool generateAVN(std::shared_ptr<Aircraft> aircraft, const std::string& description, double currentTime);
    
    /**
     * Clears all violation records (for testing or reset)
     */
    void clearViolationRecords();
    
    /**
     * Calculates fines for violations (for billing airlines)
     * @param airlineName Name of the airline
     * @return Total fine amount
     */
    double calculateFines(const std::string& airlineName) const;
    
    /**
     * Updates the speed monitor state for one simulation step
     * @param aircraft Vector of all active aircraft
     * @param currentTime Current simulation time
     */
    void update(const std::vector<std::shared_ptr<Aircraft>>& aircraft, double currentTime);
    
    /**
     * Gets a string representation of the speed monitor state
     * @return String description
     */
    std::string toString() const;

private:
    // Map to track speeds of aircraft by ID
    std::unordered_map<std::string, std::vector<double>> m_speedHistory;
    
    // Vector of all violation records
    std::vector<ViolationRecord> m_violations;
    
    // Counters for violation statistics
    std::unordered_map<std::string, int> m_violationsByAirline;
    std::unordered_map<FlightPhase, int> m_violationsByPhase;
    
    mutable std::mutex m_mutex;    // Mutex for thread-safe operations
    
    /**
     * Records a speed violation
     * @param aircraft Pointer to the aircraft
     * @param actualSpeed Speed at violation time
     * @param minAllowedSpeed Minimum allowed speed
     * @param maxAllowedSpeed Maximum allowed speed
     * @param currentTime Current simulation time
     * @return Violation record that was created
     */
    ViolationRecord recordViolation(
        std::shared_ptr<Aircraft> aircraft,
        double actualSpeed,
        double minAllowedSpeed,
        double maxAllowedSpeed,
        double currentTime
    );
    
    /**
     * Generates a description for a speed violation
     * @param aircraft Pointer to the aircraft
     * @param actualSpeed Speed at violation time
     * @param minAllowedSpeed Minimum allowed speed
     * @param maxAllowedSpeed Maximum allowed speed
     * @return Description string
     */
    std::string generateViolationDescription(
        std::shared_ptr<Aircraft> aircraft,
        double actualSpeed,
        double minAllowedSpeed,
        double maxAllowedSpeed
    ) const;
    
    /**
     * Adds a speed data point to the history for an aircraft
     * @param aircraftId ID of the aircraft
     * @param speed Speed to record
     */
    void recordSpeedDataPoint(const std::string& aircraftId, double speed);
    
    /**
     * Check if aircraft is experiencing rapid speed changes 
     * (additional violation check beyond simple min/max)
     * @param aircraftId ID of the aircraft
     * @return True if rapid changes detected
     */
    bool detectRapidSpeedChanges(const std::string& aircraftId) const;
};

} // namespace AirControlX

#endif // SPEED_MONITOR_H

