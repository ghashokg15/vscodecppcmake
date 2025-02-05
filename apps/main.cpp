#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <random>
#include <algorithm>
#include <queue>

class Event {
public:
    std::string data;
    int origin_thread_id; // ID of the thread that originally created the event
    Event(std::string data, int origin_thread_id) : data(data), origin_thread_id(origin_thread_id) {}
};

class ThreadSafeQueue {
private:
    std::queue<Event> q;
    std::mutex mtx;
    std::condition_variable cv;
public:
    void enqueue(Event event) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(event);
        }
        cv.notify_one();
    }
    Event dequeue() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this] { return !q.empty(); });
        Event event = q.front();
        q.pop();
        return event;
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return q.empty();
    }
};

class Worker {
public:
    int id;
    std::vector<Worker*> others; // Pointers to other workers
    ThreadSafeQueue incoming_events;
    std::mutex mtx; // Mutex for thread-safe operations
    bool done = false;
    std::condition_variable cv;
    Worker(int id) : id(id) {}
    void set_others(const std::vector<Worker*>& others) {
        this->others = others;
    }
    void process_events() {
        while (!done) {
            Event event;
            try {
                event = incoming_events.dequeue();
            } catch (const std::exception& e) {
                std::cerr << "Thread " << id << " error dequeuing: " << e.what() << std::endl;
                break;
            }
            std::cout << "Thread " << id << " received event: " << event.data << " (Origin: " << event.origin_thread_id << ")" << std::endl;
            // Forward to other threads (excluding the origin)
            for (Worker* other : others) {
                if (other->id != event.origin_thread_id && other->id != id) {
                    other->incoming_events.enqueue(event);
                }
            }
        }
        std::cout << "Thread " << id << " exiting." << std::endl;
    }
    void stop() {
        std::unique_lock<std::mutex> lock(mtx);
        done = true;
        cv.notify_all();
    }
};

int main() {
    const int num_threads = 3;
    std::vector<Worker> workers;
    std::vector<std::thread> threads;
    // Initialize workers
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(i);
    }
    // Create pointers to workers for easy access
    std::vector<Worker*> worker_ptrs;
    for (auto& worker : workers) {
        worker_ptrs.push_back(&worker);
    }
   //Set Others
    for (auto& worker : workers) {
        std::vector<Worker*> temp_worker_ptrs;
        for (auto& other_worker : workers) {
            if (other_worker.id != worker.id) {
              temp_worker_ptrs.push_back(&other_worker);
            }
        }
        worker.set_others(temp_worker_ptrs);
    }
    // Launch worker threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&workers, i]() { workers[i].process_events(); });
    }
    // Event generation thread (in main)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1, 5); // Interval between 1 and 5 seconds
    for (int i = 0; i < 5; ++i) { // Generate 5 events
        int random_thread_id = i % num_threads;
        std::string event_data = "Event " + std::to_string(i) + " from thread " + std::to_string(random_thread_id);
        Event event(event_data, random_thread_id);
        workers[random_thread_id].others[0]->incoming_events.enqueue(event); // Send to first other
        int sleep_duration = distrib(gen);
        std::cout << "Main thread sleeping for " << sleep_duration << " seconds." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(sleep_duration));
    }
    // Signal threads to stop and join them
    for (auto& worker : workers) {
        worker.stop();
    }
    for (auto& thread : threads) {
        thread.join();
    }
    std::cout << "Main thread exiting." << std::endl;
    return 0;
}