#include "socket.hpp"
#include <logger.hpp>

static const char* RX_ADDR{"tcp://0.0.0.0:66465"};
static const char* TX_ADDR{"tcp://0.0.0.0:66466"};

using namespace kiq::log;
namespace kiq
{
ipc::ipc()
: context_{1},
  rx_(context_, ZMQ_ROUTER),
  tx_(context_, ZMQ_DEALER)
{
  rx_.set(zmq::sockopt::linger, 0);
  tx_.set(zmq::sockopt::linger, 0);
  rx_.set(zmq::sockopt::routing_id, "dcv_server");
  tx_.set(zmq::sockopt::routing_id, "dcv_server_tx");
  rx_.set(zmq::sockopt::tcp_keepalive, 1);
  tx_.set(zmq::sockopt::tcp_keepalive, 1);
  rx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  tx_.set(zmq::sockopt::tcp_keepalive_idle,  300);
  rx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);
  tx_.set(zmq::sockopt::tcp_keepalive_intvl, 300);

  kiq::set_log_fn([](const char* message) { klog().d(message); } );

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
  try
  {
    rx_.bind   (RX_ADDR);
    tx_.connect(TX_ADDR);

    future_ = std::async(std::launch::async, [this] { run(); });
    klog().i("Server listening on {}", RX_ADDR);
  }
  catch (const std::exception& e)
  {
    klog().e("Exception caught during start(): {}", e.what());
  }
}
//----------------------------------
void ipc::stop()
{
  rx_.disconnect(RX_ADDR);
  tx_.disconnect(TX_ADDR);
  active_ = false;
  if (future_.valid())
    future_.wait();
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
ipc_msg_t ipc::get_msg()
{
  ipc_msg_t msg = std::move(msgs_.front());
  msgs_.pop_front();
  return msg;
}
//----------------------------------
bool ipc::has_msgs() const
{
  return !msgs_.empty();
}

void ipc::run()
{
  klog().d("Receive worker initiated");
  while (active_)
    recv();
}
//----------------------------------
void ipc::recv()
{
  // using namespace kutils;
  using buffers_t = std::vector<ipc_message::byte_buffer>;

  zmq::message_t identity;
  if (!rx_.recv(identity) || identity.empty())
  {
    klog().i("Socket failed to receive");
    return;
  }

  buffers_t      buffer;
  zmq::message_t msg;
  int            more_flag{1};

  while (more_flag && rx_.recv(msg))
  {
    more_flag = rx_.get(zmq::sockopt::rcvmore);
    buffer.push_back( {static_cast<char*>(msg.data()),
                       static_cast<char*>(msg.data()) + msg.size() });
  }

  ipc_msg_t  ipc_msg = DeserializeIPCMessage(std::move(buffer));
  if (!ipc_msg)
  {
    klog().e("Failed to deserialize IPC message");
    return;
  }

  msgs_.push_back(std::move(ipc_msg));
}
//------------------------------------
void ipc::send_msg(unsigned char* data, size_t size)
{
  ipc_message::byte_buffer buffer    = {data, data + size};
  dcv_message              msg       = {buffer};
  const auto&              payload   = msg.data();
  const auto               frame_num = payload.size();

  klog().d("Sending IPC message: {}", msg.type());

  for (uint32_t i = 0; i < frame_num; i++)
  {
    auto flag = i == (frame_num - 1) ? zmq::send_flags::none : zmq::send_flags::sndmore;
    auto data = payload.at(i);

    zmq::message_t message{data.size()};
    std::memcpy(message.data(), data.data(), data.size());

    tx_.send(message, flag);
  }
}

} // ns kiq
