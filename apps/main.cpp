#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <pthread.h>
#include <thread>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Forward declarations
class User;
class Appointment;
class AppointmentSystem;

// Helper functions (declarations)
namespace AppointmentHelper {
    bool isValidDate(const std::string& date);
    bool isValidTime(const std::string& time);
    bool isPastDate(const std::string& date);
    std::time_t dateTimeToTimeT(const std::string& date, const std::string& time);
}

// User class
class User {
private:
    std::string id;
    std::string name;
    std::string role; // "patient" or "doctor"

public:
    User() = default;
    User(const std::string& id, const std::string& name, const std::string& role) : id(id), name(name), role(role) {}

    std::string getId() const { return id; }
    std::string getName() const { return name; }
    std::string getRole() const { return role; }

    void setId(const std::string& id) { this->id = id; }
    void setName(const std::string& name) { this->name = name; }
    void setRole(const std::string& role) { this->role = role; }

    json toJson() const {
        json j;
        j["id"] = id;
        j["name"] = name;
        j["role"] = role;
        return j;
    }

    static User fromJson(const json& j) {
        User user;
        user.setId(j["id"].get<std::string>());
        user.setName(j["name"].get<std::string>());
        user.setRole(j["role"].get<std::string>());
        return user;
    }
};


// Appointment class
class Appointment {
private:
    std::string id;
    std::string date; // YYYY-MM-DD
    std::string time; // HH:MM
    std::vector<std::string> participants; // User IDs

public:
    Appointment() = default;
    Appointment(const std::string& id, const std::string& date, const std::string& time, const std::vector<std::string>& participants)
        : id(id), date(date), time(time), participants(participants) {}

    std::string getId() const { return id; }
    std::string getDate() const { return date; }
    std::string getTime() const { return time; }
    std::vector<std::string> getParticipants() const { return participants; }

    void setId(const std::string& id) { this->id = id; }
    void setDate(const std::string& date) { this->date = date; }
    void setTime(const std::string& time) { this->time = time; }
    void setParticipants(const std::vector<std::string>& participants) { this->participants = participants; }

    json toJson() const {
        json j;
        j["id"] = id;
        j["date"] = date;
        j["time"] = time;
        j["participants"] = participants;
        return j;
    }

    static Appointment fromJson(const json& j) {
        Appointment appointment;
        appointment.setId(j["id"].get<std::string>());
        appointment.setDate(j["date"].get<std::string>());
        appointment.setTime(j["time"].get<std::string>());
        appointment.setParticipants(j["participants"].get<std::vector<std::string>>());
        return appointment;
    }
};


// Helper functions (implementations)
namespace AppointmentHelper {
    bool isValidDate(const std::string& date) {
        std::tm t{};
        std::istringstream ss(date);
        ss >> std::get_time(&t, "%Y-%m-%d");
        return !ss.fail() && ss.eof();
    }

    bool isValidTime(const std::string& time) {
        std::tm t{};
        std::istringstream ss(time);
        ss >> std::get_time(&t, "%H:%M");
        return !ss.fail() && ss.eof();
    }

    bool isPastDate(const std::string& date) {
        std::time_t now = std::time(nullptr);
        std::tm* now_tm = std::localtime(&now);

        std::tm appointment_tm{};
        std::istringstream ss(date);
        ss >> std::get_time(&appointment_tm, "%Y-%m-%d");

        if (ss.fail()) {
            return false; // Invalid date format
        }

        appointment_tm.tm_hour = 0;
        appointment_tm.tm_min = 0;
        appointment_tm.tm_sec = 0;

        std::time_t appointment_time = std::mktime(&appointment_tm);

        // Get current date, set hours, minutes, seconds to zero
        std::tm current_date_tm = *now_tm;
        current_date_tm.tm_hour = 0;
        current_date_tm.tm_min = 0;
        current_date_tm.tm_sec = 0;

        std::time_t current_date_time = std::mktime(&current_date_tm);


        return appointment_time < current_date_time;
    }

    std::time_t dateTimeToTimeT(const std::string& date, const std::string& time) {
        std::tm t{};
        std::istringstream ss(date + " " + time);
        ss >> std::get_time(&t, "%Y-%m-%d %H:%M");
        if (ss.fail()) {
            throw std::runtime_error("Failed to parse date and time.");
        }
        return mktime(&t);
    }
}

// AppointmentSystem class
class AppointmentSystem {
private:
    std::string appointmentsFile = "appointments.json";
    std::vector<Appointment> appointments;
    std::vector<User> users;
    std::mutex appointmentsMutex;

