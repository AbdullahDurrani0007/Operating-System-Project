#include "../include/Flight.h"
#include "../include/Aircraft.h"
#include "../include/Runway.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace AirControlX {

// Flight constructor
Flight::Flight(std::shared_ptr<Aircraft> aircraft, double scheduledTime, bool isEmergency)
    : m_aircraft(aircraft)
    , m_status(Status::SCHEDULED)
    , m_isEmergency(isEmergency)
    , m_scheduledTime(scheduledTime)
    , m_activationTime(0.0)
    , m_estimatedCompletionTime(0.0)
    , m_currentPlanStep(0)
{
    // Set the flight ID from the aircraft
    if (aircraft) {
        m_id = aircraft->getId();
        
        // If this is an emergency flight, update the aircraft type if needed
        if (m_isEmergency && aircraft->getType() != AircraftType::EMERGENCY) {
            // Note: Since Aircraft::setType() is not provided, we can't change the type here
            // In a real implementation, we might want to add that method
        }
        
        // Create an appropriate flight plan based on direction and emergency status
        createFlightPlan();
        
        // Update estimated completion time based on flight plan
        calculateEstimatedCompletionTime();
    }
}

// Public methods

std::string Flight::getId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_id;
}

std::shared_ptr<Aircraft> Flight::getAircraft() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_aircraft;
}

Flight::Status Flight::getStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status;
}

bool Flight::isEmergency() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isEmergency;
}

void Flight::setEmergency(bool isEmergency) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only allow setting emergency status for scheduled or active flights
    if (m_status == Status::SCHEDULED || m_status == Status::ACTIVE) {
        m_isEmergency = isEmergency;
        
        // If becoming an emergency, update the flight status
        if (isEmergency && m_status != Status::EMERGENCY) {
            m_status = Status::EMERGENCY;
            
            // Recreate flight plan for emergency operations
            createEmergencyFlightPlan();
            calculateEstimatedCompletionTime();
        } 
        // If no longer an emergency and currently in emergency status, revert to ACTIVE
        else if (!isEmergency && m_status == Status::EMERGENCY) {
            m_status = Status::ACTIVE;
            
            // Recreate appropriate flight plan
            createFlightPlan();
            calculateEstimatedCompletionTime();
        }
    }
}

bool Flight::assignRunway(std::shared_ptr<Runway> runway) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Ensure flight is in a state where runway assignment is valid
    if (m_status != Status::SCHEDULED && m_status != Status::ACTIVE && m_status != Status::EMERGENCY) {
        return false;
    }
    
    // Check if this flight already has a runway assigned
    if (auto existingRunway = m_assignedRunway.lock()) {
        // Already has a runway assigned
        return false;
    }
    
    // Ensure the runway is valid for this flight's aircraft
    if (!runway->canUseForDirection(m_aircraft->getDirection()) ||
        !runway->canUseForAircraftType(m_aircraft->getType())) {
        return false;
    }
    
    // Attempt to assign the runway to this flight's aircraft
    if (!runway->assignAircraft(m_aircraft)) {
        return false;
    }
    
    // Store the runway
    m_assignedRunway = runway;
    
    return true;
}

bool Flight::releaseRunway() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Get the assigned runway (if any)
    auto runway = m_assignedRunway.lock();
    if (!runway) {
        // No runway assigned
        return false;
    }
    
    // Release the runway
    if (!runway->releaseAircraft(m_aircraft)) {
        return false;
    }
    
    // Clear the assigned runway
    m_assignedRunway.reset();
    
    return true;
}

std::shared_ptr<Runway> Flight::getAssignedRunway() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_assignedRunway.lock();
}

bool Flight::activate(double currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if the flight is in a state where activation is valid
    if (!isValidStatusTransition(Status::ACTIVE)) {
        return false;
    }
    
    // Update the status
    m_status = m_isEmergency ? Status::EMERGENCY : Status::ACTIVE;
    m_activationTime = currentTime;
    
    // Calculate estimated completion time
    calculateEstimatedCompletionTime();
    
    return true;
}

bool Flight::complete() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if the flight is in a state where completion is valid
    if (!isValidStatusTransition(Status::COMPLETED)) {
        return false;
    }
    
    // Release the runway if one is assigned
    if (auto runway = m_assignedRunway.lock()) {
        runway->releaseAircraft(m_aircraft);
        m_assignedRunway.reset();
    }
    
    // Update the status
    m_status = Status::COMPLETED;
    
    return true;
}

bool Flight::cancel(const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if the flight is in a state where cancellation is valid
    if (!isValidStatusTransition(Status::CANCELED)) {
        return false;
    }
    
    // Release the runway if one is assigned
    if (auto runway = m_assignedRunway.lock()) {
        runway->releaseAircraft(m_aircraft);
        m_assignedRunway.reset();
    }
    
    // Update the status
    m_status = Status::CANCELED;
    m_statusReason = reason;
    
    return true;
}

