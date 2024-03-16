#pragma once

#include <map>
#include <fstream>
#include <source_location>
#include <fmt/format.h>
#include <cctype>
#include "active.hpp"

namespace kiq::log
{
static const int32_t flush_limit = 100;
//-------------------------------------------------
enum colour
{
  YELLOW   = 93,
  RED      = 31,
  GREEN    = 38,
  WHITE    = 97,
  MAGENTA  = 35,
  TURQOISE = 36
};
//-------------------------------------------------
enum class loglevel
{
  silent = 0x00,
  fatal  = 0x01,
  error  = 0x02,
  warn   = 0x03,
  info   = 0x04,
  debug  = 0x05,
  trace  = 0x06
};
//-------------------------------------------------
using loglevel_t       = std::map<std::string, loglevel>;
static const loglevel_t log_level
{
  {"trace",  loglevel::trace  },
  {"debug",  loglevel::debug  },
  {"info",   loglevel::info   },
  {"warn",   loglevel::warn   },
  {"error",  loglevel::error  },
  {"fatal",  loglevel::fatal  },
  {"silent", loglevel::silent }
};
//-------------------------------------------------
colour to_colour(loglevel level);
//-------------------------------------------------
static const char* default_log_level = "info";
//-------------------------------------------------
struct fmt_loc
{
  fmt_loc(const char*        s, const std::source_location& l = std::source_location::current());
  fmt_loc(const std::string& s, const std::source_location& l = std::source_location::current());
  const char*          value_;
  std::source_location loc_;
};
//-------------------------------------------------
//--------------------klogger----------------------
//-------------------------------------------------
class klogger
{
public:
  klogger(const std::string& name  = "KLOG",
          const std::string& level = default_log_level,
          const std::string& path = "/tmp/",
          bool  use_stdout        = true);

  ~klogger();

  template<typename... Args>
  void d(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::debug)
      log(loglevel::debug, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  template<typename... Args>
  void w(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::warn)
      log(loglevel::warn, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  template<typename... Args>
  void i(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::info)
      log(loglevel::info, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  template<typename... Args>
  void e(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::error)
      log(loglevel::error, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  template<typename... Args>
  void f(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::fatal)
      log(loglevel::fatal, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  template<typename... Args>
  void t(const fmt_loc& format, Args&&... args)
  {
    if (level_ >= loglevel::trace)
      log(loglevel::trace, fmt::format(fmt::runtime(format.value_), std::forward<Args>(args)...), format.loc_);
  }
//-------------------------------------------------
  loglevel get_level() const;
//-------------------------------------------------
  static void    init(const std::string& name, const std::string& level = default_log_level, bool use_stdout = true);
  static klogger& instance();

private:
  void set_level(loglevel level);
  bool open_file();
  void log(loglevel level, const std::string& message, const std::source_location& loc);

  loglevel         level_;
  std::string      path_;
  std::string      buffer_;
  std::ofstream*   ostream_ptr_{new std::ofstream};
  active_object<>* active_ptr_ {new active_object<>};
  bool             to_stdout_  {true};
};

klogger& klog();

}  // namespace kiq::log
