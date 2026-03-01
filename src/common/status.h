#ifndef LILIUMDB_STATUS_H
#define LILIUMDB_STATUS_H

#include <string>

namespace LiliumDB {

class Status {
public:
    enum Code {
        Ok,
        NewFile,
        ErrFile,
        ErrIO,
        ErrCorrupt,
        Error
    };

    Status(Code c, std::string msg = ""): code_(c), message_(std::move(msg)) {}

    explicit operator bool() const { return code_ == Ok; }

    Code code() const { return code_; }
    const std::string& message() const { return message_; }

private:
    Code code_;
    std::string message_;
};

} // namespace LiliumDB

#endif