bool Flight::divert(const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if the flight is in a state where diversion is valid
    if (!isValidStatusTransition(Status::DIVERTED)) {
        return false;
    }
    
    // Release the runway if one is assigned
    if (auto runway = m_assignedRunway.lock()) {
        runway->releaseAircraft(m_aircraft);
        m_assignedRunway.reset();
    }
    
    // Update the status
    m_status = Status::DIVERTED;
    m_statusReason = reason;
    
    return true;
}

bool Flight::isReadyForNextPhase(double currentTime) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only active flights can transition phases
    if (m_status != Status::ACTIVE && m_status != Status::EMERGENCY) {
        return false;
    }
    
    // If no flight plan or all steps completed, can't proceed
    if (m_flightPlan.empty() || m_currentPlanStep >= m_flightPlan.size()) {
        return false;
    }
    
    // Check if enough time has passed for the next step
    double elapsedTime = currentTime - m_activationTime;
    return (elapsedTime >= m_flightPlan[m_currentPlanStep].relativeTimeOffset);
}

bool Flight::transitionToNextPhase() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only active flights can transition phases
    if (m_status != Status::ACTIVE && m_status != Status::EMERGENCY) {
        return false;
    }
    
    // Delegate to the aircraft to handle the actual phase transition
    return m_aircraft->transitionToNextPhase();
}

double Flight::getScheduledTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scheduledTime;
}

double Flight::getActivationTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activationTime;
}

double Flight::getEstimatedCompletionTime() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_estimatedCompletionTime;
}

double Flight::getDelay(double currentTime) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // If not yet activated, calculate potential delay
    if (m_status == Status::SCHEDULED) {
        if (currentTime > m_scheduledTime) {
            return currentTime - m_scheduledTime;
        }
        return 0.0;
    }
    
    // If activated, calculate actual delay
    if (m_activationTime > 0.0) {
        return m_activationTime - m_scheduledTime;
    }
    
    return 0.0;
}

bool Flight::handleGroundFault() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if the aircraft has a ground fault
    if (m_aircraft && m_aircraft->hasGroundFault()) {
        // Release any assigned runway
        if (auto runway = m_assignedRunway.lock()) {
            runway->releaseAircraft(m_aircraft);
            m_assignedRunway.reset();
        }
        
        // Cancel the flight due to ground fault
        m_status = Status::CANCELED;
        m_statusReason = "Ground fault detected";
        return true;
    }
    
    return false;
}

void Flight::update(double deltaTime, double currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Only update active or emergency flights
    if (m_status != Status::ACTIVE && m_status != Status::EMERGENCY) {
        return;
    }
    
    // Update the aircraft state
    if (m_aircraft) {
        m_aircraft->update(deltaTime);
        
        // Check for ground faults
        if (m_aircraft->hasGroundFault()) {
            handleGroundFault();
            return;
        }
    }
    
    // Execute flight plan steps if ready
    if (isReadyForNextPhase(currentTime)) {
        executeFlightPlanStep(currentTime);
    }
}

std::string Flight::toString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::stringstream ss;
    ss << "Flight ID: " << m_id << "\n";
    
    // Convert status to string
    std::string statusStr;
    switch (m_status) {
        case Status::SCHEDULED:
            statusStr = "Scheduled";
            break;
        case Status::ACTIVE:
            statusStr = "Active";
            break;
        case Status::COMPLETED:
            statusStr = "Completed";
            break;
        case Status::CANCELED:
            statusStr = "Canceled";
            break;
        case Status::DIVERTED:
            statusStr = "Diverted";
            break;
        case Status::EMERGENCY:
            statusStr = "Emergency";
            break;
        default:
            statusStr = "Unknown";
            break;
    }
    
    ss << "Status: " << statusStr << "\n";
    
    if (!m_statusReason.empty()) {
        ss << "Reason: " << m_statusReason << "\n";
    }
    
    ss << "Emergency: " << (m_isEmergency ? "Yes" : "No") << "\n";
    ss << "Scheduled Time: " << std::fixed << std::setprecision(1) << m_scheduledTime << "\n";
    
    if (m_activationTime > 0.0) {
        ss << "Activation Time: " << std::fixed << std::setprecision(1) << m_activationTime << "\n";
    }
    
    if (m_estimatedCompletionTime > 0.0) {
        ss << "Estimated Completion Time: " << std::fixed << std::setprecision(1) << m_estimatedCompletionTime << "\n";
    }
    
    // Show aircraft details
    if (m_aircraft) {
        ss << "Aircraft Type: " << AircraftTypeToString(m_aircraft->getType()) << "\n";
        ss << "Direction: " << FlightDirectionToString(m_aircraft->getDirection()) << "\n";
        ss << "Current Phase: " << FlightPhaseToString(m_aircraft->getCurrentPhase()) << "\n";
        ss << "Current Speed: " << std::fixed << std::setprecision(1) << m_aircraft->getCurrentSpeed() << " km/h\n";
    }
    
    // Show runway assignment
    if (auto runway = m_assignedRunway.lock()) {
        ss << "Assigned Runway: " << RunwayIdToString(runway->getId()) << "\n";
    } else {
        ss << "Assigned Runway: None\n";
    }
    
    // Show flight plan status
    ss << "Flight Plan Progress: " << m_currentPlanStep << "/" << m_flightPlan.size() << "\n";
    
    return ss.str();
}

