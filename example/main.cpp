#include <gthread.hpp>

#include <iostream>
#include <vector>

std::vector<int> it(int begin, int end) {
    std::vector<int> a;
    a.reserve(end-begin);
    for (int i = begin; i < end; i++)
        a.push_back(i);
    return a;
}

std::thread::id get_id() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
}