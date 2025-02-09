#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <thread>
#include <random>
#include <unordered_map>
#include <shared_mutex>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Helper function to generate a random string of specified length
std::string generateRandomString(size_t length) {
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device random_device;
    std::mt19937 generator(random_device());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string random_string;
    for (size_t i = 0; i < length; ++i) {
        random_string += characters[distribution(generator)];
    }

    return random_string;
}


class User {
private:
    std::string id;
    std::string name;
    std::string role; // "patient" or "doctor"

public:
    User() : id(generateRandomString(10)), name(""), role("") {}
    User(const std::string& id, const std::string& name, const std::string& role) : id(id), name(name), role(role) {}

    std::string getId() const { return id; }
    std::string getName() const { return name; }
    std::string getRole() const { return role; }

    void setId(const std::string& id) { this->id = id; }
    void setName(const std::string& name) { this->name = name; }
    void setRole(const std::string& role) { this->role = role; }

    // JSON serialization/deserialization
    json toJson() const {
        return {
            {"id", id},
            {"name", name},
            {"role", role}
        };
    }

    static User fromJson(const json& j) {
        return User(j["id"], j["name"], j["role"]);
    }

    //Overload == operator for comparison
    bool operator==(const User& other) const {
        return (id == other.id && name == other.name && role == other.role);
    }
};

class Appointment {
private:
    std::string id;
    std::string date; // YYYY-MM-DD
    std::string time; // HH:MM
    std::vector<std::string> participants; // User IDs

public:
    Appointment() : id(generateRandomString(10)), date(""), time(""), participants({}) {}
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

    // JSON serialization/deserialization
    json toJson() const {
        return {
            {"id", id},
            {"date", date},
            {"time", time},
            {"participants", participants}
        };
    }

    static Appointment fromJson(const json& j) {
        return Appointment(j["id"], j["date"], j["time"], j["participants"].get<std::vector<std::string>>());
    }

    //Overload == operator for comparison
    bool operator==(const Appointment& other) const {
        return (id == other.id && date == other.date && time == other.time && participants == other.participants);
    }
};

class AppointmentSystem {
private:
    std::string appointmentsFile = "appointments.json";
    std::string usersFile = "users.json";

    // Use unordered_map for efficient lookups
    std::unordered_map<std::string, Appointment> appointments;
    std::unordered_map<std::string, User> users;

    // Use shared_mutex for concurrent read/write access
    std::shared_mutex appointmentsMutex;
    std::shared_mutex usersMutex;

    // Helper function to load data from JSON file
    template <typename T>
    void loadData(const std::string& filename, std::unordered_map<std::string, T>& data, std::shared_mutex& mutex) {
        std::ifstream file(filename);
        if (file.is_open()) {
            try {
                json j;
                file >> j;
                std::unique_lock<std::shared_mutex> lock(mutex); // Exclusive write lock
                for (auto& item : j.items()) {
                    data[item.key()] = T::fromJson(item.value());
                }
            }
            catch (const json::parse_error& e) {
                std::cerr << "Error parsing JSON file: " << filename << " - " << e.what() << std::endl;
            }
        }
        else {
            std::cerr << "Error opening file: " << filename << std::endl;
        }
    }

    // Helper function to save data to JSON file
    template <typename T>
    void saveData(const std::string& filename, const std::unordered_map<std::string, T>& data, std::shared_mutex& mutex) {
        json j;
        std::shared_lock<std::shared_mutex> lock(mutex); // Shared read lock
        for (const auto& item : data) {
            j[item.first] = item.second.toJson();
        }

        std::ofstream file(filename);
        if (file.is_open()) {
            file << std::setw(4) << j << std::endl;
        }
        else {
            std::cerr << "Error opening file for writing: " << filename << std::endl;
        }
    }

