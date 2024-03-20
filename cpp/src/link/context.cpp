#include "context.hpp"
#include <sys/socket.h>

bool is_socket_readable(int socket_fd, int timeout_ms)
{
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
    return false;

  return true;
}
//------------------------------------------------------------------
size_t write_to_channel(int socket, uint8_t *buffer, size_t size)
{
  klog().d("write_to_channel called to write {} bytes to {}", size, socket);

  size_t bytes_written = 0;
  while (bytes_written < size)
  {
    ssize_t curr_written = write(socket, buffer + bytes_written, size - bytes_written);
    if (curr_written <= 0)
    {
      klog().i("Channel write failed with {} bytes written. Error:{}",
        bytes_written, strerror(errno));
      return 0;
    }

    bytes_written += curr_written;

    klog().t("Wrote {} of {} bytes", bytes_written, size);
  }

  return bytes_written;
}
//--------------------------------------------------------------------
size_t read_channel(int socket_fd, uint8_t *buffer, size_t size)
{
  klog().d("read_channel called to read {} bytes from {}", size, socket_fd);

  size_t bytes_read = 0;
  while (bytes_read < size)
  {
    ssize_t curr_read = read(socket_fd, buffer + bytes_read, size - bytes_read);
    if (curr_read <= 0)
    {
      klog().i("Error or unable to read with {} bytes read. Last read returned {}. Last error: {}",
        bytes_read, curr_read, strerror(errno));
      return 0;
    }

    bytes_read += curr_read;

    klog().t("read {} of {} bytes", bytes_read, size);
  }

  return bytes_read;
}

//--------------------------------------------------------------------
bool context::init(const std::string& token)
{
  const char* msg_ptr = &token[0];
  bool res = write_to_channel(m_socket_fd,
                reinterpret_cast<uint8_t *>(const_cast<char*>(msg_ptr)),
                token.size());
  if (!res)
  {
    klog().e("Write failed with error: {}", strerror(errno));
    throw std::runtime_error("Failed to open virtual channel");
  }

  return true;
}
//--------------------------------------------------------------------
bool context::run()
{
  try
  {
    if (is_socket_readable(m_socket_fd, 100)) // This sets the work-rate of our program
    {
      char read_buffer[READ_BUFFER_SIZE];
      auto size = recv(m_socket_fd, read_buffer, READ_BUFFER_SIZE, 0);
      if (size == -1)
        klog().e("Error reading from channel");
      else
      if (size)
      {
        std::string out_msg{read_buffer, static_cast<size_t>(size)};
        klog().d("Outbound message:\n{}", out_msg);
        m_endpoint.send_msg(reinterpret_cast<unsigned char*>(read_buffer), size);
      }
    }

    try
    {
      if (m_endpoint.has_msgs())
      {
        const auto msg   = m_endpoint.get_msg();
        const auto msg_s = std::string{
          reinterpret_cast<char*>(const_cast<unsigned char*>(msg.data())),
          reinterpret_cast<char*>(const_cast<unsigned char*>(msg.data())) + msg.size()};

        klog().i("Has message to send. Sending:\n{}", msg_s);

        if (!write_to_channel(m_socket_fd, const_cast<uint8_t*>(msg.data()), msg.size()))
          return false;
      }

      return true;
    }
    catch (const std::exception& e)
    {
      klog().e( "Error while polling: {}", e.what());
    }
  }
  catch (const std::exception& e)
  {
    klog().e( "Exception caught: {}", e.what());
  }

  return false;
}
//--------------------------------------------------------------------
void context::set_channel_socket(int socket_fd)
{
  m_socket_fd = socket_fd;
}

//--------------------------------------------------------------------
int context::get_channel_socket() const
{
  return m_socket_fd;
}

//--------------------------------------------------------------------
context& context::instance()
{
  static context* context_instance;
  if (!context_instance)
    context_instance = new context;

  assert(context_instance);

  return *context_instance;
}

//--------------------------------------------------------------------
context& ctx()
{
  return context::instance();
}
