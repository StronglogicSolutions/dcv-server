#include <deque>
#include <future>
#include "ipc.hpp"

//----------------------------------------------------------------
namespace kiq
{
using ipc_msg_t = ipc_message::u_ipc_msg_ptr;
//-------------------------------------------------------------
class ipc
{
public:
  ipc();
  ~ipc();

  bool      is_active()                    const;
  bool      has_msgs ()                    const;
  ipc_msg_t get_msg  ();
  void      send_msg(unsigned char* data, size_t size);
  void      reset    ();

private:
  void      stop     ();
  void      start    ();
  void      run();
  void      recv();
  zmq::context_t             context_;
  zmq::socket_t              rx_;
  zmq::socket_t              tx_;
  std::future<void>          future_;
  bool                       active_{true};
  std::deque<kiq::ipc_msg_t> msgs_;
}; // server
} // ns kiq
