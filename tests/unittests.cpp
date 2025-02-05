#include "gtest/gtest.h"
#include "modelA.h"

// Test case 1: Basic test with a valid pair

TEST(FindPairsTest, BasicTest)
{
    vector<int> arr = {1, 2, 3, 4, 5};
    int target = 7;
    vector<pair<int, int>> expected = {{3, 4}, {2, 5}}; // Order may vary depending on implementation
    vector<pair<int, int>> actual = findPairs(arr, target);
    EXPECT_EQ(actual.size(), expected.size()); // Check correct number of pairs
    // Helper function to sort pairs for easy comparison

    auto sortPairs = [](vector<pair<int, int>> &pairs)
    {
        for (auto &p : pairs)
        {
            if (p.first > p.second)
            {
                swap(p.first, p.second);
            }
        }
        sort(pairs.begin(), pairs.end());
    };

    sortPairs(expected);
    sortPairs(actual);
    EXPECT_EQ(actual, expected);
}

// Test case 2: No pairs found

TEST(FindPairsTest, NoPairsTest)
{
    vector<int> arr = {1, 2, 3, 4, 5};
    int target = 100;
    vector<pair<int, int>> expected = {};
    vector<pair<int, int>> actual = findPairs(arr, target);
    EXPECT_EQ(actual.size(), 0); // OR EXPECT_EQ(actual, expected); if expected is an empty vector.
}

// Test case 3: Array with duplicate numbers

TEST(FindPairsTest, DuplicateNumbersTest)
{
    vector<int> arr = {1, 2, 2, 3, 4};
    int target = 4;
    vector<pair<int, int>> expected = {{2, 2}, {1, 3}}; // Order may vary
    vector<pair<int, int>> actual = findPairs(arr, target);
    EXPECT_EQ(actual.size(), expected.size());
    // Helper function to sort pairs for easy comparison

    auto sortPairs = [](vector<pair<int, int>> &pairs)
    {
        for (auto &p : pairs)
        {
            if (p.first > p.second)
            {
                swap(p.first, p.second);
            }
        }
        sort(pairs.begin(), pairs.end());
    };

    sortPairs(expected);
    sortPairs(actual);
    EXPECT_EQ(actual, expected);
}

// Test case 4: Empty array

TEST(FindPairsTest, EmptyArrayTest)
{
    vector<int> arr = {};
    int target = 5;
    vector<pair<int, int>> expected = {};
    vector<pair<int, int>> actual = findPairs(arr, target);
    EXPECT_EQ(actual.size(), 0); // OR EXPECT_EQ(actual, expected);
}

// Test case 5: Array with negative numbers

TEST(FindPairsTest, NegativeNumbersTest)
{
    vector<int> arr = {-1, 0, 1, 2, 3};
    int target = 1;
    vector<pair<int, int>> expected = {{-1, 2}, {0, 1}}; // Order may vary
    vector<pair<int, int>> actual = findPairs(arr, target);
    EXPECT_EQ(actual.size(), expected.size());
    // Helper function to sort pairs for easy comparison
    auto sortPairs = [](vector<pair<int, int>> &pairs)
    {
        for (auto &p : pairs)
        {
            if (p.first > p.second)
            {
                swap(p.first, p.second);
            }
        }
        sort(pairs.begin(), pairs.end());
    };

    sortPairs(expected);
    sortPairs(actual);
    EXPECT_EQ(actual, expected);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}