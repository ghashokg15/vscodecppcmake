#include "modelA.h"

// Function to find pairs that sum to the target
vector<pair<int, int>> findPairs(const vector<int> &arr, int targetSum)
{
    vector<pair<int, int>> result;
    int n = arr.size();
    // Method 1:  Brute-Force (Simple, but O(n^2))
    /*
    for (int i = 0; i < n; ++i) {
      for (int j = i + 1; j < n; ++j) {
        if (arr[i] + arr[j] == targetSum) {
          result.push_back({arr[i], arr[j]});
        }
      }
    }
    */

    // Method 2:  Using a Hash Table (unordered_map) - O(n) average
    unordered_map<int, int> numMap; // key: number, value: index (optional)
    for (int i = 0; i < n; ++i)
    {
        int complement = targetSum - arr[i];
        if (numMap.find(complement) != numMap.end())
        {
            result.push_back({arr[i], complement}); // Or {arr[i], arr[numMap[complement]]} if you stored indices
        }
        numMap[arr[i]] = i; // Store the number and its index (optional)
    }

    // Method 3: Two Pointers (Efficient if the array is sorted) - O(n)
    /*
    vector<int> sortedArr = arr; // Create a copy to sort
    sort(sortedArr.begin(), sortedArr.end());
    int left = 0;
    int right = n - 1;

    while (left < right) {
      int currentSum = sortedArr[left] + sortedArr[right];
      if (currentSum == targetSum) {
        result.push_back({sortedArr[left], sortedArr[right]});
        left++;
        right--; // You might want to skip duplicate pairs if needed
      } else if (currentSum < targetSum) {
        left++;
      } else {
        right--;
      }
    }
    */
    return result;
}