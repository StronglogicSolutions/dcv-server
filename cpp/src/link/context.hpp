#pragma once

#include <string>
#include <string_view>
#include <unistd.h>
#include <functional>
#include <logger.hpp>
#include <INIReader.h>
#include <assert.h>
#include <poll.h>


#include <array>


using namespace kiq::log;

enum
{
  READ_BUFFER_SIZE = 4096
};

static const std::string get_current_working_directory()
{
  std::string ret;
  char        buffer[PATH_MAX];
  if (getcwd(buffer, sizeof(buffer)) != nullptr)
    ret = std::string(buffer);
  return ret;
}

static const char g_null_terminator = '\0';
//------------------------------------------------------------------
inline bool is_socket_readable(int socket_fd, int timeout_ms = 30)
{
  klog().d("Polling socket");
  struct pollfd pfd;

  pfd.fd     = socket_fd;
  pfd.events = POLLIN;

  int result = poll(&pfd, 1, timeout_ms);

  if (result == -1)
  {
    klog().e("Poll failed");
    return false;
  }
  else if (result == 0)
  {
    klog().t("No data to read from {}", socket_fd);
    return false;
  }

  klog().d("Socket ready to be read");
  return true;
}
//------------------------------------------------------------------
inline bool write_to_channel(int socket, uint8_t *buffer, size_t size)
{
  klog().d("write_to_channel called to write {} bytes to {}", size, socket);

  size_t bytes_written = 0;
  while (bytes_written < size)
  {
    ssize_t curr_written = write(socket, buffer + bytes_written, size - bytes_written);
    if (curr_written <= 0)
    {
      klog().i("Channel write failed: {}", strerror(errno));
      return false;
    }

    bytes_written += curr_written;

    klog().t("Wrote {} of {} bytes", bytes_written, size);
  }

  return true;
}
//--------------------------------------------------------------------
inline bool read_channel(int socket_fd, uint8_t *buffer, size_t size)
{
  klog().d("read_channel called to read {} bytes from {}", size, socket_fd);

  size_t bytes_read = 0;
  while (bytes_read < size)
  {
    ssize_t curr_read = read(socket_fd, buffer + bytes_read, size - bytes_read);
    if (curr_read <= 0)
    {
      klog().i("Read returned {}. Could not read from socket: {}", curr_read, strerror(errno));
      return false;
    }

    bytes_read += curr_read;

    klog().t("read {} of {} bytes", bytes_read, size);
  }

  return true;
}
//--------------------------------------------------------------------
class context
{
public:
  static context&     instance();

  bool           run(int id);
  int            get_channel_socket() const;
  void           set_channel_socket(int socket_fd);
  bool           init(const std::string& token);

private:
  context()                           = default;
  context(const context& c)           = delete;
  context(context&& c)                = delete;
  context operator=(const context& c) = delete;
  context operator=(context&& c)      = delete;

  int  m_socket_fd;

};

extern context& ctx();

