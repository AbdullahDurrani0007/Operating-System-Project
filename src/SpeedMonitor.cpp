#include "../include/SpeedMonitor.h"
#include "../include/Aircraft.h"
#include "../include/Airline.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace AirControlX {

// Constructor
SpeedMonitor::SpeedMonitor() {
    // Initialize maps and vectors
    m_speedHistory.clear();
    m_violations.clear();
    m_violationsByAirline.clear();
    m_violationsByPhase.clear();
}

bool SpeedMonitor::isSpeedValid(std::shared_ptr<Aircraft> aircraft) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!aircraft) {
        return false;
    }
    
    // Get current phase and speed
    FlightPhase phase = aircraft->getCurrentPhase();
    double speed = aircraft->getCurrentSpeed();
    
    // Get speed limits for this phase
    auto limits = getSpeedLimitsForPhase(phase);
    
    // Check if speed is within limits
    return (speed >= limits.min && speed <= limits.max);
}

bool SpeedMonitor::monitorAircraftSpeed(std::shared_ptr<Aircraft> aircraft, double currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!aircraft) {
        return false;
    }
    
    // Get current phase and speed
    FlightPhase phase = aircraft->getCurrentPhase();
    double speed = aircraft->getCurrentSpeed();
    
    // Record the speed data point for analysis
    recordSpeedDataPoint(aircraft->getId(), speed);
    
    // Get speed limits for this phase
    auto limits = getSpeedLimitsForPhase(phase);
    
    // Check if speed is outside limits
    bool isViolation = (speed < limits.min || speed > limits.max);
    
    // Also check for rapid speed changes
    bool hasRapidChanges = detectRapidSpeedChanges(aircraft->getId());
    
    if (isViolation || hasRapidChanges) {
        // Record the violation
        ViolationRecord record = recordViolation(
            aircraft, 
            speed, 
            limits.min, 
            limits.max, 
            currentTime
        );
        
        // Generate an AVN and issue it to the aircraft
        std::string description = hasRapidChanges ? 
            "Rapid and unsafe speed changes detected" : 
            "Speed limit violation";
            
        generateAVN(aircraft, description, currentTime);
        
        return true;
    }
    
    return false;
}

SpeedLimits SpeedMonitor::getSpeedLimitsForPhase(FlightPhase phase) const {
    // Look up limits from the Constants.h map
    auto it = SPEED_LIMITS.find(phase);
    if (it != SPEED_LIMITS.end()) {
        return it->second;
    }
    
    // Default limits if phase not found (should not happen)
    return {0.0, 0.0};
}

std::string SpeedMonitor::getPhaseSpeedRequirements(FlightPhase phase) const {
    std::stringstream ss;
    auto limits = getSpeedLimitsForPhase(phase);
    
    ss << "Phase: " << FlightPhaseToString(phase) << " - ";
    ss << "Speed Requirements: " << limits.min << "-" << limits.max << " km/h";
    
    return ss.str();
}

std::vector<ViolationRecord> SpeedMonitor::getAllViolations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violations;
}

std::vector<ViolationRecord> SpeedMonitor::getViolationsForAircraft(const std::string& aircraftId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<ViolationRecord> filteredViolations;
    for (const auto& violation : m_violations) {
        if (violation.aircraftId == aircraftId) {
            filteredViolations.push_back(violation);
        }
    }
    
    return filteredViolations;
}

std::vector<ViolationRecord> SpeedMonitor::getViolationsForAirline(const std::string& airlineName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<ViolationRecord> filteredViolations;
    for (const auto& violation : m_violations) {
        if (violation.airlineName == airlineName) {
            filteredViolations.push_back(violation);
        }
    }
    
    return filteredViolations;
}

int SpeedMonitor::getTotalViolationCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_violations.size());
}

std::unordered_map<std::string, int> SpeedMonitor::getViolationCountsByAirline() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violationsByAirline;
}

std::unordered_map<FlightPhase, int> SpeedMonitor::getViolationCountsByPhase() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violationsByPhase;
}

bool SpeedMonitor::generateAVN(std::shared_ptr<Aircraft> aircraft, const std::string& description, double currentTime) {
    if (!aircraft) {
        return false;
    }
    
    // Get current phase and speed
    FlightPhase phase = aircraft->getCurrentPhase();
    double speed = aircraft->getCurrentSpeed();
    
    // Get speed limits for this phase
    auto limits = getSpeedLimitsForPhase(phase);
    
    // Create a detailed description
    std::stringstream detailedDesc;
    detailedDesc << description << " at time " << std::fixed << std::setprecision(1) << currentTime << "s: ";
    detailedDesc << "Speed " << std::fixed << std::setprecision(1) << speed << " km/h ";
    
    if (speed < limits.min) {
        detailedDesc << "is below minimum " << limits.min << " km/h ";
    } else if (speed > limits.max) {
        detailedDesc << "exceeds maximum " << limits.max << " km/h ";
    }
    
    detailedDesc << "for " << FlightPhaseToString(phase) << " phase.";
    
    // Issue the AVN to the aircraft
    aircraft->issueAVN(detailedDesc.str());
    
    return true;
}

void SpeedMonitor::clearViolationRecords() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_violations.clear();
    m_violationsByAirline.clear();
    m_violationsByPhase.clear();
}

