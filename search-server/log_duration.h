#pragma once
#include <chrono>
#include <iostream>
#include <cstdlib>

class LogDuration {
public:
    LogDuration() {
    }

    LogDuration(const std::string& title, std::ostream& os = std::cerr)
        : title_(title), output(os) {

    }

    ~LogDuration() {

        using namespace std;
        using namespace chrono;

        const auto end_time = steady_clock::now();
        const auto dur = end_time - start_time_;
        if (!title_.empty())
            output << title_ << ": "s;
        output << duration_cast<milliseconds>(dur).count() << " ms"s << endl;
    }

private:
    // В переменной будет время конструирования объекта LogDuration
    const std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    const std::string title_;
    std::ostream& output = std::cerr;
};

#define PROFILE_CONCAT_INTERNAL(X, Y) X ## Y
#define PROFILE_CONCAT(X, Y) PROFILE_CONCAT_INTERNAL(X, Y)
#define UNIQUE_VAR_NAME_PROFILE PROFILE_CONCAT(profileGuard, __LINE__)

#define LOG_DURATION(x) LogDuration UNIQUE_VAR_NAME_PROFILE(x)

#define LOG_DURATION_STREAM(x, stream) LogDuration UNIQUE_VAR_NAME_PROFILE(x, stream)
