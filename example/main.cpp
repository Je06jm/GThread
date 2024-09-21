#include <gthread.hpp>

#include <iostream>
#include <vector>
#include <cstdlib>

std::list<int> sort_helper(std::list<int> lhs, std::list<int> rhs) {
    if (lhs.size() == 0)
        return rhs;
    
    if (rhs.size() == 0)
        return lhs;

    gthread::future<std::list<int>> f_lhs, f_rhs;

    auto split = [](const std::list<int>& value, std::list<int>& lower, std::list<int>& upper) {
        auto half = value.size() / 2;
        auto it = value.begin();
        auto end = value.end();
        for (size_t i = 0; it != end; i++, it++) {
            if (i < half)
                lower.push_back(*it);
            
            else
                upper.push_back(*it);
        }
    };

    {
        std::list<int> lower, upper;
        split(lhs, lower, upper);
        f_lhs = gthread::execute(sort_helper, lower, upper);
    }

    {
        std::list<int> lower, upper;
        split(rhs, lower, upper);
        f_rhs = gthread::execute(sort_helper, lower, upper);
    }

    auto l_lhs = f_lhs.get();
    auto l_rhs = f_rhs.get();

    std::list<int> sorted;

    while (l_lhs.size() && l_rhs.size()) {
        if (l_lhs.front() < l_rhs.front()) {
            sorted.push_back(l_lhs.front());
            l_lhs.pop_front();
        }
        else {
            sorted.push_back(l_rhs.front());
            l_rhs.pop_front();
        }
    }

    while (l_lhs.size()) {
        sorted.push_back(l_lhs.front());
        l_lhs.pop_front();
    }

    while (l_rhs.size()) {
        sorted.push_back(l_rhs.front());
        l_rhs.pop_front();
    }

    return sorted;
}

std::list<int> sort(std::list<int> unsorted) {
    auto half = unsorted.size() / 2;
    std::list<int> lower, upper;
    auto it = unsorted.begin();
    auto end = unsorted.end();
    for (size_t i = 0; it != end; i++, it++) {
        if (i < half) {
            lower.push_back(*it);
        }
        else {
            upper.push_back(*it);
        }
    }
    return sort_helper(lower, upper);
}

std::vector<int> it(int begin, int end) {
    std::vector<int> a;
    a.reserve(end-begin);
    for (int i = begin; i < end; i++)
        a.push_back(i);
    return a;
}

std::thread::id get_id() {
    return std::this_thread::get_id();
}

int main() {
    auto i = gthread::execute(it, 0, 10);
    
    for (auto v : i.get()) {
        std::cout << v << " ";
    }

    std::cout << std::endl;

    std::list<gthread::future<std::thread::id>> ids;

    for (int i = 0; i < 10; i++)
        ids.emplace_back(gthread::execute(get_id));
    
    for (auto& id : ids) {
        std::cout << std::hash<std::thread::id>()(id.get()) << std::endl;
    }

    srand(time(nullptr));

    std::list<int> v;
    for (int i = 0; i < 50; i++) {
        v.push_back(rand() % 100);
    }

    auto f_v = gthread::execute(sort, v);

    for (auto& value : f_v.get()) {
        std::cout << value << " ";
    }
    
    std::cout << std::endl;
}