    // Helper function to load appointments from JSON file
    void loadAppointments() {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        std::ifstream file(appointmentsFile);
        if (file.is_open()) {
            json j;
            file >> j;
            for (const auto& item : j) {
                appointments.push_back(Appointment::fromJson(item));
            }
            file.close();
        }
    }

    // Helper function to save appointments to JSON file
    void saveAppointments() {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        json j = json::array();
        for (const auto& appointment : appointments) {
            j.push_back(appointment.toJson());
        }
        std::ofstream file(appointmentsFile);
        file << std::setw(4) << j << std::endl;
        file.close();
    }


    // Helper function to find an appointment by ID
    int findAppointmentIndex(const std::string& appointmentId) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        for (size_t i = 0; i < appointments.size(); ++i) {
            if (appointments[i].getId() == appointmentId) {
                return static_cast<int>(i);
            }
        }
        return -1; // Not found
    }

public:
    AppointmentSystem() {
        loadAppointments();
    }
    ~AppointmentSystem() {
        saveAppointments();
    }


    // Adds an appointment
    bool addAppointment(const Appointment& appointment) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);

        if (!AppointmentHelper::isValidDate(appointment.getDate()) || !AppointmentHelper::isValidTime(appointment.getTime())) {
            std::cerr << "Invalid date or time format." << std::endl;
            return false;
        }

        if (AppointmentHelper::isPastDate(appointment.getDate())) {
            std::cerr << "Cannot add appointments in the past." << std::endl;
            return false;
        }

        // Check for time slot availability (basic check)
        for (const auto& existingAppointment : appointments) {
            if (existingAppointment.getDate() == appointment.getDate() &&
                existingAppointment.getTime() == appointment.getTime()) {
                std::cerr << "Time slot already booked." << std::endl;
                return false;
            }
        }

        appointments.push_back(appointment);
        saveAppointments();
        return true;
    }

    // Modifies appointment
    bool modifyAppointment(const std::string& appointmentId, const Appointment& newAppointment) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);

        if (!AppointmentHelper::isValidDate(newAppointment.getDate()) || !AppointmentHelper::isValidTime(newAppointment.getTime())) {
            std::cerr << "Invalid date or time format." << std::endl;
            return false;
        }

        if (AppointmentHelper::isPastDate(newAppointment.getDate())) {
            std::cerr << "Cannot modify appointments to a past date." << std::endl;
            return false;
        }

        int index = findAppointmentIndex(appointmentId);
        if (index == -1) {
            std::cerr << "Appointment not found." << std::endl;
            return false;
        }

        // Check for time slot availability (excluding the appointment being modified)
        for (size_t i = 0; i < appointments.size(); ++i) {
            if (static_cast<int>(i) != index && appointments[i].getDate() == newAppointment.getDate() &&
                appointments[i].getTime() == newAppointment.getTime()) {
                std::cerr << "Time slot already booked." << std::endl;
                return false;
            }
        }

        appointments[index] = newAppointment;
        saveAppointments();
        return true;
    }

    // Removes appointment
    bool removeAppointment(const std::string& appointmentId) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);

        int index = findAppointmentIndex(appointmentId);
        if (index == -1) {
            std::cerr << "Appointment not found." << std::endl;
            return false;
        }

        appointments.erase(appointments.begin() + index);
        saveAppointments();
        return true;
    }

    // List appointments by date and time
    std::vector<Appointment> listAppointments(const std::string& date, const std::string& time) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        std::vector<Appointment> results;
        for (const auto& appointment : appointments) {
            if (appointment.getDate() == date && appointment.getTime() == time) {
                results.push_back(appointment);
            }
        }
        return results;
    }

    // Search for appointments by participant (doctor or patient)
    std::vector<Appointment> searchAppointmentsByParticipant(const std::string& participantId) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        std::vector<Appointment> results;
        for (const auto& appointment : appointments) {
            for (const auto& participant : appointment.getParticipants()) {
                if (participant == participantId) {
                    results.push_back(appointment);
                    break;
                }
            }
        }
        return results;
    }

    // Add user
    bool addUser(const User& user) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        users.push_back(user);
        return true;
    }

    // Remove user
    bool removeUser(const std::string& userId) {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        for (size_t i = 0; i < users.size(); ++i) {
            if (users[i].getId() == userId) {
                users.erase(users.begin() + i);
                return true;
            }
        }
        return false;
    }

    // Get all appointments (for testing purposes)
    std::vector<Appointment> getAllAppointments() {
        std::lock_guard<std::mutex> lock(appointmentsMutex);
        return appointments;
    }
};


