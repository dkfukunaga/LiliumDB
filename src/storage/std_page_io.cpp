#include "std_page_io.h"

#include "common/file_format.h"

namespace LiliumDB {

DbResult<std::unique_ptr<PageIO>> StdPageIO::open(std::string_view path, OpenMode mode) {
    std::unique_ptr<StdPageIO> stdPageIO(new StdPageIO(mode));

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

    // if the file didn't open, try to create a new file in case it doesn't exist
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

    // if the file still isn't open, return an error
    if (!stdPageIO->file_.is_open()) {
        return Err(Status::fileErr("Failed to open file."));
    }

    stdPageIO->file_.seekg(0, std::ios::end);
    auto fileSize = stdPageIO->file_.tellg();
    if (fileSize % PAGE_SIZE != 0) {
        return Err(Status::fileErr("Incorrect file size."));
    }
    stdPageIO->pageCount_ = static_cast<PageNum>(fileSize / PAGE_SIZE);
    
    std::unique_ptr<PageIO> pageIO = std::move(stdPageIO);
    return Ok(std::move(pageIO));
}

VoidResult StdPageIO::close() {
    if (isOpen()) {
        file_.close(); 
    }

    if (!file_.is_open()) {
        return Ok(Success);
    } else {
        return Err(Status::fileErr("An error occurred during or after closing the file."));
    }
}

VoidResult StdPageIO::readPage(PageNum page, ByteSpan dst) {
    if (!isOpen()) {
        return Err(Status::fileErr("Open file before reading."));
    }

    if (page >= pageCount_) {
        return Err(Status::ioErr("Page number out of bounds."));
    }

    file_.seekg(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Err(Status::ioErr("Seek failed."));
    }
    file_.read(reinterpret_cast<char*>(dst.data()), PAGE_SIZE);
    if (!file_) {
        return Err(Status::ioErr("Error on file read."));
    }

    return Ok(Success);
}

VoidResult StdPageIO::writePage(PageNum page, ByteView src) {
    if (!isOpen()) {
        return Err(Status::fileErr("Open file before writing."));
    }
    if (mode_ == OpenMode::ReadOnly) {
        return Err(Status::fileErr("Cannot write to read-only file."));
    }

    if (page > pageCount_) { // allow writing to pageCount_ (appending), but not beyond
        return Err(Status::ioErr("Page number out of bounds."));
    }

    file_.seekp(static_cast<std::streamoff>(page) * PAGE_SIZE);
    if (!file_) {
        return Err(Status::ioErr("Seek failed."));
    }
    file_.write(reinterpret_cast<const char*>(src.data()), PAGE_SIZE);
    if (!file_) {
        return Err(Status::ioErr("Error on file write."));
    }
    
    if (page == pageCount_) {
        pageCount_++;
    }
    return Ok(Success);
}


bool StdPageIO::isOpen() const { return file_.is_open(); }

} // namespace LiliumDB