bool Flight::createFlightPlan() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Clear any existing flight plan
    m_flightPlan.clear();
    m_currentPlanStep = 0;
    
    // Create appropriate flight plan based on direction
    if (m_aircraft) {
        if (m_isEmergency) {
            return createEmergencyFlightPlan();
        } else if (m_aircraft->isArrival()) {
            return createArrivalFlightPlan();
        } else {
            return createDepartureFlightPlan();
        }
    }
    
    return false;
}

bool Flight::executeFlightPlanStep(double currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if there are steps to execute
    if (m_flightPlan.empty() || m_currentPlanStep >= m_flightPlan.size()) {
        return false;
    }
    
    // Check if enough time has passed for this step
    double elapsedTime = currentTime - m_activationTime;
    if (elapsedTime < m_flightPlan[m_currentPlanStep].relativeTimeOffset) {
        return false;
    }
    
    // Execute the operation
    bool success = m_flightPlan[m_currentPlanStep].operation();
    
    // Move to the next step
    m_currentPlanStep++;
    
    // If this was the last step, mark the flight as completed
    if (m_currentPlanStep >= m_flightPlan.size()) {
        complete();
    }
    
    return success;
}

// Private methods

bool Flight::isValidStatusTransition(Status newStatus) const {
    // Check if the transition from current status to new status is valid
    
    switch (m_status) {
        case Status::SCHEDULED:
            // From SCHEDULED, can go to ACTIVE, EMERGENCY, or CANCELED
            return (newStatus == Status::ACTIVE || 
                    newStatus == Status::EMERGENCY || 
                    newStatus == Status::CANCELED);
        
        case Status::ACTIVE:
            // From ACTIVE, can go to COMPLETED, CANCELED, DIVERTED, or EMERGENCY
            return (newStatus == Status::COMPLETED || 
                    newStatus == Status::CANCELED || 
                    newStatus == Status::DIVERTED || 
                    newStatus == Status::EMERGENCY);
        
        case Status::EMERGENCY:
            // From EMERGENCY, can go to COMPLETED, CANCELED, or DIVERTED
            return (newStatus == Status::COMPLETED || 
                    newStatus == Status::CANCELED || 
                    newStatus == Status::DIVERTED);
        
        case Status::COMPLETED:
        case Status::CANCELED:
        case Status::DIVERTED:
            // Terminal states - no transitions allowed
            return false;
        
        default:
            return false;
    }
}

bool Flight::createArrivalFlightPlan() {
    // Create a flight plan for an arriving aircraft
    
    // Ensure we have a valid aircraft
    if (!m_aircraft) {
        return false;
    }
    
    // Clear any existing plan
    m_flightPlan.clear();
    m_currentPlanStep = 0;
    
    // Add flight plan steps with appropriate timing
    
    // Step 1: Transition from HOLDING to APPROACH (after 30 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // HOLDING -> APPROACH
        },
        30.0 // seconds after activation
    });
    
    // Step 2: Transition from APPROACH to LANDING (after 60 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // APPROACH -> LANDING
        },
        60.0 // seconds after activation
    });
    
    // Step 3: Transition from LANDING to TAXI_IN (after 90 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            // Release runway as we're done with the landing phase
            releaseRunway();
            return m_aircraft->transitionToNextPhase(); // LANDING -> TAXI_IN
        },
        90.0 // seconds after activation
    });
    
    // Step 4: Transition from TAXI_IN to AT_GATE_ARRIVAL (after 120 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // TAXI_IN -> AT_GATE_ARRIVAL
        },
        120.0 // seconds after activation
    });
    
    // Step 5: Complete the flight (after 150 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return this->complete();
        },
        150.0 // seconds after activation
    });
    
    return true;
}