    bool isValidAppointmentTime(const std::string& time) {
        try {
            int hour = std::stoi(time.substr(0, 2));
            int minute = std::stoi(time.substr(3, 2));

            if (hour < 9 || hour > 19) return false; // 9:00am to 8:00pm (20:00)
            if (minute != 0 && minute != 30) return false; // Only allow on the hour or half hour
            if (hour == 19 && minute == 30) return false; // Last appointment is at 8:00pm

            return true;
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Invalid time format: " << time << std::endl;
            return false;
        }
        catch (const std::out_of_range& e) {
            std::cerr << "Time out of range: " << time << std::endl;
            return false;
        }
    }

    bool isValidAppointmentDate(const std::string& date) {
        std::time_t t = std::time(nullptr);
        std::tm now = *std::localtime(&t);

        std::tm input_time = {};
        std::istringstream ss(date);
        ss >> std::get_time(&input_time, "%Y-%m-%d");
        if (ss.fail()) {
            return false;
        }
        input_time.tm_isdst = -1; // Important: Allow the mktime() function to determine if DST is in effect

        std::time_t input_time_t = std::mktime(&input_time);
        std::time_t now_time_t = std::mktime(&now);

        if (input_time_t < now_time_t) {
            return false;
        }

        return true;
    }


public:
    AppointmentSystem() {
        loadData(appointmentsFile, appointments, appointmentsMutex);
        loadData(usersFile, users, usersMutex);
    }

    ~AppointmentSystem() {
        saveData(appointmentsFile, appointments, appointmentsMutex);
        saveData(usersFile, users, usersMutex);
    }

    // User management
    bool addUser(const User& user) {
        std::unique_lock<std::shared_mutex> lock(usersMutex);
        if (users.find(user.getId()) != users.end()) {
            std::cerr << "User with ID " << user.getId() << " already exists." << std::endl;
            return false;
        }
        users[user.getId()] = user;
        return true;
    }

    bool removeUser(const std::string& userId) {
        std::unique_lock<std::shared_mutex> lock(usersMutex);
        if (users.find(userId) == users.end()) {
            std::cerr << "User with ID " << userId << " not found." << std::endl;
            return false;
        }
        users.erase(userId);
        return true;
    }

    User getUser(const std::string& userId) {
        std::shared_lock<std::shared_mutex> lock(usersMutex);
        auto it = users.find(userId);
        if (it != users.end()) {
            return it->second;
        }
        else {
            throw std::runtime_error("User not found");
        }
    }


    // Adds an appointment
    bool addAppointment(const Appointment& appointment) {
        if (!isValidAppointmentTime(appointment.getTime())) {
            std::cerr << "Invalid appointment time: " << appointment.getTime() << std::endl;
            return false;
        }

        if (!isValidAppointmentDate(appointment.getDate())) {
            std::cerr << "Invalid appointment date: " << appointment.getDate() << std::endl;
            return false;
        }
        std::unique_lock<std::shared_mutex> lock(appointmentsMutex); // Acquire exclusive write lock
        if (appointments.find(appointment.getId()) != appointments.end()) {
            std::cerr << "Appointment with ID " << appointment.getId() << " already exists." << std::endl;
            return false;
        }
        appointments[appointment.getId()] = appointment;
        return true;
    }

    // Modifies appointment
    bool modifyAppointment(const std::string& appointmentId, const Appointment& newAppointment) {
        if (!isValidAppointmentTime(newAppointment.getTime())) {
            std::cerr << "Invalid appointment time: " << newAppointment.getTime() << std::endl;
            return false;
        }

        if (!isValidAppointmentDate(newAppointment.getDate())) {
            std::cerr << "Invalid appointment date: " << newAppointment.getDate() << std::endl;
            return false;
        }
        std::unique_lock<std::shared_mutex> lock(appointmentsMutex); // Acquire exclusive write lock
        if (appointments.find(appointmentId) == appointments.end()) {
            std::cerr << "Appointment with ID " << appointmentId << " not found." << std::endl;
            return false;
        }
        appointments[appointmentId] = newAppointment;
        return true;
    }

