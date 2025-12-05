#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <thread>

struct FunctionData {
    std::string name;
    std::string rootfs;
    int cpu;
    int memoryMB;
    std::string cmd; // Command to run inside Junction
    std::map<std::string, std::string> env;
};

struct FunctionStatus {
    std::string name;
    bool running;
    int pid;
};

class JunctionD {
public:
    JunctionD() {
        monitorThread = std::thread([this]() { monitorInstances(); });
        monitorThread.detach();
    }

    ~JunctionD() {
        std::lock_guard<std::mutex> lock(mtx);
        for (auto &kv : statusMap) {
            remove(kv.first);
        }
    }

    bool spawn(const FunctionData &func) {
        std::string cfgFile;
        if (!generateConfig(func, cfgFile)) return false;

        const char* home = std::getenv("HOME");
        std::string junctionRun = std::string(home) + "/junction/build/junction/junction_run";

        pid_t pid = fork();
        if (pid < 0) return false;
        if (pid == 0) {
            // Child process runs junction_run
            execlp(junctionRun.c_str(), "junction_run", cfgFile.c_str(), nullptr);
            std::cerr << "[junctiond] Exec failed!" << std::endl;
            exit(1);
        }

        std::lock_guard<std::mutex> lock(mtx);
        statusMap[func.name] = {func.name, true, pid};
        std::cout << "[junctiond] Spawned " << func.name << " PID " << pid << std::endl;
        return true;
    }

    bool remove(const std::string &name) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = statusMap.find(name);
        if (it == statusMap.end()) {
            std::cerr << "[junctiond] No such instance: " << name << std::endl;
            return false;
        }

        FunctionStatus &status = it->second;
        if (status.running) {
            std::cout << "[junctiond] Killing PID " << status.pid << std::endl;
            if (kill(status.pid, SIGTERM) != 0) {
                perror("[junctiond] SIGTERM failed, trying SIGKILL");
                kill(status.pid, SIGKILL);
            }
            waitpid(status.pid, nullptr, 0);
            status.running = false;
        }

        // Clean up temporary config file
        std::string cfgPath = "/tmp/junction_" + name + ".config";
        unlink(cfgPath.c_str());

        statusMap.erase(it);
        std::cout << "[junctiond] Instance " << name << " fully removed." << std::endl;
        return true;
    }

    std::vector<FunctionStatus> list() {
        std::lock_guard<std::mutex> lock(mtx);
        std::vector<FunctionStatus> functions;
        for (auto &kv : statusMap) {
            functions.push_back(kv.second);
        }
        return functions;
    }

private:
    std::map<std::string, FunctionStatus> statusMap;
    std::mutex mtx;
    std::thread monitorThread;

    void monitorInstances() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::lock_guard<std::mutex> lock(mtx);

            for (auto it = statusMap.begin(); it != statusMap.end(); ) {
                int status;
                pid_t ret = waitpid(it->second.pid, &status, WNOHANG);
                if (ret > 0) {
                    std::cout << "[junctiond] Instance " << it->second.name
                              << " terminated unexpectedly." << std::endl;
                    it = statusMap.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    bool generateConfig(const FunctionData &func, std::string &cfgPath) {
        std::string name = func.name.empty() ? "function_default" : func.name;
        std::string rootfs = func.rootfs.empty() ? "/rootfs" : func.rootfs;
        int cpu = func.cpu > 0 ? func.cpu : 1;
        int memory = func.memoryMB > 0 ? func.memoryMB : 128;

        std::string workspaceDir = "/var/lib/junction/" + name;
        system(("mkdir -p " + workspaceDir).c_str());
        cfgPath = workspaceDir + "/" + name + ".config";

        std::ofstream cfg(cfgPath);
        if (!cfg.is_open()) {
            std::cerr << "[junctiond] Failed to open config file: " << cfgPath << std::endl;
            return false;
        }

        cfg << "# Junction configuration for function: " << name << "\n";
        cfg << "host_addr 127.0.0.1\n";
        cfg << "host_netmask 255.255.255.0\n";
        cfg << "host_gateway 127.0.0.1\n";
        cfg << "runtime_kthreads 1\n";
        cfg << "runtime_spinning_kthreads 0\n";
        cfg << "runtime_guaranteed_kthreads 0\n";
        cfg << "runtime_priority lc\n";
        cfg << "runtime_quantum_us 0\n";

        cfg << "rootfs " << rootfs << "\n";  // match junction_run config format
        cfg << "cpu " << cpu << "\n";
        cfg << "memoryMB " << memory << "\n";

        cfg.close();
        std::cout << "[junctiond] Generated config for " << name
                << " at " << cfgPath << std::endl;

        return true;
    }
};
