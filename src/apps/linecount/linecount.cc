#include <m3/stream/Standard.h>
#include <m3/vfs/Dir.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/VFS.h>

using namespace m3;

static const char *small_file = "/test.txt";
static const int BUF_SIZE = 8;
static const char NEWLINE_CHAR = '\n';
static const char NULL_CHAR = '\0';

int main(int argc, char **argv) {
    auto file = VFS::open(small_file, FILE_R);
    int lines = 0;
    bool last_line_has_newline_char = true;
    char buffer[BUF_SIZE] = {NULL_CHAR};

    while(auto count = file->read(buffer, sizeof(buffer))) {
        int i = BUF_SIZE - 1;
        while(i >= 0 && buffer[i] == NULL_CHAR)
            i--;
        int data_size = (i + 1) < BUF_SIZE ? i + 1 : BUF_SIZE;
        if(!data_size)
            break;

        for(int j = 0; j < data_size; j++) {
            if(buffer[j] == NEWLINE_CHAR)
                lines++;
        }

        // If the last line of the file does not end with '\n' character
        // then we need to add that line to the final answer.
        last_line_has_newline_char = buffer[data_size - 1] == NEWLINE_CHAR;

        for(int i = 0; i < BUF_SIZE; i++)
            buffer[i] = NULL_CHAR;
    }
    if(!last_line_has_newline_char)
        lines++;

    println("No of lines: {}"_cf, lines);

    return 0;
}