#include "junctiond.h"  // your JunctionD class
#include <fstream>
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

// Generate a minimal config file for a function
bool generateConfig(const std::string &name, const std::string &rootfs) {
    std::ofstream cfg(name + ".config");
    if (!cfg.is_open()) return false;

    cfg << "# Junction config for function: " << name << "\n";
    cfg << "host_addr 127.0.0.1\n";
    cfg << "host_netmask 255.255.255.0\n";
    cfg << "host_gateway 127.0.0.1\n";
    cfg << "runtime_kthreads 1\n";
    cfg << "runtime_spinning_kthreads 0\n";
    cfg << "runtime_guaranteed_kthreads 0\n";
    cfg << "runtime_priority lc\n";
    cfg << "runtime_quantum_us 0\n";
    cfg << "fs_config_path " << rootfs << "\n";

    cfg.close();
    return true;
}

int main() {
    JunctionD jd;

    FunctionData func;
    func.name = "testfunc";
    func.rootfs = "/rootfs"; // adjust to your rootfs path
    func.cpu = 1;
    func.memoryMB = 128;

    // 1. Generate config file
    assert(generateConfig(func.name, func.rootfs) && "Failed to generate config");

    // 2. Spawn function
    bool ok = jd.spawn(func);
    assert(ok && "Failed to spawn Junction instance");

    std::cout << "Spawned function: " << func.name << std::endl;

    // 3. Wait a bit to let the process start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 4. List running functions
    auto list = jd.list();
    std::cout << "Running instances:\n";
    for (auto &f : list) {
        std::cout << " - " << f.name << " PID: " << f.pid
                  << " Running: " << f.running << std::endl;
    }

    // 5. Remove function
    bool removed = jd.remove(func.name);
    assert(removed && "Failed to remove function");

    std::cout << "Removed function: " << func.name << std::endl;

    return 0;
}
