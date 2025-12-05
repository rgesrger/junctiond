#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cstdlib>

struct FunctionData {
    std::string name;
    std::string rootfs;
    int cpu;
    int memoryMB;
    std::map<std::string, std::string> env;
};

struct FunctionStatus {
    std::string name;
    bool running;
    int pid;
};

class JunctionD {
public:
    bool spawn(const FunctionData& func) {
        std::string cmd = "junction_run --rootfs " + func.rootfs +
                          " --cpu " + std::to_string(func.cpu) +
                          " --mem " + std::to_string(func.memoryMB) + "M";
        std::cout << "[junctiond] Running: " << cmd << std::endl;
        int ret = system(cmd.c_str());
        if (ret == 0) {
            statusMap[func.name] = {func.name, true, 1234}; // placeholder PID
        }
        return ret == 0;
    }

    bool remove(const std::string& name) {
        std::cout << "[junctiond] Removing: " << name << std::endl;
        // TODO: actually kill the junction instance
        auto it = statusMap.find(name);
        if (it != statusMap.end()) {
            it->second.running = false;
            return true;
        }
        return false;
    }

    std::vector<FunctionStatus> list() {
        std::vector<FunctionStatus> functions;
        for (auto& kv : statusMap) {
            functions.push_back(kv.second);
        }
        return functions;
    }

private:
    std::map<std::string, FunctionStatus> statusMap;
};

int main() {
    JunctionD jd;
    FunctionData func{"myfunc", "/path/to/rootfs", 1, 128, {}};
    jd.spawn(func);

    auto funcs = jd.list();
    for (auto f : funcs) {
        std::cout << "Function " << f.name << " running? " << f.running << std::endl;
    }

    jd.remove("myfunc");
    return 0;
}
