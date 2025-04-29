#include "../include/Airline.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <chrono>

namespace AirControlX {

// Constructor
Airline::Airline(const std::string& name, AircraftType type, int totalAircrafts, int activeFlights)
    : m_name(name)
    , m_primaryType(type)
    , m_totalAircrafts(totalAircrafts)
    , m_activeFlights(activeFlights)
    , m_violationCount(0)
    , m_lastFlightScheduleTime(0.0)
{
    // Initialize random seed for flight generation
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
}

std::string Airline::getName() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_name;
}

AircraftType Airline::getPrimaryType() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_primaryType;
}

int Airline::getTotalAircrafts() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalAircrafts;
}

int Airline::getActiveFlights() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeFlights;
}

std::shared_ptr<Aircraft> Airline::createAircraft(FlightDirection direction, bool forceEmergency) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if we can schedule more flights
    if (!canScheduleFlight()) {
        return nullptr;
    }

    // Determine aircraft type based on direction and emergency status
    AircraftType aircraftType = determineAircraftType(direction, forceEmergency);

    // Generate a unique flight ID
    std::string flightId = generateFlightId();

    // Create the aircraft
    auto aircraft = std::make_shared<Aircraft>(flightId, aircraftType, direction, 
                                               std::shared_ptr<Airline>(this, [](Airline*){}));

    // Add to fleet
    m_fleet[flightId] = aircraft;
    m_activeFlights++;

    return aircraft;
}

std::vector<std::shared_ptr<Aircraft>> Airline::getAllAircraft() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<Aircraft>> aircraft;
    aircraft.reserve(m_fleet.size());
    
    for (const auto& pair : m_fleet) {
        aircraft.push_back(pair.second);
    }
    
    return aircraft;
}

std::vector<std::shared_ptr<Aircraft>> Airline::getAircraftWithViolations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<std::shared_ptr<Aircraft>> violatingAircraft;
    
    for (const auto& pair : m_fleet) {
        if (pair.second->hasActiveAVN()) {
            violatingAircraft.push_back(pair.second);
        }
    }
    
    return violatingAircraft;
}

int Airline::getTotalViolationCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_violationCount;
}

double Airline::processAVNPayments() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    double totalPayment = 0.0;
    
    // Calculate payment amount based on violation types
    const double BASIC_VIOLATION_FEE = 1000.0;  // Basic fee per violation
    
    for (const auto& pair : m_fleet) {
        auto aircraft = pair.second;
        
        if (aircraft->hasActiveAVN()) {
            auto violations = aircraft->getActiveAVNs();
            
            for (const auto& violation : violations) {
                // Here you could implement different fee rates for different violation types
                // For simplicity, we'll use a flat fee
                totalPayment += BASIC_VIOLATION_FEE;
            }
            
            // Clear violations after payment (in a real system, you might want to keep a record)
            // For simplicity, we're not actually clearing them here
        }
    }
    
    return totalPayment;
}

bool Airline::scheduleFlightIfNeeded(double currentTime, FlightDirection direction) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Get the interval for this direction
    auto intervalIter = FLIGHT_GENERATION_INTERVAL.find(direction);
    if (intervalIter == FLIGHT_GENERATION_INTERVAL.end()) {
        return false;
    }
    
    double interval = intervalIter->second;
    
    // Check if enough time has passed since the last flight schedule
    if (currentTime - m_lastFlightScheduleTime < interval) {
        return false;
    }
    
    // Check if we have capacity for a new flight
    if (!canScheduleFlight()) {
        return false;
    }
    
    // Determine if this should be an emergency flight
    bool isEmergency = false;
    auto emergencyProbIter = EMERGENCY_PROBABILITY.find(direction);
    if (emergencyProbIter != EMERGENCY_PROBABILITY.end()) {
        double emergencyProb = emergencyProbIter->second;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        isEmergency = (dist(gen) < emergencyProb);
    }
    
    // Create a new aircraft
    auto aircraft = createAircraft(direction, isEmergency);
    if (!aircraft) {
        return false;
    }
    
    // Update last schedule time
    m_lastFlightScheduleTime = currentTime;
    
    return true;
}

void Airline::updateAllAircraft(double deltaTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& pair : m_fleet) {
        pair.second->update(deltaTime);
    }
}

int Airline::handleGroundFaults() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int faultsHandled = 0;
    
    // Identify aircraft with ground faults
    std::vector<std::string> faultyAircraftIds;
    
    for (const auto& pair : m_fleet) {
        if (pair.second->hasGroundFault()) {
            faultyAircraftIds.push_back(pair.first);
            faultsHandled++;
        }
    }
    
    // Remove faulty aircraft from the fleet (in a real system, you might put them in a maintenance queue)
    for (const auto& id : faultyAircraftIds) {
        m_fleet.erase(id);
        m_activeFlights--;
    }
    
    return faultsHandled;
}

std::string Airline::toString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Airline: " << m_name << "\n";
    ss << "Primary Type: " << AircraftTypeToString(m_primaryType) << "\n";
    ss << "Total Aircraft: " << m_totalAircrafts << "\n";
    ss << "Active Flights: " << m_activeFlights << "\n";
    ss << "Violation Count: " << m_violationCount << "\n";
    ss << "Last Flight Schedule Time: " << m_lastFlightScheduleTime << "\n";
    ss << "Fleet Size: " << m_fleet.size() << " aircraft\n";
    
    return ss.str();
}

// Private methods

std::string Airline::generateFlightId() const {
    // Generate a unique flight ID based on airline code and a random number
    
    // Extract first letter of each word in the airline name to create a code
    std::string airlineCode;
    std::istringstream iss(m_name);
    std::string word;
    while (iss >> word) {
        if (!word.empty()) {
            airlineCode += toupper(word[0]);
        }
    }
    
    if (airlineCode.empty()) {
        airlineCode = "XX"; // Default code if name is empty
    }
    
    // Add a random number
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(100, 9999);
    
    int flightNumber = dist(gen);
    
    // Combine to create a unique ID (e.g., "BA1234")
    std::stringstream idss;
    idss << airlineCode << flightNumber;
    
    return idss.str();
}

AircraftType Airline::determineAircraftType(FlightDirection direction, bool forceEmergency) const {
    // Default to the airline's primary type
    AircraftType result = m_primaryType;
    
    // Override for emergency flights
    if (forceEmergency) {
        return AircraftType::EMERGENCY;
    }
    
    // For certain airlines, always use their primary type
    if (m_primaryType == AircraftType::EMERGENCY || m_primaryType == AircraftType::CARGO) {
        return m_primaryType;
    }
    
    // For commercial airlines, mostly use commercial aircraft,
    // but occasionally use cargo or emergency based on direction
    if (m_primaryType == AircraftType::COMMERCIAL) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        
        // Check emergency probability for this direction
        auto emergencyProbIter = EMERGENCY_PROBABILITY.find(direction);
        if (emergencyProbIter != EMERGENCY_PROBABILITY.end()) {
            double emergencyProb = emergencyProbIter->second;
            
            if (dist(gen) < emergencyProb) {
                return AircraftType::EMERGENCY;
            }
        }
        
        // 5% chance of cargo for commercial airlines
        if (dist(gen) < 0.05) {
            return AircraftType::CARGO;
        }
    }
    
    return result;
}

bool Airline::canScheduleFlight() const {
    // Check if we've reached our maximum active flights
    return (m_fleet.size() < static_cast<size_t>(m_totalAircrafts));
}

} // namespace AirControlX

