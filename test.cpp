#include "junctiond.h"  // your JunctionD class
#include <fstream>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include "junctiond.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    JunctionD jd;

    FunctionData func;
    func.name = "testfunc";
    func.rootfs = "/rootfs";      // adjust this to your rootfs location
    func.cpu = 1;
    func.memoryMB = 128;

    // 1. Spawn Junction function
    bool ok = jd.spawn(func);
    assert(ok && "Failed to spawn Junction instance");

    std::cout << "Spawned function: " << func.name << std::endl;

    // 2. Let it start up
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 3. List running functions
    auto instances = jd.list();
    std::cout << "Running instances:\n";
    for (auto &f : instances) {
        std::cout << " - " << f.name 
                  << " PID: " << f.pid
                  << " Running: " << (f.running ? "true" : "false")
                  << std::endl;
    }

    // 4. Remove function
    bool removed = jd.remove(func.name);
    assert(removed && "Failed to remove function");

    std::cout << "Removed function: " << func.name << std::endl;

    return 0;
}
