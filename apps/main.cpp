#include <iostream>
#include <vector>
#include <set>
using namespace std;

//backtracking solution to find combinations
template <typename T>
void find_combinations_with_target_sum(vector<T>& in, vector<vector<T>>& out, vector<T>& current, T target_sum, int index)
{
    if(target_sum == 0) {
        out.push_back(current);
        return;
    }

    if(target_sum < 0 || in.size() < index) {
        return;
    }

    for (int i = index; i < in.size(); i++) {
        if(index >= i && in[i] == in[i-1]) {
            continue;
        }
        current.push_back((in[i]));
        find_combinations_with_target_sum(in, out, current, target_sum - in[i], i + 1);
        current.pop_back();
    }
}

template <typename T>
void print_combinations(const vector<vector<T>> &in) {
    string h = "hello";
    for (auto& v : in) {
        cout << "[";
        auto len = v.size();
        for (int i = 0; i < len; i++) {
            cout << v[i];
            if(i < len - 1) {
                cout << ",";
            }
        }
        cout << "]" << endl;
    }
}

template <typename T>
void remove_duplicates_and_sort(vector<T>& v)
{
    set<T> s(v.begin(), v.end());
    v.assign(s.begin(), s.end());
}

int main() {
    vector<int> v = {3, 6, 9, 2, 5, 7, 1, 1, 2, 3, 5};
    vector<vector<int>> ans = {};
    vector<int> current;
    //Need to sort and remove duplicates the input before finding combinations to remove duplicates.
    remove_duplicates_and_sort(v);
    //sort(v.begin(), v.end(), [](int a, int b){return a < b;});
    find_combinations_with_target_sum(v, ans, current, 15, 0);
    print_combinations<int>(ans);
    return 0;    
}