#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main() {
    std::atomic<bool> running{true};

    auto work_loop = [&running]() {
        while (running) {
            std::cout << "background thread working\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
        std::cout << "background thread exit\n";
    };

    std::thread bg_thread(work_loop);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "notify background thread to exit\n";
    running = false;

    bg_thread.join();
    std::cout << "joined, main exit\n";
    return 0;
}
