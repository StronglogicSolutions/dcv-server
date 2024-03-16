#include "socket.hpp"
#include <logger.hpp>
#include <unistd.h>

static const char* RX_ADDR{"tcp://0.0.0.0:62466"};
static const char* TX_ADDR{"tcp://0.0.0.0:62467"};
static const int   SCK_ERR{-1};

using namespace kiq::log;
namespace kiq
{
ipc::ipc()
{
  if ((sx_ = socket(AF_INET, SOCK_STREAM, 0)) == SCK_ERR)
  {
    klog().e("Error creating socket");
    throw std::runtime_error("Error creating socket");
  }

  sx_addr_.sin_family      = AF_INET;
  sx_addr_.sin_port        = htons(62466);
  sx_addr_.sin_addr.s_addr = INADDR_ANY;

  if (bind(sx_, (struct sockaddr*)&sx_addr_, sizeof(sx_addr_)) == SCK_ERR)
  {
    klog().e("Error binding socket");
    close(sx_);
    throw std::runtime_error("Error binding socket");
  }

  start();
}
//----------------------------------
ipc::~ipc()
{
  stop();
}
//----------------------------------
void ipc::start()
{
  active_ = true;
  try
  {
    future_ = std::async(std::launch::async, [this] { run(); });
  }
  catch (const std::exception& e)
  {
    klog().e("Exception caught during start(): {}", e.what());
  }
}
//----------------------------------
void ipc::stop()
{
  active_ = false;
  if (future_.valid())
    future_.wait();

  close(sx_);
  sx_ = 0;

  klog().d("Server has stopped");
}
//----------------------------------
void ipc::reset()
{
  klog().i("Server is resetting connection");
  stop ();
  start();
}
//----------------------------------
bool ipc::is_active() const
{
  return active_;
}
//----------------------------------
ipc_buf_t ipc::get_msg()
{
  ipc_buf_t msg = msgs_.front();
  msgs_.pop_front();
  return msg;
}
//----------------------------------
bool ipc::has_msgs() const
{
  return !msgs_.empty();
}
//----------------------------------
void ipc::run()
{
  klog().d("Receive worker initiated");
  while (active_)
    listen_for_cxn();
}
//----------------------------------
void ipc::listen_for_cxn()
{
  if (listen(sx_, 5) == SCK_ERR)
  {
    klog().e("Error listening for connections");
    close(sx_);
    return;
  }

  klog().d("Server listening on port 62466...");

  while (true)
  {
    struct sockaddr_in client_addr;
           socklen_t   addr_len = sizeof(client_addr);

    if ((client_fd_ = accept(sx_, (struct sockaddr*)&client_addr, &addr_len)) == SCK_ERR)
    {
      klog().e("Error accepting connection");
      continue;
    }

    klog().d("Connection accepted from {}:{}",
      inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    handle();

    close(client_fd_);

    client_fd_ = 0;
  }
}
//----------------------------------
void ipc::handle()
{
  ipc_buf_t buffer;
  buffer.resize(16384);

  while (client_fd_)
  {
    ssize_t bytes_rx = recv(client_fd_, buffer.data(), buffer.size(), 0);
    if (bytes_rx <= 0)
    {
      klog().e("Error receiving data from client. Errno {}", strerror(errno));
      return;
    }

    klog().t("Received from client application: {}", std::string{
      reinterpret_cast<char*>(buffer.data()), reinterpret_cast<char*>(buffer.data()) + bytes_rx});

    msgs_.push_back(ipc_buf_t{ buffer.data(), buffer.data() + bytes_rx });
  }
}
//------------------------------------
void ipc::send_msg(unsigned char* data, size_t size)
{
  klog().d("Sending IPC message of size {}", size);

  if (send(client_fd_, data, size, 0) == -1)
    klog().e("Error sending data to client");
}

} // ns kiq
