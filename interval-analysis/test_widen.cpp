#include "interval_analysis.hpp"

int main() {
    interval_abstract_store s1 = {{"v1", std::make_pair(-1, -1)},
                                  {"v2", TOP}};
    interval_abstract_store s2 = {{"v1", std::make_pair(4, 4)},
                                  {"v3", std::make_pair(4, 64)}};
    if (widen(s1, s2)) {
        std::cout << "New value of s1:" << std::endl;
        print(s1);
    }
}