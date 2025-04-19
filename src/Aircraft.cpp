#include "../include/Aircraft.h"
#include "../include/Airline.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>

namespace AirControlX {

// Constructor
Aircraft::Aircraft(std::string id, AircraftType type, FlightDirection direction, std::shared_ptr<Airline> airline)
    : m_id(std::move(id))
    , m_type(type)
    , m_direction(direction)
    , m_airline(airline)
    , m_currentSpeed(0.0)
    , m_assignedRunway(std::nullopt)
    , m_hasGroundFault(false)
{
    // Set initial phase based on flight direction (arrival or departure)
    setInitialPhase();
    
    // Set initial speed based on the current phase
    setInitialSpeed();
}

std::string Aircraft::getId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_id;
}

AircraftType Aircraft::getType() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_type;
}

FlightPhase Aircraft::getCurrentPhase() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentPhase;
}

double Aircraft::getCurrentSpeed() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentSpeed;
}

FlightDirection Aircraft::getDirection() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_direction;
}

bool Aircraft::isArrival() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // North and South are arrivals
    return (m_direction == FlightDirection::NORTH || m_direction == FlightDirection::SOUTH);
}

std::shared_ptr<Airline> Aircraft::getAirline() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_airline;
}

void Aircraft::assignRunway(RunwayId runwayId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_assignedRunway = runwayId;
}

std::optional<RunwayId> Aircraft::getAssignedRunway() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_assignedRunway;
}

bool Aircraft::transitionToNextPhase() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    FlightPhase nextPhase = getNextPhase();
    
    if (isValidTransition(nextPhase)) {
        m_currentPhase = nextPhase;
        
        // Update speed to be within the range for the new phase
        double newSpeed = generateRandomSpeedForPhase(nextPhase);
        m_currentSpeed = newSpeed;
        
        return true;
    }
    
    return false;
}

bool Aircraft::updateSpeed(double speedDelta) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    double newSpeed = m_currentSpeed + speedDelta;
    
    // Get phase limits
    auto limitsIter = SPEED_LIMITS.find(m_currentPhase);
    if (limitsIter != SPEED_LIMITS.end()) {
        const auto& limits = limitsIter->second;
        // Clamp speed to stay within limits unless intentionally violated
        newSpeed = std::clamp(newSpeed, limits.min, limits.max);
    }
    
    m_currentSpeed = newSpeed;
    return true;
}
bool Aircraft::setSpeed(double newSpeed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Set the speed directly without validation
    // This allows speed violations to occur and be tracked
    m_currentSpeed = newSpeed;
    
    return true;
}

bool Aircraft::isSpeedValid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Get speed limits for current phase
    auto limitsIter = SPEED_LIMITS.find(m_currentPhase);
    if (limitsIter == SPEED_LIMITS.end()) {
        // Unknown phase, consider invalid
        return false;
    }
    
    const auto& limits = limitsIter->second;
    
    // Check if current speed is within limits
    return (m_currentSpeed >= limits.min && m_currentSpeed <= limits.max);
}

void Aircraft::issueAVN(const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Add the violation to the list
    m_activeAVNs.push_back(reason);
    
    // Could also notify the airline here if needed
}

bool Aircraft::hasActiveAVN() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_activeAVNs.empty();
}

std::vector<std::string> Aircraft::getActiveAVNs() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeAVNs;
}

bool Aircraft::simulateGroundFault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only simulate faults during ground operations
    if (m_currentPhase == FlightPhase::TAXI_IN || 
        m_currentPhase == FlightPhase::AT_GATE_ARRIVAL ||
        m_currentPhase == FlightPhase::AT_GATE_DEPARTURE ||
        m_currentPhase == FlightPhase::TAXI_OUT) {
        
        // Generate a random fault with low probability
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        if (dist(gen) < 0.05) { // 5% chance of a fault
            m_hasGroundFault = true;
            return true;
        }
    }
    
    return false;
}

bool Aircraft::hasGroundFault() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hasGroundFault;
}

void Aircraft::update(double deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Simulate minor speed fluctuations
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 2.0); // Mean 0, std dev 2 km/h
    
    double speedDelta = dist(gen);
    m_currentSpeed += speedDelta;
    
    // Simulate ground faults during ground operations
    if ((m_currentPhase == FlightPhase::TAXI_IN || 
         m_currentPhase == FlightPhase::AT_GATE_ARRIVAL ||
         m_currentPhase == FlightPhase::AT_GATE_DEPARTURE ||
         m_currentPhase == FlightPhase::TAXI_OUT) && 
        !m_hasGroundFault) {
        
        std::uniform_real_distribution<double> faultDist(0.0, 1.0);
        if (faultDist(gen) < 0.001 * deltaTime) { // Rare chance of fault, scaled by time step
            m_hasGroundFault = true;
        }
    }
}

