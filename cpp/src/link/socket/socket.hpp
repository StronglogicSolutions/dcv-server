#include <arpa/inet.h>
#include <deque>
#include <future>
#include <vector>

//----------------------------------------------------------------
namespace kiq
{
using ipc_buf_t = std::vector<uint8_t>;
//-------------------------------------------------------------
class ipc
{
public:
  ipc();
  ~ipc();

  bool      is_active()                    const;
  bool      has_msgs ()                    const;
  ipc_buf_t get_msg  ();
  void      send_msg(unsigned char* data, size_t size);
  void      reset    ();
  bool      has_client()
  {
    return client_fd_ != -1;
  }

private:
  void      stop     ();
  void      start    ();
  void      listen_for_cxn   ();
  void      run();
  void      handle();

  int                        sx_       {0};
  int                        client_fd_{0};
  struct sockaddr_in         sx_addr_;
  std::future<void>          future_;
  bool                       active_   {true};
  std::deque<ipc_buf_t>      msgs_;
}; // server
} // ns kiq
