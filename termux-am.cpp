/**
 *    termux-am sends commands to the Termux:API app through a unix socket
 *    Copyright (C) 2021 Tarek Sander
 *
 *    This program is free software: you can redistribute it and/or
 *    modify it under the terms of the GNU General Public License as
 *    published by the Free Software Foundation, version 3.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstdbool>
#include <cstdio>
#include <cstdlib>

#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <array>
#include <string>
#include <regex>

#include "termux-am.h"

void send_blocking(const int fd, const char* data, int len) {
    while (len > 0) {
        int ret = send(fd, data, len, MSG_NOSIGNAL);
        if (ret == -1) {
            perror("Socket write error");
            exit(1);
        }
        len -= ret;
        data += ret;
    }
}


bool recv_part(const int fd, char* data, int len) {
    while (len > 0) {
        int ret = recv(fd, data, 1, 0);
        if (ret == -1) {
            perror("Socket read error");
            exit(1);
        }
        if (*data == '\0' || ret == 0) {
            return true;
        }
        data++;
        len--;
    }
    return false;
}

bool is_number(const std::string& s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

/**
 * Returns a quoted string.
 * Escapes quotes and backslashes, if a quote or backslash is present.
 * Encloses in double quotes if whitespace is present.
 * This is enough for the tokenizer implementation used in the am server.
 */
std::string quote_string(std::string raw) {
    bool whitespace = false;
    std::string processed;
    // account some quoted characters
    processed.reserve(raw.size() + 20);
    for (char c : raw) {
        if (isspace(c)) whitespace = true;
        switch (c) {
            case '"':
                // replace double quotes with escaped double quotes
                processed.push_back('\\');
                processed.push_back('"');
                break;
            case '\'':
                // replace single quotes with escaped single quotes
                processed.push_back('\\');
                processed.push_back('\'');
                break;
            case '\\':
                // double all backslashes
                processed.push_back(c);
            default:
                processed.push_back(c);
        }
    }
    if (whitespace) {
        processed = "\"" + processed + "\"";
    }
    return processed;
}

void print_help() {
    std::cout << help_text;
    const char* server_enabled = getenv("TERMUX_APP__AM_SOCKET_SERVER_ENABLED");
    if (server_enabled != NULL) {
        std::cout << "TERMUX_APP__AM_SOCKET_SERVER_ENABLED=" << server_enabled;
    } else {
        std::cout << "TERMUX_APP__AM_SOCKET_SERVER_ENABLED undefined";
    }
    std::cout << std::endl;
}

/**
 * Handles command line arguments. Returns the correctly escaped am command arguments string if no option caused the program to quit.
 * 
 */
std::string handle_args(int argc, char* argv[]) {
    const option longopts[] = {
        option {
            .name = "help",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'h'
        },
        option {
            .name = "am-help",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'H'
        },
        option {
            .name = "version",
            .has_arg = no_argument,
            .flag = NULL,
            .val = 'V'
        },
        option {
            .name = 0,
            .has_arg = 0,
            .flag = 0,
            .val = 0
        }
    };
    while (true) {
        int ret = getopt_long(argc, argv, "+h", longopts, NULL);
        // end of arguments
        if (ret == -1) break;
        // error is denoted by '?'
        if (ret == '?') {
            print_help();
            exit(1);
        }
        if (ret == 'V') {
            std::cout << TERMUX_AM_VERSION << std::endl;
            exit(0);
        }
        // print local help
        if (ret == 'h') {
            print_help();
            exit(0);
        }
        // print server help
        if (ret == 'H') {
            return "";
        }
    }
    std::string args;
    for (int i = optind; i < argc; i++) {
        args += quote_string(argv[i]);
        args += " ";
    }
    //std::cout << args << std::endl;
    return args;
}


int main(int argc, char* argv[]) {
    // print program help if no argument is specified
    if (argc == 1) {
        print_help();
        exit(1);
    }
    std::string args = handle_args(argc, argv);

    struct sockaddr_un adr = {.sun_family = AF_UNIX};
    if (strlen(SOCKET_PATH) >= sizeof(adr.sun_path)) {
        std::cerr << "Socket path \"" << SOCKET_PATH << "\" too long" << std::endl;
        return 1;
    }

    const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Could not create socket");
        return 1;
    }

    strncpy(adr.sun_path, SOCKET_PATH, sizeof(adr.sun_path)-1);
    if (connect(fd, (struct sockaddr*) &adr, sizeof(adr)) == -1) {
        perror("Could not connect to socket");
        close(fd);
        return 1;
    }

    send_blocking(fd, args.data(), args.size());

    shutdown(fd, SHUT_WR);

    int exit_code;

    {
        char tmp[10];
        memset(tmp, '\0', sizeof(tmp));
        if (!recv_part(fd, tmp, sizeof(tmp) - 1)) {
            std::string errmsg = "Exit code \"" + std::string(tmp) + "\" is too long. It must be valid number between 0-255";
            if (errno == 0)
                std::cerr << errmsg << std::endl;
            else
                perror(errmsg.c_str());
            close(fd);
            return 1;
        }

        // strtol returns 0 if conversion failed and allows leading whitespaces
        errno = 0;
        exit_code = strtol(tmp, NULL, 10);
        if (!is_number(std::string(tmp)) || (exit_code == 0 && std::string(tmp) != "0")) {
            std::cerr << "Exit code \"" + std::string(tmp) + "\" is not a valid number between 0-255" << std::endl;
            close(fd);
            return 1;
        }

        // Out of bound exit codes would return with exit code `44` `Channel number out of range`.
        if (exit_code < 0 || exit_code > 255) {
            std::cerr << "Exit code \"" << std::string(tmp) << "\" is not a valid exit code between 0-255" << std::endl;
            close(fd);
            return 1;
        }
    }

    {
        char tmp[4096];
        bool ret;
        do {
            memset(tmp, '\0', sizeof(tmp));
            ret = recv_part(fd, tmp, sizeof(tmp)-1);
            fputs(tmp, stdout);
        } while (!ret);
    }

    {
        char tmp[4096];
        bool ret;
        do {
            memset(tmp, '\0', sizeof(tmp));
            ret = recv_part(fd, tmp, sizeof(tmp)-1);
            fputs(tmp, stderr);
        } while (!ret);
    }

    close(fd);

    return exit_code;
}