    // Removes appointment
    bool removeAppointment(const std::string& appointmentId) {
        std::unique_lock<std::shared_mutex> lock(appointmentsMutex); // Acquire exclusive write lock
        if (appointments.find(appointmentId) == appointments.end()) {
            std::cerr << "Appointment with ID " << appointmentId << " not found." << std::endl;
            return false;
        }
        appointments.erase(appointmentId);
        return true;
    }

    // List appointments
    std::vector<Appointment> listAppointments(const std::string& date, const std::string& time) {
        std::vector<Appointment> result;
        std::shared_lock<std::shared_mutex> lock(appointmentsMutex); // Acquire shared read lock
        for (const auto& pair : appointments) {
            const Appointment& appointment = pair.second;
            if ((date.empty() || appointment.getDate() == date) && (time.empty() || appointment.getTime() == time)) {
                result.push_back(appointment);
            }
        }

        // Sort by date and time
        std::sort(result.begin(), result.end(), [](const Appointment& a, const Appointment& b) {
            if (a.getDate() != b.getDate()) {
                return a.getDate() < b.getDate();
            }
            return a.getTime() < b.getTime();
        });

        return result;
    }

    // Search appointments by participant
    std::vector<Appointment> searchAppointmentsByParticipant(const std::string& participantId) {
        std::vector<Appointment> result;
        std::shared_lock<std::shared_mutex> lock(appointmentsMutex);
        for (const auto& pair : appointments) {
            const Appointment& appointment = pair.second;
            if (std::find(appointment.getParticipants().begin(), appointment.getParticipants().end(), participantId) != appointment.getParticipants().end()) {
                result.push_back(appointment);
            }
        }
        return result;
    }
};


void testAppointmentSystem() {
    AppointmentSystem system;

    // Create test users
    User doctor1("doctor1", "Dr. Smith", "doctor");
    User patient1("patient1", "John Doe", "patient");
    User patient2("patient2", "Jane Doe", "patient");

    assert(system.addUser(doctor1));
    assert(system.addUser(patient1));
    assert(system.addUser(patient2));

    // Test addAppointment
    Appointment appointment1("appt1", "2025-02-10", "10:00", { "doctor1", "patient1" });
    assert(system.addAppointment(appointment1));

    Appointment appointment2("appt2", "2025-02-10", "10:30", { "doctor1", "patient2" });
    assert(system.addAppointment(appointment2));

    Appointment appointment3("appt3", "2025-02-11", "09:00", { "doctor1", "patient1" });
    assert(system.addAppointment(appointment3));

    // Test listAppointments
    std::vector<Appointment> appointmentsOnDate = system.listAppointments("2025-02-10", "");
    assert(appointmentsOnDate.size() == 2);
    assert(appointmentsOnDate[0].getId() == "appt1");
    assert(appointmentsOnDate[1].getId() == "appt2");

    // Test modifyAppointment
    Appointment modifiedAppointment = appointment1;
    modifiedAppointment.setTime("11:00");
    assert(system.modifyAppointment("appt1", modifiedAppointment));
    std::vector<Appointment> appointmentsAfterModification = system.listAppointments("2025-02-10", "11:00");
    assert(appointmentsAfterModification.size() == 1);
    assert(appointmentsAfterModification[0].getId() == "appt1");

    // Test searchAppointmentsByParticipant
    std::vector<Appointment> appointmentsWithPatient1 = system.searchAppointmentsByParticipant("patient1");
    assert(appointmentsWithPatient1.size() == 2); // appt1 (modified) and appt3

    // Test removeAppointment
    assert(system.removeAppointment("appt2"));
    std::vector<Appointment> appointmentsAfterRemoval = system.listAppointments("2025-02-10", "");
    assert(appointmentsAfterRemoval.size() == 1); // Only appt1 should remain

    assert(system.removeUser("doctor1"));
    assert(system.removeUser("patient1"));
    assert(system.removeUser("patient2"));

    std::cout << "All tests passed!" << std::endl;
}


