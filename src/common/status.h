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
    } code;

    Status(Code c, const char* msg = nullptr): code(c), message_(msg) { }
    Status(Code c, std::string msg): Status(c, msg.c_str()) { }

    explicit operator bool() const { return code == Ok; }

    const char* message() const { return message_ == nullptr ? "" : message_; }

private:
    const char* message_;

};

} // namespace LiliumDB

#endif