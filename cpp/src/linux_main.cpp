#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <logger.hpp>
#include "link/context.hpp"

#include "extensions.pb.h"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>

using namespace kiq::log;

using ext_msg_t     = dcv::extensions::ExtensionMessage;
using ext_req_t     = dcv::extensions::Request;
using ext_dcv_t     = dcv::extensions::DcvMessage;
using ext_svt_req_t = dcv::extensions::SetupVirtualChannelRequest;
using ext_cvt_req_t = dcv::extensions::CloseVirtualChannelRequest;

             int  last_request_id   = 1;
const std::string CHANNEL_NAME = "echo";
//-----------------------------------------------------------------
//-----------------------------------------------------------------
void write_msg(ext_msg_t &msg);

//-----------------------------------------------------------------
ext_dcv_t *read_next_msg()
{
  uint32_t msg_sz = 0;

  if (!read_channel(STDIN_FILENO, reinterpret_cast<uint8_t *>(&msg_sz), sizeof(msg_sz)))
    return nullptr;

  uint8_t *buf = new uint8_t[msg_sz];

  if (!read_channel(STDIN_FILENO, buf, msg_sz))
    return nullptr;

  ext_dcv_t *msg = new ext_dcv_t();
  if (!msg->ParseFromArray(buf, msg_sz))
  {
    klog().i("Could not unpack message from stdin");
    delete msg;
    return nullptr;
  }

  return msg;
}
//-----------------------------------------------------------------
void write_request(ext_req_t *request)
{
  ext_msg_t extension_msg;

  extension_msg.set_allocated_request(request);

  write_msg(extension_msg);
}
//-----------------------------------------------------------------
void open_virtual_channel()
{
  auto request = new ext_req_t();
  auto msg = new ext_svt_req_t();

  msg->set_virtual_channel_name(CHANNEL_NAME);
  msg->set_relay_client_process_id(getpid());

  request->set_allocated_setup_virtual_channel_request(msg);
  request->set_request_id(std::to_string(last_request_id++));

  write_request(request);
}
//-----------------------------------------------------------------
void close_virtual_channel()
{
  auto request = new ext_req_t();
  auto msg     = new ext_cvt_req_t();

  msg->set_virtual_channel_name(CHANNEL_NAME);

  request->set_allocated_close_virtual_channel_request(msg);
  request->set_request_id(std::to_string(last_request_id++));

  write_request(request);
}
//-----------------------------------------------------------------
void write_msg(ext_msg_t &msg)
{
  int msg_sz;

  msg_sz = static_cast<int>(msg.ByteSize());
  uint8_t *buf = new uint8_t[msg_sz];
  msg.SerializeToArray(buf, msg_sz);

  if (!write_to_channel(STDOUT_FILENO, reinterpret_cast<uint8_t *>(&msg_sz), sizeof(msg_sz)))
  {
    free(buf);
    return;
  }

  write_to_channel(STDOUT_FILENO, buf, msg_sz);
  fsync(STDOUT_FILENO);

  free(buf);
}

void DriverOpen()
{
  open_virtual_channel();

  dcv::extensions::DcvMessage *msg = read_next_msg();
  if (!msg)
  {
    klog().e("Could not get messages from stdin");
    throw std::runtime_error("Failed to open virtual channel");
  }

  klog().i("Expecting a response");

  if (!msg->has_response())
  {
    klog().i("Unexpected message case {}", msg->msg_case());
    delete msg;
    throw std::runtime_error("Failed to open virtual channel");
  }

  if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS)
  {
    klog().i("Error in response for setup request {}", msg->response().status());
    delete msg;
    throw std::runtime_error("Failed to open virtual channel");
  }

  klog().i("Connect to channel socket");

  const auto path = msg->response().setup_virtual_channel_response().relay_path();
  klog().i("Received path: {}", path);

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    perror("socket");
    throw std::runtime_error("Failed to open virtual channel");
  }

  struct sockaddr_un address;
  memset((char*)&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strcpy(&address.sun_path[1], path.c_str());
  int len = sizeof(address.sun_family) + strlen(path.c_str()) + 1;

  if (connect(sockfd, (struct sockaddr*)&address, len) == -1)
  {
    perror("connect");
    close(sockfd);
    klog().i("Failed to open socket: {}", strerror(errno));
    delete msg;
    throw std::runtime_error("Failed to open virtual channel");
  }

  ctx().set_channel_socket(sockfd);

  klog().i("Writing auth token on socket");

  bool initialized = ctx().init
    (msg->response().setup_virtual_channel_response().virtual_channel_auth_token());

  delete msg;

  if (!initialized)
    throw std::runtime_error("Failed to open virtual channel");

  klog().i("Wait for the event");

  msg = read_next_msg();
  if (!msg)
  {
    klog().i("Could not get messages from stdin");
    throw std::runtime_error("Failed to open virtual channel");
  }

  if (!msg->has_event())
  {
    klog().i("Unexpected message case {}", msg->msg_case());
    delete msg;
    throw std::runtime_error("Failed to open virtual channel");
  }

  if (msg->event().event_case() != dcv::extensions::Event::EventCase::kVirtualChannelReadyEvent)
  {
    klog().i("Unexpected event case {}", msg->event().event_case());
    delete msg;
    throw std::runtime_error("Failed to open virtual channel");
  }

  // TODO: Print event information
  // DO something?

  delete msg;


}
//-----------------------------------------------------------------
void DriverRun()
{
  klog().d("Running driver");
  while (!ctx().run(0))
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

}
//-----------------------------------------------------------------
void DriverClose()
{
  klog().t("Closing driver");
  int sockfd = ctx().get_channel_socket();
  close(sockfd);

  close_virtual_channel();

  dcv::extensions::DcvMessage* msg = read_next_msg();
  if (!msg)
  {
    klog().e("Could not get messages from stdin");
    throw std::runtime_error("Failed to close driver");
  }

  if (!msg->has_response())
  {
    klog().e("Unexpected message case {}", msg->msg_case());
    delete msg;
    throw std::runtime_error("Failed to close driver");
  }

  if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS)
  {
    klog().e("Error in response for close request {}", msg->response().status());
    delete msg;
    throw std::runtime_error("Failed to close driver");
  }

  delete msg;
}
//-----------------------------------------------------------------
//-------------------------MAIN------------------------------------
//-----------------------------------------------------------------
int main()
{
  try
  {
    klogger::init("dcv", "trace");

    klog().i("open_virtual_channel");

    DriverOpen();

    klog().i("Write/Read from virtual channel");

    DriverRun();

    klog().i("DriverRun() completed");

    DriverClose();

    return 0;
  }
  catch (const std::exception& e)
  {
    klog().e("Exception caught: {}", e.what());
    return -1;
  }

  return 0;
}
