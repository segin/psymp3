#include "debug.h"
#include <iostream>

std::ofstream Debug::m_logfile;
std::mutex Debug::m_mutex;
std::unordered_set<std::string> Debug::m_enabled_channels;
bool Debug::m_log_to_file = false;

void Debug::init(const std::string& logfile, const std::vector<std::string>& channels) {}
void Debug::shutdown() {}
bool Debug::isChannelEnabled(const std::string& channel) { return true; }
void Debug::write(const std::string& channel, const std::string& function, int line, const std::string& message) {
    // std::cout << "[DEBUG][" << channel << "] " << message << std::endl;
}
