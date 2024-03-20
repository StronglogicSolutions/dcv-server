#pragma once

#include <string>
#include <string_view>
#include <unistd.h>
#include <functional>
#include <logger.hpp>
#include <INIReader.h>
#include <assert.h>
#include <poll.h>
#include "socket/socket.hpp"

using namespace kiq::log;

enum { READ_BUFFER_SIZE = 4096 };

static const char g_null_terminator = '\0';
//------------------------------------------------------------------
bool   is_socket_readable(int s, int timeout_ms = 30);
size_t write_to_channel  (int s, uint8_t*, size_t);
size_t read_channel      (int s, uint8_t*, size_t);
//--------------------------------------------------------------------
class context
{
public:
  static context&     instance();

  bool run();
  int  get_channel_socket() const;
  void set_channel_socket(int socket_fd);
  bool init(const std::string& token);

private:
  context()                           = default;
  context(const context& c)           = delete;
  context(context&& c)                = delete;
  context operator=(const context& c) = delete;
  context operator=(context&& c)      = delete;

  int      m_socket_fd; // DCV channel
  kiq::ipc m_endpoint;  // IPC channel

};

extern context& ctx();
