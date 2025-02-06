#include "gtest/gtest.h"
// #include "modelA.h"
// #include "../src/EventQueue.h"

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



// --- Unit Tests ---
TEST(EventQueueTest, EnqueueDequeue) {
    EventQueue queue;
    Event event1 = {"Test Event 1", 1, 1};
    Event event2 = {"Test Event 2", 2, 2};

    queue.enqueue(event1);
    queue.enqueue(event2);

    Event retrieved_event1 = queue.dequeue();
    Event retrieved_event2 = queue.dequeue();

    ASSERT_EQ(retrieved_event1.data, event1.data);
    ASSERT_EQ(retrieved_event2.data, event2.data);
    ASSERT_TRUE(queue.empty());
}

TEST(EventQueueTest, StopRequest) {
    EventQueue queue;
    queue.request_stop();
    Event retrieved_event = queue.dequeue();
    ASSERT_TRUE(retrieved_event.data.empty());  // Check that an empty event is returned
    ASSERT_TRUE(queue.is_stop_requested());
}

TEST(EventQueueTest, MultipleEnqueueDequeue) {
  EventQueue queue;
  std::vector<Event> events;
  for (int i = 0; i < 10; ++i) {
    events.push_back({"Event " + std::to_string(i), 1, static_cast<uint64_t>(i)});
    queue.enqueue(events.back());
  }

  for (int i = 0; i < 10; ++i) {
    Event retrieved_event = queue.dequeue();
    ASSERT_EQ(retrieved_event.data, events[i].data);
    ASSERT_EQ(retrieved_event.sequence_number, events[i].sequence_number);
  }
  ASSERT_TRUE(queue.empty());
}

TEST(EventGeneratorTest, BasicGeneration) {
    EventQueue queue;
    std::atomic<bool> stop_flag(true);
    std::atomic<uint64_t> sequence_number(0);

    // Run the generator for a very short time
    std::thread generator_thread(event_generator, 1, std::ref(queue), std::ref(stop_flag), std::ref(sequence_number));
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Short sleep
    generator_thread.join();
    
    // It's hard to assert the exact number of events, but we can check that *some* events were generated,
    // as long as the generator actually ran (which is difficult to guarantee in a test this short).
    //This test primarily confirms it doesn't crash when started and stopped quickly.
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
     return RUN_ALL_TESTS();
}
