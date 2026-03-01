#include "std_page_io.h"

#include "common/file_format.h"

namespace LiliumDB {

StdPageIO::~StdPageIO() { close(); }

Status StdPageIO::open(std::string_view path, OpenMode mode) {
    if (isOpen()) {
        return Status(Status::ErrFile, "File is already open.");
    }

    mode_ = mode;

    std::ios::openmode om;
    switch (mode) {
        case OpenMode::ReadWrite:
            om = std::ios::binary | std::ios::in | std::ios::out;
            break;
        case OpenMode::ReadOnly:
            om = std::ios::binary | std::ios::in;
            break;
        default:
            return Status(Status::ErrFile, "Unknown open mode.");
    }

    file_.open(path.data(), om);

    if (file_.is_open()) {
        file_.seekg(0, std::ios::end);
        pageCount_ = static_cast<PageNum>(file_.tellg() / PAGE_SIZE);
        return Status(Status::Ok, "File opened successfully.");
    }

    // file doesn't exist, create it
    file_.open(path.data(), std::ios::binary | std::ios::out);
    file_.close();
    file_.open(path.data(), om);

    if (file_.is_open()) {
        return Status(Status::NewFile, "New file created.");
    }

    return Status(Status::ErrFile, "Failed to open file.");
}

Status StdPageIO::close() {
    if (isOpen()) {
        file_.close(); 
    }

    if (!file_.is_open()) {
        return Status(Status::Ok, "File closed successfully.");
    } else {
        return Status(Status::ErrFile, "An error occurred during or after closing the file.");
    }
}

Status StdPageIO::readPage(PageNum page, ByteSpan dst) {
    if (!isOpen()) {
        return Status(Status::ErrFile, "Open file before reading.");
    }

    if (page >= pageCount_) {
        return Status(Status::ErrIO, "Page number out of bounds.");
    }

    file_.seekg(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Status(Status::ErrIO, "Seek failed.");
    }
    file_.read(reinterpret_cast<char*>(dst.data()), PAGE_SIZE);
    if (!file_) {
        return Status(Status::ErrIO, "Error on file read.");
    }

    return Status(Status::Ok, "Read successful.");
}

Status StdPageIO::writePage(PageNum page, ByteView src) {
    if (!isOpen()) {
        return Status(Status::ErrFile, "Open file before writing.");
    }
    if (mode_ == OpenMode::ReadOnly) {
        return Status(Status::ErrFile, "Cannot write to read-only file.");
    }

    if (page > pageCount_) { // allow writing to pageCount_ (appending), but not beyond
        return Status(Status::ErrIO, "Page number out of bounds.");
    }

    file_.seekp(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Status(Status::ErrIO, "Seek failed.");
    }
    file_.write(reinterpret_cast<const char*>(src.data()), PAGE_SIZE);
    if (!file_) {
        return Status(Status::ErrIO, "Error on file write.");
    }
    
    if (page == pageCount_) {
        pageCount_++;
    }
    return Status(Status::Ok, "Write successful.");
}


bool StdPageIO::isOpen() const { return file_.is_open(); }

} // namespace LiliumDB