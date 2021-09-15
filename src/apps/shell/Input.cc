/*
 * Copyright (C) 2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <m3/stream/Standard.h>
#include <m3/vfs/Dir.h>

#include <algorithm>
#include <ctype.h>
#include <string>
#include <vector>

#include "Input.h"

using namespace m3;

static std::vector<std::string> history;
static size_t history_pos;
static size_t tab_count;

static std::vector<std::string> get_completions(const char *line, size_t len, size_t *prefix_len) {
    // determine prefix
    size_t prefix_start = len;
    while(prefix_start > 0 && !isspace(line[prefix_start - 1]))
        prefix_start--;
    // check whether we should complete binaries or a path
    bool complete_bins = true;
    for(ssize_t i = static_cast<ssize_t>(prefix_start) - 1; i >= 0; --i) {
        if(line[i] == '|')
            break;
        if(!isspace(line[i])) {
            complete_bins = false;
            break;
        }
    }

    const char *prefix = line + prefix_start;
    *prefix_len = len - prefix_start;
    std::vector<std::string> matches;
    Dir::Entry e;

    if((*prefix || tab_count > 1) && complete_bins) {
        try {
            // we have no PATH, binary directory is hardcoded for now
            Dir bin("/bin");
            while(bin.readdir(e)) {
                if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
                    continue;
                if(!*prefix || strncmp(e.name, prefix, strlen(prefix)) == 0)
                    matches.push_back(e.name);
            }
        }
        catch(const Exception &) {
            // ignore failures
        }
    }

    // since we have no CWD yet, paths have to start with /
    if(*prefix == '/') {
        const char *lastdir = strrchr(prefix, '/');
        const char *filename = lastdir + 1;
        if(*filename || tab_count > 1) {
            std::string dirname(prefix, 0, 1 + static_cast<size_t>(lastdir - prefix));
            try {
                Dir dir(dirname.c_str());
                while(dir.readdir(e)) {
                    if(strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0)
                        continue;
                    if(!*filename || strncmp(e.name, filename, strlen(filename)) == 0)
                        matches.push_back(dirname + e.name);
                }
            }
            catch(const Exception &) {
                // ignore failures
            }
        }
    }

    return matches;
}

static void handle_tab(char *buffer, size_t &o) {
    buffer[o] = '\0';
    size_t prefix_len;
    std::vector<std::string> matches = get_completions(buffer, o, &prefix_len);
    std::sort(matches.begin(), matches.end());
    if(matches.size() == 1) {
        // accept the completion
        for(char c : matches[0].substr(prefix_len)) {
            buffer[o++] = c;
            cout.write(c);
        }
        cout.flush();
    }
    else if(matches.size() > 0) {
        // print all completions
        cout << "\n";
        for(auto &s : matches)
            cout << s.c_str() << " ";
        // and the shell prompt with the current buffer again
        cout << "\n$ " << buffer;
        cout.flush();
    }
}

static void handle_worddel(char *buffer, size_t &o) {
    // walk to the last word end
    for(; o > 0 && isspace(buffer[o - 1]); --o)
        cout.write_all("\b \b", 3);
    // delete this word
    for(; o > 0 && !isspace(buffer[o - 1]); --o)
        cout.write_all("\b \b", 3);
    cout.flush();
}

static void handle_backspace(char *, size_t &o) {
    if(o > 0) {
        // overwrite last byte with a space and delete it
        cout.write_all("\b \b", 3);
        cout.flush();
        o--;
    }
}

static void handle_escape(char *buffer, size_t &o) {
    char c2 = cin.read();
    char c3 = cin.read();

    ssize_t idx = -1;
    // cursor up
    if(c2 == '[' && c3 == 'A') {
        if(history.size() > 0)
            idx = static_cast<ssize_t>((--history_pos) % history.size());
    }
    // cursor down
    else if(c2 == '[' && c3 == 'B') {
        if(history.size() > 0)
            idx = static_cast<ssize_t>((++history_pos) % history.size());
    }
    // just print the escape code
    else {
        buffer[o++] = '^';
        buffer[o++] = c2;
        buffer[o++] = c3;
        cout << "^" << c2 << c3;
        cout.flush();
    }

    if(idx != -1) {
        auto &history_item = history[static_cast<size_t>(idx)];
        cout << "\r";
        // overwrite all including "$ "
        for(size_t i = 0; i < o + 2; ++i)
            cout << " ";
        // replace with item from history
        cout << "\r$ " << history_item.c_str();
        o = history_item.size();
        memcpy(buffer, history_item.c_str(), o);
    }
}

ssize_t Input::readline(char *buffer, size_t max) {
    size_t o = 0;

    // reset state
    history_pos = history.size();
    tab_count = 0;

    // ensure that the line is empty
    buffer[o] = '\0';
    while(o < max) {
        // flush stdout, because cin.read blocks
        cout.flush();

        char c = cin.read();
        // EOF?
        if(c == 0x04)
            return -1;
        // TODO ^C
        if(c == 0x03)
            continue;

        if(c == '\t')
            tab_count += 1;
        else
            tab_count = 0;

        switch(c) {
            case '\t':
                handle_tab(buffer, o);
                break;
            case 0x17:
                handle_worddel(buffer, o);
                break;
            case 0x7F:
                handle_backspace(buffer, o);
                break;
            case 0x1b:
                handle_escape(buffer, o);
                break;

            default: {
                // echo
                if(isprint(c) || c == '\n') {
                    cout.write(c);
                    cout.flush();
                }

                if(isprint(c))
                    buffer[o++] = c;
                break;
            }
        }

        if(c == '\n')
            break;
    }

    buffer[o] = '\0';
    history.push_back(buffer);

    return static_cast<ssize_t>(o);
}