#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <random>
#include <chrono>
#include <atomic>
#include <unordered_set>
#include <sstream>
#include <algorithm>

#include <cassert>  // For unit tests

// Forward declarations
class Event;
class EventGenerator;
class WorkerThread;

// Configuration
const int NUM_WORKER_THREADS = 5;
const int EVENT_SEND_DELAY_MS = 2000; // 2 seconds
const int EVENT_GENERATION_MIN_MS = 1;
const int EVENT_GENERATION_MAX_MS = 1000;

// Global atomic flag to signal shutdown
std::atomic<bool> g_shutdown(false);

// Thread-safe queue for events
class ThreadSafeQueue {
public:
    void enqueue(Event event) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(event);
        condition_.notify_one();
    }

    Event dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || g_shutdown; });
        if (queue_.empty() && g_shutdown) {
             // Return a special "shutdown" event.
            return Event(-1, "SHUTDOWN"); // Assuming -1 is an invalid ID
        }

        Event event = queue_.front();
        queue_.pop();
        return event;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<Event> queue_;
    std::mutex mutex_;
    std::condition_variable condition_;
};

// Event class
class Event {
public:
    int id;
    std::string data;

    Event(int id, const std::string& data) : id(id), data(data) {}

    // Overload the == operator for comparing events.  Crucial for testing!
    bool operator==(const Event& other) const {
        return (id == other.id) && (data == other.data);
    }
};

// Hash function for Event
namespace std {
    template <>
    struct hash<Event> {
        size_t operator()(const Event& event) const {
            size_t h1 = std::hash<int>{}(event.id);
            size_t h2 = std::hash<std::string>{}(event.data);
            return h1 ^ (h2 << 1);
        }
    };
}


// Event Generator
class EventGenerator {
public:
    EventGenerator(ThreadSafeQueue& queue) : queue_(queue), event_id_(0) {}

    void generateEvents() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(EVENT_GENERATION_MIN_MS, EVENT_GENERATION_MAX_MS);

        while (!g_shutdown) {
            int delay = distrib(gen);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));

            if (g_shutdown) break;

            Event event(event_id_++, "Event data " + std::to_string(event_id_));
            queue_.enqueue(event);
            std::cout << "Generated event " << event.id << std::endl;
        }

        std::cout << "Event generator stopped." << std::endl;
    }

private:
    ThreadSafeQueue& queue_;
    int event_id_;
};


// Worker Thread
class WorkerThread {
public:
    WorkerThread(int id, ThreadSafeQueue& queue, std::vector<WorkerThread*>& others)
        : id_(id), queue_(queue), others_(others) {}

    void processEvents() {
        std::unordered_set<Event> received_events; // Deduplication

        while (!g_shutdown) {
            Event event = queue_.dequeue();

            if (event.id == -1 && event.data == "SHUTDOWN") { //Special shutdown event
                break;
            }

            if (received_events.find(event) == received_events.end()) {
                received_events.insert(event);
                std::cout << "Thread " << id_ << " received event " << event.id << std::endl;

                // Resend to other threads (with a delay)
                std::thread([this, event]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(EVENT_SEND_DELAY_MS));
                    if (!g_shutdown) {
                        for (WorkerThread* other : others_) {
                            if (other != this) {
                                other->queue_.enqueue(event);
                            }
                        }
                    }
                }).detach(); // Detach so it doesn't block
            } else {
                std::cout << "Thread " << id_ << " ignoring duplicate event " << event.id << std::endl;
            }
        }
        std::cout << "Thread " << id_ << " stopped." << std::endl;
    }

private:
    int id_;
    ThreadSafeQueue& queue_;
    std::vector<WorkerThread*>& others_;
};

// Unit Tests (using assert for simplicity)
void testThreadSafeQueue() {
    ThreadSafeQueue queue;
    Event event1(1, "Test event 1");
    Event event2(2, "Test event 2");

    queue.enqueue(event1);
    queue.enqueue(event2);

    Event dequeued_event1 = queue.dequeue();
    Event dequeued_event2 = queue.dequeue();

    assert(dequeued_event1 == event1);
    assert(dequeued_event2 == event2);
    assert(queue.empty());

    std::cout << "ThreadSafeQueue tests passed" << std::endl;
}

void testEventEquality() {
    Event event1(1, "Test event");
    Event event2(1, "Test event");
    Event event3(2, "Different event");

    assert(event1 == event2);
    assert(!(event1 == event3));

    std::cout << "Event equality tests passed" << std::endl;
}

int main() {
    // Run unit tests
    testThreadSafeQueue();
    testEventEquality();

    ThreadSafeQueue event_queue;
    EventGenerator generator(event_queue);

    std::vector<WorkerThread*> workers;
    std::vector<std::thread> worker_threads;

    // Create worker threads
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        workers.push_back(new WorkerThread(i, event_queue, workers));
    }

    // Now that all workers are created, update the 'others' vector for each.
    for (WorkerThread* worker : workers) {
        worker->others_ = workers; //Crucial to set the others vector correctly.
    }

    // Launch threads
    std::thread generator_thread(&EventGenerator::generateEvents, &generator);
    for (int i = 0; i < NUM_WORKER_THREADS; ++i) {
        worker_threads.emplace_back(&WorkerThread::processEvents, workers[i]);
    }

    // Wait for user input to shutdown
    std::cout << "Press Enter to shutdown..." << std::endl;
    std::cin.get();
    std::cout << "Shutting down..." << std::endl;

    // Signal shutdown
    g_shutdown = true;

    // Wake up all waiting threads by enqueueing shutdown events.
    for(int i = 0; i < NUM_WORKER_THREADS; ++i){
        event_queue.enqueue(Event(-1, "SHUTDOWN")); // Enqueue shutdown events
    }

    // Join threads
    generator_thread.join();
    for (auto& thread : worker_threads) {
        thread.join();
    }

    // Cleanup
    for (WorkerThread* worker : workers) {
        delete worker;
    }

    std::cout << "Shutdown complete." << std::endl;

    return 0;
}