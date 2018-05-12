// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "precompiled.hpp"
#include "utilities.hpp"
#include <iostream> // std::cerr
#include <time.h> // ::time_t, ::time(), ::asctime_r()

namespace Asteria {

Logger::Logger(const char *file, unsigned long line, const char *func) noexcept
	: m_file(file), m_line(line), m_func(func)
{
	m_stream <<std::boolalpha;
}
Logger::~Logger() = default;

void Logger::do_put(bool value){
	m_stream <<value;
}
void Logger::do_put(char value){
	m_stream <<value;
}
void Logger::do_put(signed char value){
	m_stream <<static_cast<int>(value);
}
void Logger::do_put(unsigned char value){
	m_stream <<static_cast<unsigned>(value);
}
void Logger::do_put(short value){
	m_stream <<static_cast<int>(value);
}
void Logger::do_put(unsigned short value){
	m_stream <<static_cast<unsigned>(value);
}
void Logger::do_put(int value){
	m_stream <<value;
}
void Logger::do_put(unsigned value){
	m_stream <<value;
}
void Logger::do_put(long value){
	m_stream <<value;
}
void Logger::do_put(unsigned long value){
	m_stream <<value;
}
void Logger::do_put(long long value){
	m_stream <<value;
}
void Logger::do_put(unsigned long long value){
	m_stream <<value;
}
void Logger::do_put(const char *value){
	m_stream <<value;
}
void Logger::do_put(const signed char *value){
	m_stream <<static_cast<const void *>(value);
}
void Logger::do_put(const unsigned char *value){
	m_stream <<static_cast<const void *>(value);
}
void Logger::do_put(const void *value){
	m_stream <<value;
}

bool write_log_to_stderr(Logger &&logger) noexcept
try {
	auto &stream = logger.get_stream();
	::time_t now;
	::time(&now);
	char time_str[26];
#ifdef _WIN32
	::ctime_s(time_str, sizeof(time_str), &now);
#else
	::ctime_r(&now, time_str);
#endif
	time_str[24] = 0;
	stream.set_caret(0);
	stream <<"[" <<time_str <<"] " <<logger.get_file() <<":" <<logger.get_line() <<" ## ";
	std::cerr <<stream.get_string() <<std::endl;
	return true;
} catch(...){
	return false;
}
bool throw_runtime_error(Logger &&logger){
	auto &stream = logger.get_stream();
	stream.set_caret(0);
	stream <<logger.get_func() <<": ";
	ASTERIA_DEBUG_LOG("*** Throwing exception: ", stream.get_string());
	throw std::runtime_error(stream.get_string());
}

}