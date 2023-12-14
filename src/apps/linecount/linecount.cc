#include <m3/vfs/Dir.h>
#include <m3/vfs/FileRef.h>
#include <m3/vfs/VFS.h>
#include <m3/stream/Standard.h>

using namespace m3;
static const char *small_file = "/test.txt";
int main(int argc, char **argv) {
    char buffer[8*1024];
    auto file = VFS::open(small_file, 1);
    
    auto count=file->read(buffer, sizeof(buffer));

    int lines=0;
    for(int i=0;i<strlen(buffer);i++)
    {
        if((int)buffer[i]==10)
        lines++;
    }
    if(buffer[strlen(buffer)-1]!=10)
    lines++;

    println("No of lines: {}"_cf,lines);

    return 0;
}