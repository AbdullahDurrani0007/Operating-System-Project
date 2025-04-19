#include "../include/Runway.h"
#include "../include/Aircraft.h"
#include <iostream>
#include <sstream>
#include <string>

namespace AirControlX {

// Constructor
Runway::Runway(RunwayId id)
    : m_id(id)
    , m_status(Status::AVAILABLE)
    , m_assignedAircraft(nullptr)
    , m_usageCount(0)
    , m_totalUsageTime(0.0)
    , m_lastAssignmentTime(std::chrono::steady_clock::now())
{
    // Initialize runway with default status
}

RunwayId Runway::getId() const {
    return m_id; // This is a simple getter, no need for mutex lock
}

Runway::Status Runway::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

bool Runway::isAvailable() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (m_status == Status::AVAILABLE);
}

bool Runway::assignAircraft(std::shared_ptr<Aircraft> aircraft) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if runway is available
    if (m_status != Status::AVAILABLE) {
        return false;
    }
    
    // Check if aircraft is valid
    if (!aircraft) {
        return false;
    }
    
    // Check if the runway can be used for this aircraft's direction
    if (!canUseForDirection(aircraft->getDirection())) {
        return false;
    }
    
    // Check if the runway can be used for this aircraft type
    if (!canUseForAircraftType(aircraft->getType())) {
        return false;
    }
    
    // Assign the aircraft
    m_assignedAircraft = aircraft;
    m_status = Status::IN_USE;
    m_lastAssignmentTime = std::chrono::steady_clock::now();
    
    // Update the aircraft with the runway assignment
    aircraft->assignRunway(m_id);
    
    // Increment usage count
    m_usageCount++;
    
    return true;
}

bool Runway::releaseAircraft(std::shared_ptr<Aircraft> aircraft) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if runway is in use
    if (m_status != Status::IN_USE) {
        return false;
    }
    
    // Check if the correct aircraft is releasing the runway
    if (!m_assignedAircraft || m_assignedAircraft != aircraft) {
        return false;
    }
    
    // Calculate usage time for this session
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAssignmentTime).count();
    m_totalUsageTime += static_cast<double>(duration);
    
    // Release the runway
    m_assignedAircraft = nullptr;
    m_status = Status::AVAILABLE;
    
    return true;
}

std::shared_ptr<Aircraft> Runway::getAssignedAircraft() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_assignedAircraft;
}

bool Runway::canUseForDirection(FlightDirection direction) const {
    // This method doesn't modify state, so we can avoid locking
    // unless we need to access member variables
    return isValidRunwayForDirection(m_id, direction);
}

bool Runway::canUseForAircraftType(AircraftType type) const {
    // This method doesn't modify state, so we can avoid locking
    // unless we need to access member variables
    return isValidRunwayForAircraftType(m_id, type);
}

bool Runway::setStatus(Status status) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Cannot change status if runway is in use, unless forcing closure
    if (m_status == Status::IN_USE && 
        (status == Status::MAINTENANCE || status == Status::WEATHER_CLOSED)) {
        // Force closure, but first record the usage time
        if (m_assignedAircraft) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAssignmentTime).count();
            m_totalUsageTime += static_cast<double>(duration);
        }
    }
    
    m_status = status;
    
    // If setting to anything other than IN_USE, clear the assigned aircraft
    if (status != Status::IN_USE) {
        m_assignedAircraft = nullptr;
    }
    
    return true;
}

int Runway::getTotalUsageCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_usageCount;
}

double Runway::getTotalUsageTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // If runway is currently in use, add the current session time
    if (m_status == Status::IN_USE && m_assignedAircraft) {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAssignmentTime).count();
        return m_totalUsageTime + static_cast<double>(duration);
    }
    
    return m_totalUsageTime;
}

void Runway::update(double deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // If runway is in use, update usage time
    if (m_status == Status::IN_USE && m_assignedAircraft) {
        // Nothing to do here, as we calculate usage time when the runway is released
        // or when getTotalUsageTime() is called
    }
    
    // We could implement additional logic here if needed
    // For example, simulating wear on the runway over time
    // or handling weather changes affecting runway status
}

std::string Runway::toString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Runway: " << RunwayIdToString(m_id) << "\n";
    
    std::string statusStr;
    switch (m_status) {
        case Status::AVAILABLE:
            statusStr = "Available";
            break;
        case Status::IN_USE:
            statusStr = "In Use";
            break;
        case Status::MAINTENANCE:
            statusStr = "Under Maintenance";
            break;
        case Status::WEATHER_CLOSED:
            statusStr = "Closed due to Weather";
            break;
        default:
            statusStr = "Unknown";
            break;
    }
    
    ss << "Status: " << statusStr << "\n";
    
    if (m_assignedAircraft) {
        ss << "Assigned Aircraft: " << m_assignedAircraft->getId() << "\n";
        ss << "Aircraft Type: " << AircraftTypeToString(m_assignedAircraft->getType()) << "\n";
        ss << "Direction: " << FlightDirectionToString(m_assignedAircraft->getDirection()) << "\n";
    } else {
        ss << "Assigned Aircraft: None\n";
    }
    
    ss << "Total Usage Count: " << m_usageCount << " aircraft\n";
    ss << "Total Usage Time: " << getTotalUsageTime() << " seconds\n";
    
    return ss.str();
}

// Private static methods

bool Runway::isValidRunwayForDirection(RunwayId id, FlightDirection direction) {
    // Implement runway assignment rules according to project requirements
    
    // RWY-A: North-South alignment (arrivals)
    if (id == RunwayId::RWY_A) {
        // Valid for North and South (arrivals)
        return (direction == FlightDirection::NORTH || direction == FlightDirection::SOUTH);
    }
    
    // RWY-B: East-West alignment (departures)
    if (id == RunwayId::RWY_B) {
        // Valid for East and West (departures)
        return (direction == FlightDirection::EAST || direction == FlightDirection::WEST);
    }
    
    // RWY-C: Flexible for cargo/emergency/overflow
    if (id == RunwayId::RWY_C) {
        // Valid for any direction, but we'll enforce cargo/emergency restrictions in canUseForAircraftType
        return true;
    }
    
    return false;
}

bool Runway::isValidRunwayForAircraftType(RunwayId id, AircraftType type) {
    // Implement runway assignment rules according to project requirements
    
    // RWY-A and RWY-B can be used by any aircraft type
    if (id == RunwayId::RWY_A || id == RunwayId::RWY_B) {
        return true;
    }
    
    // RWY-C is exclusively used for cargo or emergency flights
    if (id == RunwayId::RWY_C) {
        return (type == AircraftType::CARGO || type == AircraftType::EMERGENCY);
    }
    
    return false;
}

} // namespace AirControlX

