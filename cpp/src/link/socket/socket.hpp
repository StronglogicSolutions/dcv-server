#include <deque>
#include <future>
#include <zmq.hpp>

//----------------------------------------------------------------
namespace kiq
{
//-------------------------------------------------------------
class server
{
public:
  server();
  ~server();

  bool      is_active()                    const;
  bool      has_msgs ()                    const;
  void      reply    (bool success = true);
  ipc_msg_t get_msg  ();
  void      send_msg (ipc_msg_t);
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
