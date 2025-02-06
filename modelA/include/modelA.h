#pragma once

#include <iostream>
#include <vector>
#include <algorithm> // For sorting (optional, but can improve efficiency in some cases)
#include <atomic>

using namespace std;

vector<pair<int, int>> findPairs(const vector<int> &arr, int targetSum);

struct Event;
class EventQueue;
void event_generator(int generator_id, EventQueue& queue, std::atomic<bool>& stop_flag, std::atomic<uint64_t>& sequence_number);
void event_processor(int thread_id, EventQueue& queue, const std::vector<EventQueue*>& other_queues, std::atomic<bool>& stop_flag);
