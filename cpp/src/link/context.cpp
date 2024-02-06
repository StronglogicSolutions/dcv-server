#include "context.hpp"

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
  try
  {
    klog().d("context::run()");
    size_t      rx = 0;
    size_t      tx = 0;
    char        read_buffer[READ_BUFFER_SIZE];
    std::string message       = fmt::format("C++ Test {}", getpid());

    klog().d("Created read buffer");
    klog().d("Created message: {}", message);
    klog().d("Writing to channel");

    tx = write_to_channel(m_socket_fd,
                                    reinterpret_cast<uint8_t*>(const_cast<char*>(message.data())),
                                    message.size());
    if (!tx)
    {
      klog().e("Write failed with error: {}", strerror(errno));
      return false;
    }

    rx = read_channel(m_socket_fd, reinterpret_cast<uint8_t*>(read_buffer), message.size());
    if (!rx)
    {
      klog().e("Read failed with error: {}", strerror(errno));
      return false;
    }

    klog().d("Deserializing");

    read_buffer[rx] = g_null_terminator;
    std::string deserialized{read_buffer, rx};

    klog().d("Read from channel: {}", deserialized);

    memset(read_buffer, 0, READ_BUFFER_SIZE);

    return true;
  }
  catch (const std::exception& e)
  {
    klog().e("Exception caught: {}", e.what());
    return false;
  }
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
