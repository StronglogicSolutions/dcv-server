
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
    klog().i("Write failed with error: {}", strerror(errno));
    throw std::runtime_error("Failed to open virtual channel");
  }

  return true;
}
//**********************************************//
bool context::run(int id)
{
  klog().d("context::run()");
  size_t read_bytes;
  char read_buffer[READ_BUFFER_SIZE];

  klog().d("Created read buffer");
  std::string message = fmt::format("C++ Test {}", id);

  klog().d("Created message: {}", message);

  klog().d("Writing to channel");

  if (!write_to_channel(m_socket_fd, reinterpret_cast<uint8_t*>(const_cast<char*>(message.data())), message.size() + 1))
  {
    klog().e("Write failed with error: {}", strerror(errno));
    return false;
  }

  klog().d("Data written to channel");

  if (!is_socket_readable(m_socket_fd))
  {
    klog().d("Not yet ready to read");
    return false;
  }

  if (!read_channel(m_socket_fd, reinterpret_cast<uint8_t*>(read_buffer), message.size() + 1))
  {
    klog().e("Read failed with error: {}", strerror(errno));
    return false;
  }

  klog().d("Reading complete");

  read_buffer[read_bytes] = g_null_terminator;

  klog().d("Deserializing");

  std::string deserialized{read_buffer, read_bytes};

  klog().d("Read from channel: {}", deserialized);

  memset(read_buffer, 0, READ_BUFFER_SIZE);

  sleep(1);

  return true;
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
