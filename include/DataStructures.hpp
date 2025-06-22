#pragma once
#include <map>
#include <unordered_map>
#include <list>

struct BidComparator {
    bool operator()(const double& lhs, const double& rhs) const {
        return lhs > rhs;
    }
};

struct AskComparator {
    bool operator()(const double& lhs, const double& rhs) const {
        return rhs > lhs;
    }
};

template<typename LevelType, typename Comparator>
using OneSideBook = std::map<double, LevelType, Comparator>;
