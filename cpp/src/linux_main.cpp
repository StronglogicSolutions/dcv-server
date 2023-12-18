#include <iostream>
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

#include "extensions.pb.h"

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdarg.h>

enum {
    READ_BUFFER_SIZE = 4096
};

using ext_msg_t     = dcv::extensions::ExtensionMessage;
using ext_req_t     = dcv::extensions::Request;
using ext_dcv_t     = dcv::extensions::DcvMessage;
using ext_svt_req_t = dcv::extensions::SetupVirtualChannelRequest;
using ext_cvt_req_t = dcv::extensions::CloseVirtualChannelRequest;

int last_request_id = 1;

const std::string CHANNEL_NAME = "echo";

void WriteMessage(ext_msg_t &msg);

bool ReadFromHandle(int handle, uint8_t *buffer, size_t size) {
    size_t bytes_read = 0;

    while (bytes_read < size) {
        ssize_t curr_read = read(handle, buffer + bytes_read, size - bytes_read);
        if (curr_read <= 0) {
            klog().i("Could not read from handle: {}", strerror(errno));
            return false;
        }

        bytes_read += curr_read;
    }

    return true;
}

ext_dcv_t *ReadNextMessage() {
    uint32_t msg_sz = 0;

    /*
     * Read size of message, 32 bits
     */
    if (!ReadFromHandle(STDIN_FILENO, reinterpret_cast<uint8_t *>(&msg_sz), sizeof(msg_sz))) {
        return nullptr;
    }

    uint8_t *buf = new uint8_t[msg_sz];

    /*
     * Read message and unpack it
     */
    if (!ReadFromHandle(STDIN_FILENO, buf, msg_sz)) {
        return nullptr;
    }

    ext_dcv_t *msg = new ext_dcv_t();
    if (!msg->ParseFromArray(buf, msg_sz)) {
        klog().i("Could not unpack message from stdin");
        delete msg;
        return nullptr;
    }

    return msg;
}

void WriteRequest(ext_req_t *request) {
    ext_msg_t extension_msg;

    extension_msg.set_allocated_request(request);

    WriteMessage(extension_msg);
}

void RequestVirtualChannel() {
    auto request = new ext_req_t();
    auto msg = new ext_svt_req_t();

    msg->set_virtual_channel_name(CHANNEL_NAME);
    msg->set_relay_client_process_id(getpid());

    request->set_allocated_setup_virtual_channel_request(msg);
    request->set_request_id(std::to_string(last_request_id++));

    WriteRequest(request);
}

void CloseVirtualChannel() {
    // TODO: actually call local closure

    auto request = new ext_req_t();
    auto msg = new ext_cvt_req_t();

    msg->set_virtual_channel_name(CHANNEL_NAME);

    request->set_allocated_close_virtual_channel_request(msg);
    request->set_request_id(std::to_string(last_request_id++));

    WriteRequest(request);
}

bool WriteToHandle(int handle, uint8_t *buffer, size_t size) {
    size_t bytes_written = 0;

    while (bytes_written < size) {
        ssize_t curr_written = write(handle, buffer + bytes_written, size - bytes_written);
        if (curr_written <= 0) {
            klog().i("Could not write to handle: {}", strerror(errno));
            return false;
        }

        bytes_written += curr_written;
    }

    return true;
}

void WriteMessage(ext_msg_t &msg) {
    int msg_sz;

    msg_sz = static_cast<int>(msg.ByteSize());
    uint8_t *buf = new uint8_t[msg_sz];
    msg.SerializeToArray(buf, msg_sz);

    /*
     * Write size of message, 32 bits
     */
    if (!WriteToHandle(STDOUT_FILENO, reinterpret_cast<uint8_t *>(&msg_sz), sizeof(msg_sz))) {
        free(buf);
        return;
    }

    /*
     * Write message
     */
    WriteToHandle(STDOUT_FILENO, buf, msg_sz);
    fsync(STDOUT_FILENO);

    free(buf);
}

int main()
{
  using namespace kiq::log;
  kiq::log::klogger::init("dcv", "trace");

  klog().i("RequestVirtualChannel");

  RequestVirtualChannel();

  dcv::extensions::DcvMessage *msg = ReadNextMessage();
  if (msg == nullptr)
  {
      klog().i("Could not get messages from stdin");
      return -1;
  }

  klog().i("Expecting a response");

  if (!msg->has_response()) // Expecting a response
  {
      klog().i("Unexpected message case {}", msg->msg_case());
      delete msg;
      return -1;
  }

  if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS)
  {
      klog().i("Error in response for setup request {}", msg->response().status());
      delete msg;
      return -1;
  }

  klog().i("Connect to channel socket");

  const auto path = msg->response().setup_virtual_channel_response().relay_path();
  klog().i("Received path: {}", path);

  int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sockfd == -1)
  {
    perror("socket");
    return 1;
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
      return -1;
  }

  klog().i("Writing auth token on socket");

  const char* msg_ptr = &msg->response().setup_virtual_channel_response().virtual_channel_auth_token()[0];
  bool res = WriteToHandle(sockfd,
                            reinterpret_cast<uint8_t *>(const_cast<char*>(msg_ptr)),
                            msg->response().setup_virtual_channel_response().virtual_channel_auth_token().length());

  delete msg;

  if (!res) {
      klog().i("Write failed with error: {}", strerror(errno));
      return -1;
  }

  klog().i("Wait for the event");

  // Wait for the event
  msg = ReadNextMessage();
  if (msg == nullptr) {
      klog().i("Could not get messages from stdin");
      return -1;
  }

  // Expecting an event
  if (!msg->has_event()) {
      klog().i("Unexpected message case {}", msg->msg_case());
      delete msg;
      return -1;
  }

  // Expecting a setup event

  if (msg->event().event_case() != dcv::extensions::Event::EventCase::kVirtualChannelReadyEvent) {
      klog().i("Unexpected event case {}", msg->event().event_case());
      delete msg;
      return -1;
  }

  klog().i("Write to / Read from named pipe");

  // Write to / Read from named pipe
  for (int msg_number = 0; msg_number < 100; ++msg_number) {
      size_t read_bytes;
      char read_buffer[READ_BUFFER_SIZE];
      std::string message = "C++ Test {}" + std::to_string(msg_number);

      klog().i(message);

      if (!WriteToHandle(sockfd, reinterpret_cast<uint8_t*>(const_cast<char*>(message.data())), message.length() + 1))
      {
        klog().e("Write failed with error: {}", strerror(errno));
        break;
      }

      if (!ReadFromHandle(sockfd, reinterpret_cast<uint8_t*>(read_buffer), READ_BUFFER_SIZE - 1))
      {
        klog().e("Read failed with error: {}", strerror(errno));
        break;
      }

      read_buffer[read_bytes] = '\0';
      klog().i(read_buffer);
      memset(read_buffer, 0, READ_BUFFER_SIZE);

      sleep(1);
  }

  close(sockfd);

  delete msg;

  CloseVirtualChannel();

  // Wait for response
  msg = ReadNextMessage();
  if (msg == nullptr)
  {
      klog().e("Could not get messages from stdin");
      return -1;
  }

  // Expecting close response
  if (!msg->has_response())
  {
      klog().e("Unexpected message case {}", msg->msg_case());
      delete msg;
      return -1;
  }

  if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS)
  {
    klog().e("Error in response for close request {}", msg->response().status());
    delete msg;
    return -1;
  }

  delete msg;

  // We closed!
  return 0;
}
