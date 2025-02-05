#include <iostream>
#include "modelA.h"
int main(int, char **)
{
    vector<int> numbers = {1, 2, 3, 4, 5, 6};
    int target = 7;
    vector<pair<int, int>> pairs = findPairs(numbers, target);
    cout << "Pairs that sum to " << target << ":" << endl;
    for (const auto &pair : pairs)
    {
        cout << "(" << pair.first << ", " << pair.second << ")" << endl;
    }
    return 0;
}
