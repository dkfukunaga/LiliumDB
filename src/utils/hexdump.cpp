#include "hexdump.h"

namespace LiliumDB {

void hexdump(std::ostream& out,
             const ByteView& view,
             uint64_t baseAddress,
             std::string_view label) {
    static constexpr char DOUBLE_LINE[]  = "================================================================================\n";
    static constexpr char SINGLE_LINE[]  = "|----------+--------------------------------------------------+----------------|\n";
    static constexpr char ADDRESS_LINE[] = "| Address: |  0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F |                |\n";
    static constexpr char SKIPPED_LINE[] = "| ******** | *                                                |                |\n";

    char line[81] = {};
    int lineOffset = 0;

    auto writeToLine = [&](const char* fmt, auto... args) {
        lineOffset += snprintf(line + lineOffset, sizeof(line) - lineOffset, fmt, args...);
    };
    auto outputLine = [&]() {
        out << line << '\n';
        lineOffset = 0;
    };

    out << label << '\n';
    out << DOUBLE_LINE;
    out << ADDRESS_LINE;
    out << SINGLE_LINE;

    uint64_t end = baseAddress + view.size();
    uint64_t offset = baseAddress % 16;

    // iterators for hex section and ASCII section
    auto byteIter = view.begin();
    auto asciiIter = view.begin();

    // save the last line of bytes to omit repeated lines
    uint8_t lastBytes[16] = {};
    bool canRepeat = false;
    bool inRepeat = false;

    for (auto address = baseAddress - offset; address < end; address += 16) {
        // skip repeated lines
        if (canRepeat && end - address > 16) {
            if (memcmp(lastBytes, &(*byteIter), 16) == 0) {
                if (!inRepeat) {
                    out << SKIPPED_LINE;
                    inRepeat = true;
                }
                byteIter += 16;
                asciiIter += 16;
                continue;
            } else if (inRepeat) {
                // rewind by 16 and let the loop format and output last line
                byteIter -= 16;
                asciiIter -= 16;
                address -= 16;
                inRepeat = false;
            }
        } else if (inRepeat && address + 16 >= end) {
            // rewind by 16 and let the loop format and output last line
            byteIter -= 16;
            asciiIter -= 16;
            address -= 16;
            inRepeat = false;
        }

        // cache bytes and set canRepeat unless partial line
        if (address >= baseAddress && address + 16 <= end) {
            memcpy(lastBytes, &(*byteIter), 16);
            canRepeat = true;
        }

        // print memory address in hex in an 8 wide column with leading 0's
        writeToLine("| %08llX |", address);

        // print hex values
        for (int i = 0; i < 16; ++i) {
            if (address + i < baseAddress || address + i >= end) {
                // print blanks for bytes outside of ByteView
                writeToLine("   ");
            } else {
                writeToLine( " %02X", *byteIter++);
            }

            // add exta space between sets of 8 bytes
            if (i == 7) {
                writeToLine(" ");
            }
        }

        // print 2 spaces and a | to separate ASCII representation
        writeToLine(" |");

        // print ASCII represetntation
        for (int i = 0; i < 16; ++i) {
            if (address + i < baseAddress || address + i >= end) {
                // print blanks for bytes outside of ByteView
                writeToLine(" ");
            } else {
                if (*asciiIter >= 32 && *asciiIter <= 126) {
                    // only print valid ASCII characters
                    writeToLine("%c", *asciiIter);
                } else {
                    // filler for non-ASCII bytes
                    writeToLine(".");
                }
                ++asciiIter;
            }
        }

        // end line and flush to out
        writeToLine("|");
        outputLine();
    }

    out << DOUBLE_LINE << '\n';


}

} // namespace LiliumDB