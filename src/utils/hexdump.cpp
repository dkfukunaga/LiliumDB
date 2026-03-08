#include "hexdump.h"

namespace LiliumDB {


void hexdump(
    std::ostream& out,
    const ByteView& view,
    uint64_t base_address,
    std::string_view label
) {
    static constexpr char HEX_CHARS[] = "0123456789ABCDEF";

    int64_t len = view.size();
    if (len == 0) {
        return;
    }

    int64_t address = base_address;
    int64_t end = base_address + len;


    for (int i = 0; i < len; i += 16) {
        char line[81];
        int offset = 0;

        // print memory address in hex in an 8 wide column with leading 0's
        offset += snprintf(line + offset, sizeof(line) - offset, "|%08llX| ", address);

        // print blank spaces if base_address is not a multiple of 16
        for (int j = 0; j < base_address % 16; ++ j) {
            offset += snprintf(line + offset, sizeof(line) - offset, "   ");
            if (j == 7) {
                offset += snprintf(line + offset, sizeof(line) - offset, " ");
            }
        }

        // loop through the next 16 bytes (if they exist) and print the bytes in
        // hex with a leading 0, or leave a blank space if that byte doens't exist
        // for an incomplete line, with an extra space between bytes 8 and 9.
        for (int j = 0; j < 16; ++j) {
            if (i + j < end) {
                offset += snprintf(line + offset, sizeof(line) - offset, " %02X", view[i + j]);
            } else {
                offset += snprintf(line + offset, sizeof(line) - offset, "   ");
            }
            if (j == 7) {
                offset += snprintf(line + offset, sizeof(line) - offset, " ");
            }
        }

        // print 2 spaces and a | to separate ascii representation
        offset += snprintf(line + offset, sizeof(line) - offset, "  |");
        // loop through the same 16 bytes (if they exist) as above, printing the
        // ascii character if it is printable, '.' if it is unprintable, or an empty
        // space if the byte doesn't exist
        unsigned char curr_byte = 0;
        for (int j = 0; j < 16; ++j) {
            if (i + j < end){
                curr_byte = view[i + j];
                if (curr_byte >= 32 && curr_byte <= 126) {
                    offset += snprintf(line + offset, sizeof(line) - offset, "%c", curr_byte);
                } else {
                    offset += snprintf(line + offset, sizeof(line) - offset, ".");
                }
            } else {
                offset += snprintf(line + offset, sizeof(line) - offset, " ");
            }
        }
    }

}

} // namespace LiliumDB