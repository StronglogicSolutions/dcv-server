#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <thread>
#include <type_traits>
#include <future>
#include <zmq.hpp>
#include <functional>

namespace kiq {
using external_log_fn = std::function<void(const char*)>;
namespace
{
  static void noop(const char*) { (void)"NOOP"; }
  external_log_fn log_fn = noop;
} // ns

inline void set_log_fn(external_log_fn fn)
{
  log_fn = fn;
}

namespace constants {
static const uint8_t DCV_TYPE{0x00};

static const std::map<uint8_t, const char*> IPC_MESSAGE_NAMES{
  { DCV_TYPE, "DCV_TYPE" }
};

namespace index {
static const uint8_t EMPTY     = 0x00;
static const uint8_t TYPE      = 0x01;
static const uint8_t DATA      = 0x02;
} // namespace index
}

//---------------------------------------------------------------------
class ipc_message
{
public:
using byte_buffer   = std::vector<uint8_t>;
using u_ipc_msg_ptr = std::unique_ptr<ipc_message>;

ipc_message() = default;
//--------------------
ipc_message(const ipc_message& msg)
{
  m_frames = msg.m_frames;
}
//--------------------
virtual ~ipc_message() {}
//--------------------
uint8_t type() const
{
  return m_frames.at(constants::index::TYPE).front();
}
//--------------------
std::vector<byte_buffer> data()
{
  return m_frames;
}
//--------------------
std::vector<byte_buffer> m_frames;
//--------------------
virtual std::string to_string() const
{
  return constants::IPC_MESSAGE_NAMES.at(type());
}
//--------------------
static u_ipc_msg_ptr clone(const ipc_message& msg)
{
  return std::make_unique<ipc_message>(msg);
}
};

//---------------------------------------------------------------------
class dcv_message : public ipc_message
{
public:
  dcv_message(const byte_buffer& payload)
  {
    m_frames = {
      byte_buffer{},
      byte_buffer{constants::DCV_TYPE},
      payload
    };
  }
//--------------------
  dcv_message(const std::vector<byte_buffer>& data)
  {
    m_frames = {
      byte_buffer{},
      byte_buffer{data.at(constants::index::TYPE)},
      byte_buffer{data.at(constants::index::DATA)}
    };
  }
  //--------------------
  const byte_buffer payload() const
  {
    return m_frames.at(constants::index::DATA);
  }
};
//---------------------------------------------------------------------
inline ipc_message::u_ipc_msg_ptr DeserializeIPCMessage(std::vector<ipc_message::byte_buffer>&& data)
{
  uint8_t message_type = *(data.at(constants::index::TYPE).data());
  switch (message_type)
  {
    case (constants::DCV_TYPE):             return std::make_unique<dcv_message>    (data);
    default:                                return nullptr;
  }
}

// class IPCTransmitterInterface
// {
// public:
//   virtual ~IPCTransmitterInterface() = default;
// //--------------------
//   void send_ipc_message(ipc_message::u_ipc_msg_ptr message)
//   {
//     const auto     payload   = message->data();
//     const size_t   frame_num = payload.size();

//     for (int i = 0; i < frame_num; i++)
//     {
//       const auto     flag = i == (frame_num - 1) ? zmq::send_flags::none : zmq::send_flags::sndmore;
//       const auto     data  = payload.at(i);
//       zmq::message_t message{data.size()};
//       std::memcpy(message.data(), data.data(), data.size());
//       socket().send(message, flag);
//     }
//     on_done();
//   }

// protected:
//   virtual zmq::socket_t& socket()  = 0;
//   virtual void           on_done() = 0;
// };
// //---------------------------------------------------------------------
// class IPCBrokerInterface
// {
// public:
//   virtual ~IPCBrokerInterface() = default;
//   virtual void on_heartbeat(std::string_view peer) = 0;
//   virtual void process_message(ipc_message::u_ipc_msg_ptr) = 0;
// };
// //---------------------------------------------------------------------
// class MessageHandlerInterface
// {
// public:
//   virtual ~MessageHandlerInterface() = default;
//   virtual void process_message(ipc_message::u_ipc_msg_ptr) = 0;
// };

// //---------------------------------------------------------------------
// class IPCHandlerInterface : public MessageHandlerInterface,
//                             public IPCTransmitterInterface
// {
// public:
//   ~IPCHandlerInterface() override = default;
// };
// //---------------------------------------------------------------------
// using client_handlers_t = std::map<std::string_view, IPCHandlerInterface*>;
} // ns kiq

