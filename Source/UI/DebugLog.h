#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <juce_core/juce_core.h>

namespace conductor {

class DebugLog {
public:
    static DebugLog& getInstance() {
        static DebugLog instance;
        return instance;
    }

    void log(const std::string& tag, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();
        std::string timeStr = ss.str();

        std::string fullMsg = "[" + timeStr + "] [" + tag + "] " + message;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mMessages.push_back(fullMsg);
            if (mMessages.size() > 500) {
                mMessages.erase(mMessages.begin());
            }

            // Write to stdout for terminal debugging
            std::cout << fullMsg << std::endl;

            // Write to persistent log file
            if (mFileStream.is_open()) {
                mFileStream << fullMsg << std::endl;
                mFileStream.flush();
            }
        }

        if (mOnNewMessage) {
            juce::MessageManager::callAsync([this, fullMsg]() {
                if (mOnNewMessage) mOnNewMessage(fullMsg);
            });
        }
    }

    std::vector<std::string> getRecentLogs() {
        std::lock_guard<std::mutex> lock(mMutex);
        return mMessages;
    }

    void setListener(std::function<void(const std::string&)> listener) {
        mOnNewMessage = listener;
    }

private:
    DebugLog() {
        const char* home = std::getenv("HOME");
        std::filesystem::path logPath;
        if (home) {
            logPath = std::filesystem::path(home) / "Library" / "Application Support" / "Conductor";
            std::error_code ec;
            std::filesystem::create_directories(logPath, ec);
            logPath /= "conductor.log";
            mFileStream.open(logPath.string(), std::ios::out | std::ios::app);
        }
    }

    ~DebugLog() {
        if (mFileStream.is_open()) {
            mFileStream.close();
        }
    }

    std::mutex mMutex;
    std::vector<std::string> mMessages;
    std::ofstream mFileStream;
    std::function<void(const std::string&)> mOnNewMessage;
};

inline void Log(const std::string& tag, const std::string& msg) {
    DebugLog::getInstance().log(tag, msg);
}

} // namespace conductor
