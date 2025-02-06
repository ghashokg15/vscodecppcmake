#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_set>
#include <sstream>
#include <queue>

using namespace std;
using namespace std::chrono;

// Configuration parameters
const int MIN_EVENT_INTERVAL_MS = 1;
const int MAX_EVENT_INTERVAL_MS = 1000;
const int RESEND_DELAY_MS = 2000;

// Structure to represent an event
struct Event {
    string data;
    int generator_id;
    uint64_t sequence_number;
};

// Thread-safe queue for passing events
class EventQueue {
public:
    void enqueue(const Event& event) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(event);
        cv_.notify_one();
    }

    Event dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stop_requested_.load(); });
        if (queue_.empty()) {
            return {}; // Return an "empty" event if stopped
        }
        Event event = queue_.front();
        queue_.pop();
        return event;
    }

    bool empty() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    void request_stop() {
        stop_requested_.store(true);
        cv_.notify_all(); // Wake up all waiting threads
    }

    bool is_stop_requested() const {
        return stop_requested_.load();
    }

private:
    std::queue<Event> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_requested_{false};
};

// Function to generate events
void event_generator(int generator_id, EventQueue& queue, std::atomic<bool>& stop_flag, std::atomic<uint64_t>& sequence_number) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(MIN_EVENT_INTERVAL_MS, MAX_EVENT_INTERVAL_MS);

    while (!stop_flag.load()) {
        int interval = distrib(gen);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));

        if (stop_flag.load()) break; // Check again after sleeping

        Event event;
        event.data = "Event from generator " + std::to_string(generator_id) + " seq " + std::to_string(sequence_number.load());
        event.generator_id = generator_id;
        event.sequence_number = sequence_number.fetch_add(1);

        queue.enqueue(event);
        std::cout << "Generator " << generator_id << " produced event: " << event.data << std::endl;
    }
    std::cout << "Generator " << generator_id << " stopped." << std::endl;
}

// Function for a thread in the network
void event_processor(int thread_id, EventQueue& queue, const std::vector<EventQueue*>& other_queues, std::atomic<bool>& stop_flag) {
    std::unordered_set<uint64_t> received_events;
    std::mutex received_events_mutex;

    auto is_duplicate = [&](uint64_t event_id) {
        std::lock_guard<std::mutex> lock(received_events_mutex);
        return received_events.count(event_id) > 0;
    };

    auto mark_received = [&](uint64_t event_id) {
        std::lock_guard<std::mutex> lock(received_events_mutex);
        received_events.insert(event_id);
    };

    while (!stop_flag.load()) {
        Event event = queue.dequeue();
        if (stop_flag.load() || event.data.empty()) {
            break; // Exit if stop requested or empty event received.
        }

        uint64_t event_id = ((uint64_t)event.generator_id << 32) | event.sequence_number;

        if (!is_duplicate(event_id)) {
            std::cout << "Thread " << thread_id << " received event: " << event.data << std::endl;
            mark_received(event_id);

            // Resend to other threads
            for (EventQueue* other_queue : other_queues) {
                if (other_queue != &queue) {
                    std::thread([other_queue, event, &stop_flag]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(RESEND_DELAY_MS));
                        if (!stop_flag.load()) {
                            other_queue->enqueue(event);
                        }
                    }).detach();
                }
            }
        } else {
            std::cout << "Thread " << thread_id << " received duplicate event, ignoring." << std::endl;
        }
    }

    std::cout << "Thread " << thread_id << " stopped." << std::endl;
}

int main() {
    int num_threads = 3;
    std::atomic<bool> stop_flag(false);
    std::atomic<uint64_t> sequence_number(0);

    std::vector<EventQueue> queues(num_threads);
    std::vector<std::thread> threads;
    std::vector<EventQueue*> queue_ptrs;
    for (auto& q : queues) {
        queue_ptrs.push_back(&q);
    }

    // Create and start the event generator
    EventQueue generator_queue;
    std::thread generator_thread(event_generator, 0, std::ref(generator_queue), std::ref(stop_flag), std::ref(sequence_number));
    threads.push_back(std::move(generator_thread));
    queue_ptrs.push_back(&generator_queue);

    // Create and start the threads in the network
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(event_processor, i + 1, std::ref(queues[i]), queue_ptrs, std::ref(stop_flag));
    }

    // Route events from generator to the threads
    std::thread generator_router([&]() {
        while (!stop_flag.load()) {
            Event event = generator_queue.dequeue();
            if (stop_flag.load() || event.data.empty()) break;
            for (auto& q : queues) {
                q.enqueue(event);
            }
        }
    });
    threads.push_back(std::move(generator_router));


    // Wait for user input to stop the threads
    std::cout << "Press Enter to stop the threads..." << std::endl;
    std::cin.get();

    stop_flag.store(true);
    generator_queue.request_stop();
    for (auto& queue : queues) {
        queue.request_stop();
    }
    

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads stopped." << std::endl;

    return 0;
}

