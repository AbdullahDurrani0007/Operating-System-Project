#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <string>
#include <map>
#include <vector>

namespace AirControlX {

// Aircraft Types
enum class AircraftType {
    COMMERCIAL,
    CARGO,
    EMERGENCY
};

// Flight Directions
enum class FlightDirection {
    NORTH,  // International Arrivals
    SOUTH,  // Domestic Arrivals
    EAST,   // International Departures
    WEST    // Domestic Departures
};

// Runway Identifiers
enum class RunwayId {
    RWY_A,  // North-South alignment (arrivals)
    RWY_B,  // East-West alignment (departures)
    RWY_C   // Flexible for cargo/emergency/overflow
};

// Flight Phases
enum class FlightPhase {
    // Arrival phases
    HOLDING,
    APPROACH,
    LANDING,
    TAXI_IN,
    AT_GATE_ARRIVAL,
    
    // Departure phases
    AT_GATE_DEPARTURE,
    TAXI_OUT,
    TAKEOFF_ROLL,
    CLIMB,
    CRUISE
};

// Speed limits for different flight phases (in km/h)
struct SpeedLimits {
    double min;
    double max;
};

// Defines the time interval for flight generation (in seconds)
const std::map<FlightDirection, double> FLIGHT_GENERATION_INTERVAL = {
    {FlightDirection::NORTH, 180.0},  // 3 minutes
    {FlightDirection::SOUTH, 120.0},  // 2 minutes
    {FlightDirection::EAST, 150.0},   // 2.5 minutes
    {FlightDirection::WEST, 240.0}    // 4 minutes
};

// Emergency probability for each direction
const std::map<FlightDirection, double> EMERGENCY_PROBABILITY = {
    {FlightDirection::NORTH, 0.10},  // 10% 
    {FlightDirection::SOUTH, 0.05},  // 5%
    {FlightDirection::EAST, 0.15},   // 15%
    {FlightDirection::WEST, 0.20}    // 20%
};

// Speed limits for different flight phases (in km/h)
const std::map<FlightPhase, SpeedLimits> SPEED_LIMITS = {
    {FlightPhase::HOLDING, {400.0, 600.0}},
    {FlightPhase::APPROACH, {240.0, 290.0}},
    {FlightPhase::LANDING, {30.0, 240.0}},
    {FlightPhase::TAXI_IN, {15.0, 30.0}},
    {FlightPhase::AT_GATE_ARRIVAL, {0.0, 5.0}},
    
    {FlightPhase::AT_GATE_DEPARTURE, {0.0, 5.0}},
    {FlightPhase::TAXI_OUT, {15.0, 30.0}},
    {FlightPhase::TAKEOFF_ROLL, {0.0, 290.0}},
    {FlightPhase::CLIMB, {250.0, 463.0}},
    {FlightPhase::CRUISE, {800.0, 900.0}}
};

// Airline information
struct AirlineInfo {
    std::string name;
    AircraftType type;
    int aircrafts;  // Total aircrafts
    int flights;    // Aircrafts in operation
};

// Predefined airlines
const std::vector<AirlineInfo> AIRLINES = {
    {"PIA", AircraftType::COMMERCIAL, 6, 4},
    {"AirBlue", AircraftType::COMMERCIAL, 4, 4},
    {"FedEx", AircraftType::CARGO, 3, 2},
    {"Pakistan Airforce", AircraftType::EMERGENCY, 2, 1},
    {"Blue Dart", AircraftType::CARGO, 2, 2},
    {"AghaKhan Air", AircraftType::EMERGENCY, 2, 1}
};

// Simulation duration in seconds (5 minutes)
const int SIMULATION_DURATION = 300;

// Helper functions for enum conversion to string
std::string AircraftTypeToString(AircraftType type);
std::string FlightDirectionToString(FlightDirection direction);
std::string RunwayIdToString(RunwayId id);
std::string FlightPhaseToString(FlightPhase phase);

} // namespace AirControlX

#endif // CONSTANTS_H