double SpeedMonitor::calculateFines(const std::string& airlineName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Define fine amounts per violation type
    const double BASE_FINE = 1000.0;
    const double SEVERE_FINE = 5000.0;
    
    double totalFine = 0.0;
    
    for (const auto& violation : m_violations) {
        if (violation.airlineName == airlineName) {
            // Calculate fine based on severity
            double overspeed = 0.0;
            double underspeed = 0.0;
            
            if (violation.actualSpeed > violation.maxAllowedSpeed) {
                overspeed = violation.actualSpeed - violation.maxAllowedSpeed;
            } else if (violation.actualSpeed < violation.minAllowedSpeed) {
                underspeed = violation.minAllowedSpeed - violation.actualSpeed;
            }
            
            // Use the larger of the two (should only have one non-zero)
            double deviation = std::max(overspeed, underspeed);
            
            // Apply fines based on severity
            if (deviation > 100.0) {
                // Severe violation
                totalFine += SEVERE_FINE;
            } else {
                // Regular violation
                totalFine += BASE_FINE;
            }
        }
    }
    
    return totalFine;
}

void SpeedMonitor::update(const std::vector<std::shared_ptr<Aircraft>>& aircraft, double currentTime) {
    // No need to lock the mutex here as monitorAircraftSpeed handles that
    
    for (const auto& a : aircraft) {
        monitorAircraftSpeed(a, currentTime);
    }
}

std::string SpeedMonitor::toString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Speed Monitor Status:\n";
    ss << "Total Violations: " << m_violations.size() << "\n";
    
    ss << "Violations by Airline:\n";
    for (const auto& entry : m_violationsByAirline) {
        ss << "  " << entry.first << ": " << entry.second << " violations\n";
    }
    
    ss << "Violations by Phase:\n";
    for (const auto& entry : m_violationsByPhase) {
        ss << "  " << FlightPhaseToString(entry.first) << ": " << entry.second << " violations\n";
    }
    
    ss << "Recent Violations:\n";
    // Show the 5 most recent violations (or fewer if there aren't 5)
    int count = 0;
    int maxToShow = std::min(5, static_cast<int>(m_violations.size()));
    
    for (auto it = m_violations.rbegin(); it != m_violations.rend() && count < maxToShow; ++it, ++count) {
        const auto& v = *it;
        ss << "  " << v.aircraftId << " (" << v.airlineName << ") at " 
           << std::fixed << std::setprecision(1) << v.timestamp << "s: " 
           << v.description << "\n";
    }
    
    return ss.str();
}

// Private methods

ViolationRecord SpeedMonitor::recordViolation(
    std::shared_ptr<Aircraft> aircraft,
    double actualSpeed,
    double minAllowedSpeed,
    double maxAllowedSpeed,
    double currentTime
) {
    if (!aircraft) {
        // Return an empty record if aircraft is invalid
        return ViolationRecord{};
    }
    
    // Get aircraft info
    std::string aircraftId = aircraft->getId();
    std::string airlineName = aircraft->getAirline() ? aircraft->getAirline()->getName() : "Unknown";
    FlightPhase phase = aircraft->getCurrentPhase();
    
    // Generate description
    std::string description = generateViolationDescription(
        aircraft, actualSpeed, minAllowedSpeed, maxAllowedSpeed
    );
    
    // Create violation record
    ViolationRecord record{
        aircraftId,
        airlineName,
        phase,
        actualSpeed,
        minAllowedSpeed,
        maxAllowedSpeed,
        currentTime,
        description
    };
    
    // Add to records
    m_violations.push_back(record);
    
    // Update statistics
    m_violationsByAirline[airlineName]++;
    m_violationsByPhase[phase]++;
    
    return record;
}

std::string SpeedMonitor::generateViolationDescription(
    std::shared_ptr<Aircraft> aircraft,
    double actualSpeed,
    double minAllowedSpeed,
    double maxAllowedSpeed
) const {
    if (!aircraft) {
        return "Invalid aircraft";
    }
    
    FlightPhase phase = aircraft->getCurrentPhase();
    std::stringstream ss;
    
    if (actualSpeed < minAllowedSpeed) {
        ss << "Speed too low: " << std::fixed << std::setprecision(1) << actualSpeed 
           << " km/h (minimum: " << minAllowedSpeed << " km/h) during " 
           << FlightPhaseToString(phase) << " phase";
    } else if (actualSpeed > maxAllowedSpeed) {
        ss << "Speed too high: " << std::fixed << std::setprecision(1) << actualSpeed 
           << " km/h (maximum: " << maxAllowedSpeed << " km/h) during " 
           << FlightPhaseToString(phase) << " phase";
    } else {
        ss << "Unstable speed pattern detected during " 
           << FlightPhaseToString(phase) << " phase";
    }
    
    return ss.str();
}

void SpeedMonitor::recordSpeedDataPoint(const std::string& aircraftId, double speed) {
    // Add the speed data point to the history
    // Keep only the last 10 data points for analysis
    const size_t MAX_HISTORY = 10;
    
    auto& history = m_speedHistory[aircraftId];
    history.push_back(speed);
    
    if (history.size() > MAX_HISTORY) {
        history.erase(history.begin());
    }
}

bool SpeedMonitor::detectRapidSpeedChanges(const std::string& aircraftId) const {
    // Check for rapid speed changes in the aircraft's speed history
    // Must have at least 3 data points to analyze
    const auto it = m_speedHistory.find(aircraftId);
    if (it == m_speedHistory.end() || it->second.size() < 3) {
        return false;
    }
    
    const auto& history = it->second;
    
    // Calculate the average speed change between consecutive points
    double totalChange = 0.0;
    for (size_t i = 1; i < history.size(); ++i) {
        totalChange += std::abs(history[i] - history[i-1]);
    }
    
    double avgChange = totalChange / (history.size() - 1);
    
    // Define a threshold for rapid changes
    // This is a simple detection algorithm - could be more sophisticated in a real system
    const double RAPID_CHANGE_THRESHOLD = 50.0; // km/h
    
    return avgChange > RAPID_CHANGE_THRESHOLD;
}

} // namespace AirControlX

