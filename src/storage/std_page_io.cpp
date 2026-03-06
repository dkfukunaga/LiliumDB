#include "std_page_io.h"

#include "common/file_format.h"

namespace LiliumDB {

Result<std::unique_ptr<StdPageIO>> StdPageIO::open(std::string_view path, OpenMode mode) {
    StdPageIO* p = new StdPageIO(mode);
    std::unique_ptr<StdPageIO> stdPageIO(p);

    std::ios::openmode om = std::ios::binary;
    switch (stdPageIO->mode_) {
        case OpenMode::ReadWrite:
            om |= std::ios::in | std::ios::out;
            break;
        case OpenMode::ReadOnly:
            om |= std::ios::in;
            break;
    }

    stdPageIO->file_.open(path.data(), om);

    if (!stdPageIO->file_.is_open()) {
        // if file doesn't exist and trying to open in read-only mode,
        // return FileErr instead of NewFile, since we won't be able to read from it
        if (mode == OpenMode::ReadOnly) {
            return Err(Status::fileErr("File does not exist."));
        } else { // create the file and then open it
            stdPageIO->file_.open(path.data(), std::ios::binary | std::ios::out | std::ios::app);
            stdPageIO->file_.close();
            stdPageIO->file_.open(path.data(), om);
        }
    }

    if (stdPageIO->file_.is_open()) {
        stdPageIO->file_.seekg(0, std::ios::end);
        auto fileSize = stdPageIO->file_.tellg();
        if (fileSize % PAGE_SIZE != 0) {
            return Err(Status::fileErr("Incorrect file size."));
        }
        stdPageIO->pageCount_ = static_cast<PageNum>(fileSize / PAGE_SIZE);
        return Ok(std::move(stdPageIO));
    }

    return Err(Status::fileErr("Failed to open file."));
}

Status StdPageIO::close() {
    if (isOpen()) {
        file_.close(); 
    }

    if (!file_.is_open()) {
        return Status::ok();
    } else {
        return Status::fileErr("An error occurred during or after closing the file.");
    }
}

Status StdPageIO::readPage(PageNum page, ByteSpan dst) {
    if (!isOpen()) {
        return Status::fileErr("Open file before reading.");
    }

    if (page >= pageCount_) {
        return Status::ioErr("Page number out of bounds.");
    }

    file_.seekg(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Status::ioErr("Seek failed.");
    }
    file_.read(reinterpret_cast<char*>(dst.data()), PAGE_SIZE);
    if (!file_) {
        return Status::ioErr("Error on file read.");
    }

    return Status::ok();
}

Status StdPageIO::writePage(PageNum page, ByteView src) {
    if (!isOpen()) {
        return Status::fileErr("Open file before writing.");
    }
    if (mode_ == OpenMode::ReadOnly) {
        return Status::fileErr("Cannot write to read-only file.");
    }

    if (page > pageCount_) { // allow writing to pageCount_ (appending), but not beyond
        return Status::ioErr("Page number out of bounds.");
    }

    file_.seekp(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Status::ioErr("Seek failed.");
    }
    file_.write(reinterpret_cast<const char*>(src.data()), PAGE_SIZE);
    if (!file_) {
        return Status::ioErr("Error on file write.");
    }
    
    if (page == pageCount_) {
        pageCount_++;
    }
    return Status::ok();
}


bool StdPageIO::isOpen() const { return file_.is_open(); }

} // namespace LiliumDB