#include "context.hpp"
#include <sys/socket.h>

//***********************************************//
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
//**********************************************//
bool context::run(int id)
{
  klog().d( "context::run()");
  try
  {
    if (is_socket_readable(m_socket_fd))
    {
      char read_buffer[READ_BUFFER_SIZE];
      auto size = recv(m_socket_fd, read_buffer, READ_BUFFER_SIZE, 0);
      if (size == -1)
        klog().e("Error reading from channel");
      else
      if (!size)
        klog().d( "Nothing to read");
      else
        m_endpoint.send_msg(reinterpret_cast<unsigned char*>(read_buffer), size);
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
//**********************************************//
void context::set_channel_socket(int socket_fd)
{
  m_socket_fd = socket_fd;
}

//**********************************************//
int context::get_channel_socket() const
{
  return m_socket_fd;
}

//**********************************************//
context& context::instance()
{
  static context* context_instance;
  if (!context_instance)
    context_instance = new context; // Memory released on exit

  assert(context_instance);

  return *context_instance;
}

//**********************************************//
context& ctx()
{
  return context::instance();
}
