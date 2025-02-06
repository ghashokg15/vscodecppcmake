#pragma once

#include <iostream>
#include <vector>
#include <algorithm> // For sorting (optional, but can improve efficiency in some cases)
#include <atomic>

using namespace std;

vector<pair<int, int>> findPairs(const vector<int> &arr, int targetSum);

template <typename T>
void find_combinations_with_target_sum(vector<T>& in, vector<vector<T>>& out, vector<T>& current, T target_sum, int index);