bool Flight::createDepartureFlightPlan() {
    // Create a flight plan for a departing aircraft
    
    // Ensure we have a valid aircraft
    if (!m_aircraft) {
        return false;
    }
    
    // Clear any existing plan
    m_flightPlan.clear();
    m_currentPlanStep = 0;
    
    // Add flight plan steps with appropriate timing
    
    // Step 1: Transition from AT_GATE_DEPARTURE to TAXI_OUT (after 30 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // AT_GATE_DEPARTURE -> TAXI_OUT
        },
        30.0 // seconds after activation
    });
    
    // Step 2: Transition from TAXI_OUT to TAKEOFF_ROLL (after 60 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // TAXI_OUT -> TAKEOFF_ROLL
        },
        60.0 // seconds after activation
    });
    
    // Step 3: Transition from TAKEOFF_ROLL to CLIMB (after 75 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return m_aircraft->transitionToNextPhase(); // TAKEOFF_ROLL -> CLIMB
        },
        75.0 // seconds after activation
    });
    
    // Step 4: Transition from CLIMB to CRUISE (after 90 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            // Release runway as we're done with takeoff
            releaseRunway();
            return m_aircraft->transitionToNextPhase(); // CLIMB -> CRUISE
        },
        90.0 // seconds after activation
    });
    
    // Step 5: Complete the flight (after 120 seconds)
    m_flightPlan.push_back({
        [this]() -> bool {
            return this->complete();
        },
        120.0 // seconds after activation
    });
    
    return true;
}

bool Flight::createEmergencyFlightPlan() {
    // Create an expedited flight plan for an emergency
    
    // Ensure we have a valid aircraft
    if (!m_aircraft) {
        return false;
    }
    
    // Clear any existing plan
    m_flightPlan.clear();
    m_currentPlanStep = 0;
    
    // Emergency plans are faster versions of regular plans
    if (m_aircraft->isArrival()) {
        // Emergency arrival plan (faster than regular arrival)
        
        // Step 1: Transition from HOLDING to APPROACH (after 15 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // HOLDING -> APPROACH
            },
            15.0 // seconds after activation (expedited)
        });
        
        // Step 2: Transition from APPROACH to LANDING (after 30 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // APPROACH -> LANDING
            },
            30.0 // seconds after activation (expedited)
        });
        
        // Step 3: Transition from LANDING to TAXI_IN (after 45 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                // Release runway as we're done with the landing phase
                releaseRunway();
                return m_aircraft->transitionToNextPhase(); // LANDING -> TAXI_IN
            },
            45.0 // seconds after activation (expedited)
        });
        
        // Step 4: Transition from TAXI_IN to AT_GATE_ARRIVAL (after 60 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // TAXI_IN -> AT_GATE_ARRIVAL
            },
            60.0 // seconds after activation (expedited)
        });
        
        // Step 5: Complete the flight (after 75 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return this->complete();
            },
            75.0 // seconds after activation (expedited)
        });
    }
    else {
        // Emergency departure plan (faster than regular departure)
        
        // Step 1: Transition from AT_GATE_DEPARTURE to TAXI_OUT (after 15 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // AT_GATE_DEPARTURE -> TAXI_OUT
            },
            15.0 // seconds after activation (expedited)
        });
        
        // Step 2: Transition from TAXI_OUT to TAKEOFF_ROLL (after 30 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // TAXI_OUT -> TAKEOFF_ROLL
            },
            30.0 // seconds after activation (expedited)
        });
        
        // Step 3: Transition from TAKEOFF_ROLL to CLIMB (after 37.5 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return m_aircraft->transitionToNextPhase(); // TAKEOFF_ROLL -> CLIMB
            },
            37.5 // seconds after activation (expedited)
        });
        
        // Step 4: Transition from CLIMB to CRUISE (after 45 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                // Release runway as we're done with takeoff
                releaseRunway();
                return m_aircraft->transitionToNextPhase(); // CLIMB -> CRUISE
            },
            45.0 // seconds after activation (expedited)
        });
        
        // Step 5: Complete the flight (after 60 seconds - expedited)
        m_flightPlan.push_back({
            [this]() -> bool {
                return this->complete();
            },
            60.0 // seconds after activation (expedited)
        });
    }
    
    return true;
}

void Flight::calculateEstimatedCompletionTime() {
    // Calculate the estimated completion time based on flight plan
    
    if (m_flightPlan.empty()) {
        m_estimatedCompletionTime = 0.0;
        return;
    }
    
    // Get the time offset of the last step in the flight plan
    double lastStepTime = m_flightPlan.back().relativeTimeOffset;
    
    // If the flight is already active, calculate based on activation time
    if (m_status == Status::ACTIVE || m_status == Status::EMERGENCY) {
        if (m_activationTime > 0.0) {
            m_estimatedCompletionTime = m_activationTime + lastStepTime;
        }
    }
    // Otherwise, calculate based on scheduled time
    else {
        m_estimatedCompletionTime = m_scheduledTime + lastStepTime;
    }
}

} // namespace AirControlX

