#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

#include "extensions.pb.h"

#define _CRT_SECURE_NO_WARNINGS
#define LOG_FILE "dcv.log"
#include <stdio.h>
#include <stdarg.h>

char log_file[sizeof LOG_FILE + 20];

void
log_init(const char* logFile)
{
    logFile = logFile;

    FILE* file = fopen(logFile, "w");
    fprintf(file, "Created\n");
    fclose(file);
}

void
log_f(const char* format,
    ...)
{
    va_list args;

    FILE* file = fopen(log_file, "a");
    va_start(args, format);
    vfprintf(file, format, args);
    fprintf(file, "\n");
    va_end(args);
    fclose(file);
}

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
            log_f("Could not read from handle: %s", strerror(errno));
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
        log_f("Could not unpack message from stdin");
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
            log_f("Could not write to handle: %s", strerror(errno));
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

int main() {
    sprintf(log_file, "%s_%i.log", LOG_FILE, getpid());
    log_init(log_file);

    log_f("RequestVirtualChannel");

    RequestVirtualChannel();

    ext_dcv_t *msg = ReadNextMessage();
    if (msg == nullptr) {
        log_f("Could not get messages from stdin");
        return -1;
    }

    log_f("Expecting a response");

    // Expecting a response
    if (!msg->has_response()) {
        log_f("Unexpected message case %u", msg->msg_case());
        delete msg;
        return -1;
    }

    if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS) {
        log_f("Error in response for setup request %u", msg->response().status());
        delete msg;
        return -1;
    }

    log_f("Connect to named pipe");

    // Connect to named pipe
    int named_pipe_handle = open(msg->response().setup_virtual_channel_response().relay_path().c_str(),
                                 O_RDWR | O_SYNC);

    if (named_pipe_handle == -1) {
        log_f("Failed to create and setup named pipe: %s", strerror(errno));
        delete msg;
        return -1;
    }

    log_f("Writing auth token on named pipe");

    const char* msg_ptr = &msg->response().setup_virtual_channel_response().virtual_channel_auth_token()[0];
    bool res = WriteToHandle(named_pipe_handle,
                              reinterpret_cast<uint8_t *>(const_cast<char*>(msg_ptr)),
                              msg->response().setup_virtual_channel_response().virtual_channel_auth_token().length());

    delete msg;

    if (!res) {
        log_f("Write failed with error: %s", strerror(errno));
        return -1;
    }

    log_f("Wait for the event");

    // Wait for the event
    msg = ReadNextMessage();
    if (msg == nullptr) {
        log_f("Could not get messages from stdin");
        return -1;
    }

    // Expecting an event
    if (!msg->has_event()) {
        log_f("Unexpected message case %u", msg->msg_case());
        delete msg;
        return -1;
    }

    // Expecting a setup event

    if (msg->event().event_case() != dcv::extensions::Event::EventCase::kVirtualChannelReadyEvent) {
        log_f("Unexpected event case %u", msg->event().event_case());
        delete msg;
        return -1;
    }

    log_f("Write to / Read from named pipe");

    // Write to / Read from named pipe
    for (int msg_number = 0; msg_number < 100; ++msg_number) {
        size_t read_bytes;
        char read_buffer[READ_BUFFER_SIZE];
        std::string message = "C++ Test " + std::to_string(msg_number);

        log_f("Write: '%s'", message.c_str());

        if (!WriteToHandle(named_pipe_handle, reinterpret_cast<uint8_t*>(const_cast<char*>(message.data())), message.length() + 1)) {
            log_f("Write failed with error: %s", strerror(errno));
            break;
        }

        if (!ReadFromHandle(named_pipe_handle, reinterpret_cast<uint8_t*>(read_buffer), READ_BUFFER_SIZE - 1)) {
            log_f("Read failed with error: %s", strerror(errno));
            break;
        }

        read_buffer[read_bytes] = '\0';
        log_f("Read: %s", read_buffer);
        memset(read_buffer, 0, READ_BUFFER_SIZE);

        sleep(1);
    }

    close(named_pipe_handle);

    delete msg;

    CloseVirtualChannel();

    // Wait for response
    msg = ReadNextMessage();
    if (msg == nullptr) {
        log_f("Could not get messages from stdin");
        return -1;
    }

    // Expecting close response
    if (!msg->has_response()) {
        log_f("Unexpected message case %u", msg->msg_case());
        delete msg;
        return -1;
    }

    if (msg->response().status() != dcv::extensions::Response_Status::Response_Status_SUCCESS)
    {
      log_f("Error in response for close request %u", msg->response().status());
      delete msg;
      return -1;
    }

    delete msg;

    // We closed!
    return 0;
}