void generateTestData(const std::string& appointmentsFile, const std::string& usersFile) {
    json appointmentsJson;
    json usersJson;

    // Generate a large number of appointments
    for (int i = 0; i < 100; ++i) {
        std::string appointmentId = "appt" + std::to_string(i);
        int day = 10 + (i % 5); // Dates between 2025-02-10 and 2025-02-14
        int hour = 9 + (i % 11); // Times between 9:00 and 19:00
        int minute = (i % 2) * 30; // Either 00 or 30 minutes
        std::string date = "2025-02-" + std::to_string(day);
        std::string time = std::to_string(hour) + ":" + (minute == 0 ? "00" : "30");

        appointmentsJson[appointmentId] = {
            {"id", appointmentId},
            {"date", date},
            {"time", time},
            {"participants", {"doctor1", "patient" + std::to_string(i % 10)}} // 10 different patients
        };
    }

    // Generate some users (doctors and patients)
    usersJson["doctor1"] = {{"id", "doctor1"}, {"name", "Dr. Smith"}, {"role", "doctor"}};
    for (int i = 0; i < 10; ++i) {
        usersJson["patient" + std::to_string(i)] = {
            {"id", "patient" + std::to_string(i)},
            {"name", "Patient " + std::to_string(i)},
            {"role", "patient"}
        };
    }

    // Write to files
    std::ofstream appointmentsFileStream(appointmentsFile);
    if (appointmentsFileStream.is_open()) {
        appointmentsFileStream << std::setw(4) << appointmentsJson << std::endl;
    }
    else {
        std::cerr << "Error opening file for writing: " << appointmentsFile << std::endl;
    }

    std::ofstream usersFileStream(usersFile);
    if (usersFileStream.is_open()) {
        usersFileStream << std::setw(4) << usersJson << std::endl;
    }
    else {
        std::cerr << "Error opening file for writing: " << usersFile << std::endl;
    }
}

void concurrentAccessTest() {
    AppointmentSystem system;

    // Add some initial data
    User doctor1("doctor1", "Dr. Smith", "doctor");
    User patient1("patient1", "John Doe", "patient");
    system.addUser(doctor1);
    system.addUser(patient1);

    Appointment appointment1("appt1", "2025-02-15", "10:00", { "doctor1", "patient1" });
    system.addAppointment(appointment1);

    // Number of threads and iterations
    const int numThreads = 5;
    const int numIterations = 100;

    // Lambda function for concurrent access
    auto concurrentTask = [&](int threadId) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 9);

        for (int i = 0; i < numIterations; ++i) {
            try {
                // Randomly perform operations
                int operation = distrib(gen) % 4;

                if (operation == 0) {
                    // Add a new appointment
                    Appointment newAppointment("appt_thread" + std::to_string(threadId) + "_" + std::to_string(i),
                        "2025-02-16", "11:00", { "doctor1", "patient1" });
                    system.addAppointment(newAppointment);
                }
                else if (operation == 1) {
                    // Modify an existing appointment
                    Appointment modifiedAppointment = appointment1;
                    modifiedAppointment.setTime("12:00");
                    system.modifyAppointment("appt1", modifiedAppointment);
                }
                else if (operation == 2) {
                    // List appointments
                    system.listAppointments("2025-02-15", "");
                }
                else {
                    // Search appointments by participant
                    system.searchAppointmentsByParticipant("patient1");
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Exception in thread " << threadId << ": " << e.what() << std::endl;
            }
        }
    };

    // Create and launch threads
    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(concurrentTask, i);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Clean up
    system.removeUser("doctor1");
    system.removeUser("patient1");
    system.removeAppointment("appt1");

    std::cout << "Concurrent access test completed." << std::endl;
}


int main() {
    // Generate test data
    generateTestData("appointments.json", "users.json");

    // Run unit tests
    testAppointmentSystem();

    // Run concurrent access test
    concurrentAccessTest();

    return 0;
}