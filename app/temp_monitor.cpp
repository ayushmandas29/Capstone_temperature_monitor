#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>
#include <thread>

#define TEMP_IOC_MAGIC 'T'
#define TEMP_IOC_RESET _IO(TEMP_IOC_MAGIC, 0)
#define TEMP_IOC_SET_DRIFT _IOW(TEMP_IOC_MAGIC, 1, int)

// ------------------------------------------------------------
// Lab 5 – C++ Temperature Monitor
// ------------------------------------------------------------
// State machine:
//   NORMAL    < 60°C
//   WARNING   60–79.9°C
//   CRITICAL  ≥ 80°C
//
// The app talks to the kernel driver through:
//   read()  – get current simulated temperature
//   ioctl() – reset sensor or change drift speed
// ------------------------------------------------------------

enum class State { NORMAL, WARNING, CRITICAL };

const char* state_name(State s) {
    switch (s) {
        case State::NORMAL:   return "NORMAL";
        case State::WARNING:  return "WARNING";
        case State::CRITICAL: return "CRITICAL";
    }
    return "UNKNOWN";
}

State classify(double t) {
    if (t < 60.0) return State::NORMAL;
    if (t < 80.0) return State::WARNING;
    return State::CRITICAL;
}

void usage(const char* prog) {
    std::cout << "Usage: " << prog << " [--reset] [--drift N]\n"
              << "  --reset       Reset simulated temperature to 25°C\n"
              << "  --drift N     Set drift range (default 10 = ±1.0°C/reading)\n";
}

int main(int argc, char* argv[]) {
    bool do_reset = false;
    int drift = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--reset") {
            do_reset = true;
        } else if (arg == "--drift") {
            if (i + 1 >= argc) {
                std::cerr << "--drift requires a value\n";
                return 1;
            }
            drift = std::stoi(argv[++i]);
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    int fd = open("/dev/tempsensor", O_RDWR);
    if (fd < 0) {
        std::cerr << "Cannot open /dev/tempsensor: " << strerror(errno) << "\n"
                  << "Did you load the driver with: sudo insmod tempsensor.ko ?\n";
        return 1;
    }

    if (do_reset) {
        if (ioctl(fd, TEMP_IOC_RESET) < 0) {
            std::cerr << "ioctl(RESET) failed: " << strerror(errno) << "\n";
            close(fd);
            return 1;
        }
        std::cout << "Sensor reset to 25.0°C\n";
    }

    if (drift >= 0) {
        if (ioctl(fd, TEMP_IOC_SET_DRIFT, &drift) < 0) {
            std::cerr << "ioctl(SET_DRIFT) failed: " << strerror(errno) << "\n";
            close(fd);
            return 1;
        }
        std::cout << "Drift set to " << drift
                  << " (±" << (drift / 10.0) << "°C per reading)\n";
    }

    State previous = State::NORMAL;
    bool first = true;

    std::cout << "\nTemperature Monitor started. Press Ctrl+C to stop.\n\n";

    while (true) {
        char buf[64]{};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);

        if (n < 0) {
            std::cerr << "read() failed: " << strerror(errno) << "\n";
            break;
        }

        buf[n] = '\0';
        double temperature = 0.0;
        try {
            temperature = std::stod(buf);
        } catch (...) {
            std::cerr << "Invalid temperature from driver: " << buf << "\n";
            break;
        }

        State current = classify(temperature);

        std::cout << std::fixed << std::setprecision(1)
                  << "Temperature: " << std::setw(5) << temperature << "°C"
                  << "   State: " << state_name(current);

        if (!first && current != previous) {
            std::cout << "   <-- TRANSITION: "
                      << state_name(previous) << " -> " << state_name(current);
        }

        std::cout << '\n';
        std::cout.flush();

        previous = current;
        first = false;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    close(fd);
    return 0;
}