std::string Aircraft::toString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Aircraft ID: " << m_id << "\n";
    ss << "Type: " << AircraftTypeToString(m_type) << "\n";
    ss << "Direction: " << FlightDirectionToString(m_direction) << "\n";
    ss << "Phase: " << FlightPhaseToString(m_currentPhase) << "\n";
    ss << "Speed: " << m_currentSpeed << " km/h\n";
    ss << "Airline: " << (m_airline ? m_airline->getName() : "Unknown") << "\n";
    
    if (m_assignedRunway.has_value()) {
        ss << "Assigned Runway: " << RunwayIdToString(m_assignedRunway.value()) << "\n";
    } else {
        ss << "Assigned Runway: None\n";
    }
    
    ss << "Ground Fault: " << (m_hasGroundFault ? "Yes" : "No") << "\n";
    
    if (!m_activeAVNs.empty()) {
        ss << "Active AVNs:\n";
        for (const auto& avn : m_activeAVNs) {
            ss << "  - " << avn << "\n";
        }
    } else {
        ss << "Active AVNs: None\n";
    }
    
    return ss.str();
}

// Private methods

void Aircraft::setInitialPhase() {
    // Set initial phase based on direction
    if (isArrival()) {
        // Arrivals start in the holding pattern
        m_currentPhase = FlightPhase::HOLDING;
    } else {
        // Departures start at the gate
        m_currentPhase = FlightPhase::AT_GATE_DEPARTURE;
    }
}

void Aircraft::setInitialSpeed() {
    m_currentSpeed = generateRandomSpeedForPhase(m_currentPhase);
}

FlightPhase Aircraft::getNextPhase() const {
    // Determine the next phase in the sequence
    switch (m_currentPhase) {
        // Arrival sequence
        case FlightPhase::HOLDING:
            return FlightPhase::APPROACH;
        case FlightPhase::APPROACH:
            return FlightPhase::LANDING;
        case FlightPhase::LANDING:
            return FlightPhase::TAXI_IN;
        case FlightPhase::TAXI_IN:
            return FlightPhase::AT_GATE_ARRIVAL;
            
        // Departure sequence
        case FlightPhase::AT_GATE_DEPARTURE:
            return FlightPhase::TAXI_OUT;
        case FlightPhase::TAXI_OUT:
            return FlightPhase::TAKEOFF_ROLL;
        case FlightPhase::TAKEOFF_ROLL:
            return FlightPhase::CLIMB;
        case FlightPhase::CLIMB:
            return FlightPhase::CRUISE;
            
        // Terminal phases don't have a next phase
        case FlightPhase::AT_GATE_ARRIVAL:
        case FlightPhase::CRUISE:
        default:
            return m_currentPhase; // No change
    }
}

bool Aircraft::isValidTransition(FlightPhase nextPhase) const {
    // Check if the transition is valid based on the current phase
    switch (m_currentPhase) {
        case FlightPhase::HOLDING:
            return nextPhase == FlightPhase::APPROACH;
        case FlightPhase::APPROACH:
            return nextPhase == FlightPhase::LANDING;
        case FlightPhase::LANDING:
            return nextPhase == FlightPhase::TAXI_IN;
        case FlightPhase::TAXI_IN:
            return nextPhase == FlightPhase::AT_GATE_ARRIVAL;
        case FlightPhase::AT_GATE_ARRIVAL:
            return false; // Terminal phase
            
        case FlightPhase::AT_GATE_DEPARTURE:
            return nextPhase == FlightPhase::TAXI_OUT;
        case FlightPhase::TAXI_OUT:
            return nextPhase == FlightPhase::TAKEOFF_ROLL;
        case FlightPhase::TAKEOFF_ROLL:
            return nextPhase == FlightPhase::CLIMB;
        case FlightPhase::CLIMB:
            return nextPhase == FlightPhase::CRUISE;
        case FlightPhase::CRUISE:
            return false; // Terminal phase
            
        default:
            return false;
    }
}

double Aircraft::generateRandomSpeedForPhase(FlightPhase phase) const {
    // Get speed limits for the specified phase
    auto limitsIter = SPEED_LIMITS.find(phase);
    if (limitsIter == SPEED_LIMITS.end()) {
        // Unknown phase, use a default range
        return 0.0;
    }
    
    const auto& limits = limitsIter->second;
    
    // Generate a random speed within the allowed range
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(limits.min, limits.max);
    
    return dist(gen);
}

} // namespace AirControlX

