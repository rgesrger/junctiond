#include "junctiond.h"
#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <chrono>

JunctionD::JunctionD() {
    monitorThread = std::thread([this]() { monitorInstances(); });
    monitorThread.detach();
}

JunctionD::~JunctionD() {
    std::lock_guard<std::mutex> lock(mtx);
    for (auto &kv : statusMap) {
        remove(kv.first);
    }
}

bool JunctionD::spawn(const FunctionData &func) {
    std::string cfgFile;
    if (!generateConfig(func, cfgFile)) return false;

    const char* home = std::getenv("HOME");
    std::string junctionRun = std::string(home) + "/junction/build/junction/junction_run";

    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        execlp(junctionRun.c_str(), "junction_run", cfgFile.c_str(), nullptr);
        std::cerr << "[junctiond] Exec failed!" << std::endl;
        exit(1);
    }

    std::lock_guard<std::mutex> lock(mtx);
    statusMap[func.name] = {func.name, true, pid};
    std::cout << "[junctiond] Spawned " << func.name << " PID " << pid << std::endl;
    return true;
}

bool JunctionD::remove(const std::string &name) {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = statusMap.find(name);
    if (it == statusMap.end()) return false;

    FunctionStatus &status = it->second;
    if (status.running) {
        kill(status.pid, SIGTERM);
        waitpid(status.pid, nullptr, 0);
        status.running = false;
    }

    std::string cfgPath = "/tmp/junction_" + name + ".config";
    unlink(cfgPath.c_str());

    statusMap.erase(it);
    return true;
}

std::vector<FunctionStatus> JunctionD::list() {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<FunctionStatus> functions;
    for (auto &kv : statusMap) {
        functions.push_back(kv.second);
    }
    return functions;
}

void JunctionD::monitorInstances() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::lock_guard<std::mutex> lock(mtx);

        for (auto it = statusMap.begin(); it != statusMap.end(); ) {
            int status;
            pid_t ret = waitpid(it->second.pid, &status, WNOHANG);
            if (ret > 0) {
                std::cout << "[junctiond] Instance " << it->second.name << " terminated." << std::endl;
                it = statusMap.erase(it);
            } else {
                ++it;
            }
        }
    }
}
bool JunctionD::generateConfig(const FunctionData &func, std::string &cfgPath) {
    const char* home = std::getenv("HOME");
    std::string baseDir = std::string(home) + "/.junction";

    std::string workspaceDir = baseDir + "/" + func.name;
    system(("mkdir -p " + workspaceDir).c_str());

    cfgPath = workspaceDir + "/" + func.name + ".config";

    std::ofstream cfg(cfgPath);
    if (!cfg.is_open())
        return false;

    // valid Caladan IPs
    cfg << "host_addr 18.0.0.2\n";
    cfg << "host_netmask 255.255.255.0\n";
    cfg << "host_gateway 18.0.0.1\n";

    cfg << "runtime_kthreads 1\n";
    cfg << "runtime_spinning_kthreads 0\n";
    cfg << "runtime_guaranteed_kthreads 0\n";
    cfg << "runtime_priority lc\n";
    cfg << "runtime_quantum_us 0\n";

    cfg << "rootfs " << func.rootfs << "\n";
    cfg << "cpu " << func.cpu << "\n";
    cfg << "memoryMB " << func.memoryMB << "\n";

    return true;
}