void testAppointmentSystem() {
    AppointmentSystem system;

    // Clear existing appointments
    std::vector<Appointment> allAppointments = system.getAllAppointments();
    for (const auto& appointment : allAppointments) {
        system.removeAppointment(appointment.getId());
    }

    // Test Data
    User doctor1("doc1", "Dr. Smith", "doctor");
    User patient1("pat1", "John Doe", "patient");
    User patient2("pat2", "Jane Doe", "patient");

    system.addUser(doctor1);
    system.addUser(patient1);
    system.addUser(patient2);


    Appointment app1("appt1", "2025-03-15", "10:00", { "doc1", "pat1" });
    Appointment app2("appt2", "2025-03-15", "11:00", { "doc1", "pat2" });
    Appointment app3("appt3", "2025-03-16", "14:00", { "doc1", "pat1" });

    // Test Add Appointment
    assert(system.addAppointment(app1));
    assert(system.addAppointment(app2));
    assert(system.addAppointment(app3));
    assert(!system.addAppointment(app1)); // Should fail due to time slot conflict


    // Test List Appointments
    std::vector<Appointment> appointmentsOnDate = system.listAppointments("2025-03-15", "10:00");
    assert(appointmentsOnDate.size() == 1);
    assert(appointmentsOnDate[0].getId() == "appt1");

    // Test Modify Appointment
    Appointment modifiedApp = app1;
    modifiedApp.setTime("10:30");
    assert(system.modifyAppointment("appt1", modifiedApp));

    appointmentsOnDate = system.listAppointments("2025-03-15", "10:30");
    assert(appointmentsOnDate.size() == 1);
    assert(appointmentsOnDate[0].getId() == "appt1");

    // Test Search by Participant
    std::vector<Appointment> appointmentsWithParticipant = system.searchAppointmentsByParticipant("pat1");
    assert(appointmentsWithParticipant.size() == 2);

    // Test Remove Appointment
    assert(system.removeAppointment("appt2"));
    appointmentsWithParticipant = system.searchAppointmentsByParticipant("pat2");
    assert(appointmentsWithParticipant.empty());

    // Test Invalid Date
    Appointment invalidApp("appt4", "2023-13-01", "10:00", { "doc1", "pat1" });
    assert(!system.addAppointment(invalidApp));

    // Test Past Date
    Appointment pastApp("appt5", "2023-01-01", "10:00", { "doc1", "pat1" });
    assert(!system.addAppointment(pastApp));
}

void testConcurrency() {
    AppointmentSystem system;

    // Clear existing appointments
    std::vector<Appointment> allAppointments = system.getAllAppointments();
    for (const auto& appointment : allAppointments) {
        system.removeAppointment(appointment.getId());
    }

    // Prepare test users
    User doctor1("doc1", "Dr. Concurrency", "doctor");
    User patient1("pat1", "Concurrent Patient 1", "patient");
    User patient2("pat2", "Concurrent Patient 2", "patient");

    system.addUser(doctor1);
    system.addUser(patient1);
    system.addUser(patient2);

    // Define a function for concurrent appointment creation
    auto createAppointment = [&](const std::string& appId, const std::string& time) {
        Appointment app(appId, "2025-03-20", time, { "doc1", "pat1" });
        assert(system.addAppointment(app));
    };

    // Create multiple threads to concurrently add appointments
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(createAppointment, "appt_conc_" + std::to_string(i), "1" + std::to_string(i) + ":00");
    }

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify that all appointments were added
    std::vector<Appointment> allAppointmentsAfterConcurrency = system.getAllAppointments();
    assert(allAppointmentsAfterConcurrency.size() == 5);

    // Clean up appointments created during the concurrency test
    for (const auto& appointment : allAppointmentsAfterConcurrency) {
        system.removeAppointment(appointment.getId());
    }
}


int main() {
    // Run unit tests
    std::cout << "Running unit tests..." << std::endl;
    testAppointmentSystem();
    std::cout << "All unit tests passed!" << std::endl;

    std::cout << "Running concurrency tests..." << std::endl;
    testConcurrency();
    std::cout << "All concurrency tests passed!" << std::endl;

    std::cout << "Appointment System Tests Completed." << std::endl;

    return 0;
}