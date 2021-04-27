#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <exception>
#include <string>
#include <regex>
#include <unordered_map>

#define QUEUE_LENGTH 5
#define  BUFFER_SIZE 2048
#define           CR 13
#define           LF 10


const std::regex corr_arr_cell_regex("([a-zA-Z0-9.-/]*)\t([0-9.]*)\t([0-9]*)");
const std::regex start_line_regex("([a-zA-Z0-9-_]+) (((?! ).)+) HTTP/1.1\r\n");
const std::regex header_regex("([a-zA-Z0-9_-]+):[ ]*(((?! ).)+)[ ]*\r\n");
const std::regex request_target_regex("[a-zA-Z0-9.-/]");
const std::regex connection_regex("[Cc][Oo][Nn][Nn][Ee][Cc][Tt][Ii][Oo][Nn]");
const std::regex content_length_regex("[Cc][Oo][Nn][Tt][Ee][Nn][Tt][-][Ll][Ee][Nn][Gg][Tt][Hh]");

using namespace std;
namespace fs = std::filesystem;

class internal_error_ : public std::exception {
public:
    [[nodiscard]] const char *what() const noexcept override {
        return "internal error";
    }
};

class tcp_connection_close : public std::exception {
public:
    [[nodiscard]] const char *what() const noexcept override {
        return "client closed connection";
    }
};

class disconnection_required : public std::exception {
public:
    [[nodiscard]] const char *what() const noexcept override {
        return "disconnection required";
    }
};

class invalid_format : public std::exception {
public:
    [[nodiscard]] const char *what() const noexcept override {
        return "invalid format";
    }
};

// Pobiera nową porcję danych z msg_sock.
// Zapisuje długość pobranych danych oraz ustawia indeks wykorzystania danych na 0.
void read_buffer(char (&buffer)[BUFFER_SIZE], int &buffer_len, int &buffer_index, int msg_sock) {
    buffer_len = read(msg_sock, buffer, sizeof(buffer));
    if (buffer_len < 0) {
        throw disconnection_required();
    } else if (buffer_len == 0) {
        throw tcp_connection_close();
    }
    buffer_index = 0;
}

// Podwaja wielkość tablicy znaków.
// Aktualizuje jej rozmiar.
void resize_string(char **line, size_t &line_maxsize) {
    line_maxsize *= 2;
    *line = (char *) realloc(*line, line_maxsize * sizeof(char));
    if (*line == nullptr) {
        throw internal_error_();
    }
}

// Dynamicznie alokuje tablicę znaków o długości 128.
// Jeśli wcześniej tablica była zadeklarowana, to zwalnia ją.
// Ustala maksymalną pojemność tablicy oraz zeruje jej aktualną zajętość.
void redeclare_128string(char **line, size_t &line_maxsize, size_t &line_size) {
    if (*line != nullptr) free(*line);
    line_maxsize = 128;
    *line = (char *) malloc(line_maxsize * sizeof(char));
    if (*line == nullptr) {
        throw internal_error_();
    }
    line_size = 0;
}

// Wypełnia słownik zasobów.
void complete_corr_arr(string &correlated_str, unordered_map<string, string> &corr_arr) {
    fs::path path_to_file = correlated_str;
    smatch regex_result;
    if (fs::exists(path_to_file)
        && !fs::is_directory(path_to_file)
        && access(path_to_file.c_str(), R_OK) == 0) {
        std::ifstream f(path_to_file, std::ios::in | std::ios::binary);
        for (std::string line; std::getline(f, line);) {
            if (!regex_search(line, regex_result, corr_arr_cell_regex)) {
                exit(EXIT_FAILURE);
            }
            corr_arr.insert({(string) regex_result[1],
                             (string) regex_result[2] + ":" + (string) regex_result[3]});
        }
    }
}

// Sprawdza czy plik 'file' znajduje się w katalogu 'dir'.
bool file_in_directory(fs::path &file_path, string &dir) {
    smatch regex_result;
    fs::path dir_path = dir;
    file_path = fs::canonical(file_path);
    dir_path = fs::canonical(dir_path);
    string inside_dir_str = {dir_path.string()};
    inside_dir_str += "[a-zA-Z0-9.-/]*";
    regex inside_dir(inside_dir_str);
    string file_path_str{file_path.string()};
    return (regex_search(file_path_str, regex_result, regex(inside_dir_str)));
}

// Realizuje zapytanie GET/HEAD klienta na podstawie dostępnych danych
void realize_request(string &method, string &request_target, string &directory,
                     unordered_map<string, string> &corr_arr, int msg_sock) {
    fs::path path_to_file = directory + request_target;
    const auto sz = 128;
    std::string result(sz, '\0'), start_and_headers;
    int snd_len;

    if (fs::exists(path_to_file)
        && !fs::is_directory(path_to_file)
        && access(path_to_file.c_str(), R_OK) == 0) {
        if (!file_in_directory(path_to_file, directory)) {
            start_and_headers = "HTTP/1.1 404 OUTSIDE_THE_ALLOWED_DIRECTORY\r\n\r\n";
            snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
            if (snd_len != start_and_headers.size()) {
                throw disconnection_required();
            }
            return;
        }
        std::ifstream f(path_to_file, std::ios::in | std::ios::binary);
        const auto fsz = fs::file_size(path_to_file);
        start_and_headers =
                "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: "
                + std::to_string(fsz) + "\r\n\r\n\0";
        snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
        if (snd_len != start_and_headers.size()) {
            throw disconnection_required();
        }
        if (method == "GET") {
            while (true) {
                f.read(result.data(), sz);
                if (!f.gcount()) break;
                if (f.gcount() != 128) result[f.gcount()] = '\0';
                snd_len = write(msg_sock, result.c_str(), f.gcount());
                if (snd_len != f.gcount()) {
                    throw disconnection_required();
                }
            }
        }
    } else {
        auto it = corr_arr.find(request_target);
        if (it == corr_arr.end()) {
            start_and_headers = "HTTP/1.1 404 NOT_FOUND\r\n\r\n";
            snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
            if (snd_len != start_and_headers.size()) {
                throw disconnection_required();
            }
        } else {
            start_and_headers = "HTTP/1.1 302 ON_ANOTHER_SERVER\r\nLocation: http://";
            start_and_headers += it->second + request_target + "\r\n\r\n\0";
            snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
            if (snd_len != start_and_headers.size()) {
                throw disconnection_required();
            }
        }
    }
}

int main(int argc, char *argv[]) {

    // Walidacja argumentów.
    if (argc < 3 || argc > 4) {
        exit(EXIT_FAILURE);
    }
    string directory_str = argv[1];

    fs::path directory(directory_str);
    if (!fs::exists(directory)
        || !fs::is_directory(directory)
        || access(argv[1], R_OK) != 0) {
        exit(EXIT_FAILURE);
    }

    string correlated_str = argv[2];
    unordered_map<string, string> corr_arr;
    complete_corr_arr(correlated_str, corr_arr);

    int port_num = (int) (argc == 4 ? strtol(argv[3], nullptr, 10) : 8080);
    if (port_num == 0) {
        exit(EXIT_FAILURE);
    }

    // Łączenie się z klientem.
    int sock, msg_sock;
    struct sockaddr_in server_address{};
    struct sockaddr_in client_address{};
    socklen_t client_address_len = sizeof(client_address);

    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock < 0) {
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port_num); // listening on port PORT_NUM

    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        exit(EXIT_FAILURE);
    }

    if (listen(sock, QUEUE_LENGTH) < 0) {
        exit(EXIT_FAILURE);
    }

    int loaded_lines;       // Liczba rozpatrzonych linii komunikatu.
    bool connection;        // Czy do tej pory pojawił się nagłówek Conection.
    bool val_close;         // Czy pojawiła się wartość close w nagłówku Connection.
    bool content_length;    // Czy do tej pory pojawił się nagłówek Content-Length.
    string method;          // Rozpatrywana metoda.
    string request_target;
    string field_name;
    string field_value;

    char *line = nullptr;   // Treść start-line/header.
    size_t line_maxsize;    // Miejsce zarezerwowane dla 'line' (rozmiar alokacji).
    size_t line_size;       // Realnie zajęte miejsce przez 'line'.
    string line_string;

    char buffer[BUFFER_SIZE];   // Bofor na wczytywane znaki.
    int buffer_index;           // Indeks następnego znaku bofora do rozpatrzenia.
    int buffer_len;             // Liczba znaków wczytanych do bufora.

    bool prev_is_CR;        // Czy poprzedni znak to CR.
    char curr;              // Obecnie rozpatrywany znak.

    smatch regex_result;

    for (;;) { // Pętla na klientów.
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0) {
            cerr << "msg_sock";
            exit(EXIT_FAILURE);
        }
        buffer_index = buffer_len = 0;  // Zerujemy informacje o buforze bo mamy nowego klienta.

        try {
            for (;;) { // Pętla na komunikaty od klienta.
                loaded_lines = 0;
                connection = val_close = content_length = false;

                for (;;) { // Pętla na start-line/nagłówki.
                    redeclare_128string(&line, line_maxsize, line_size);
                    prev_is_CR = false;

                    for (;;) { // Pętla na znaki nagłówka/start-line.
                        if (buffer_index >= buffer_len)
                            read_buffer(buffer, buffer_len, buffer_index, msg_sock);
                        if (line_size >= line_maxsize - 1) {
                            resize_string(&line, line_maxsize);
                        }
                        curr = buffer[buffer_index++];
                        line[line_size++] = curr;
                        if (prev_is_CR && curr == LF) {
                            break; // Koniec start-line/header.
                        }
                        prev_is_CR = (curr == CR);
                    }
                    line[line_size] = '\0';
                    line_string = line;

                    if (loaded_lines == 0) { // start-line
                        if (regex_search(line_string, regex_result, start_line_regex)) {
                            method = regex_result[1];
                            request_target = regex_result[2];
                            if (!regex_search(request_target, regex_result, request_target_regex)) {
                                throw invalid_format();
                            }
                        } else {
                            throw invalid_format();
                        }
                    } else { // header
                        if (line_size == 2) {
                            break; // Koniec nagłówków.
                        }
                        if (regex_search(line_string, regex_result, header_regex)) {
                            field_name = regex_result[1];
                            field_value = regex_result[2];
                            if (regex_search(field_name, regex_result, connection_regex)
                                && regex_result.length(0) == 10) {
                                if (connection) {
                                    throw invalid_format();
                                }
                                connection = true;
                                if (field_value == "close") val_close = true;
                            } else if (regex_search(field_name, regex_result, content_length_regex)
                                       && regex_result.length(0) == 14) {
                                if (content_length || field_value != "0") {
                                    throw invalid_format();
                                }
                                content_length = true;
                            }
                        } else {
                            throw invalid_format();
                        }
                    }
                    ++loaded_lines;
                }
                if (method == "GET" || method == "HEAD") {
                    realize_request(method, request_target, directory_str, corr_arr, msg_sock);
                    if (val_close) {
                        throw disconnection_required();
                    }
                } else {
                    string start_and_headers = "HTTP/1.1 501 NO_SUPPORT_FOR_THE_METHOD\r\n\r\n";
                    int snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
                    if (snd_len != start_and_headers.size())
                        throw disconnection_required();
                }
            }
        }
        catch (const tcp_connection_close &e) {
            if (close(msg_sock) < 0)
                cerr << "close\n";
        }
        catch (const internal_error_ &e) {
            string start_and_headers = "HTTP/1.1 500 INTERNAL_ERROR\r\nConnection: close\r\n\r\n";
            int snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
            if (snd_len != start_and_headers.size())
                cerr << "write to socket\n";
            if (close(msg_sock) < 0)
                cerr << "close\n";
        }
        catch (const invalid_format &e) {
            string start_and_headers = "HTTP/1.1 400 INVALID_FORMAT\r\nConnection: close\r\n\r\n";
            int snd_len = write(msg_sock, start_and_headers.c_str(), start_and_headers.size());
            if (snd_len != start_and_headers.size())
                cerr << "write to socket";
            if (close(msg_sock) < 0)
                cerr << "close";
        }
        catch (const disconnection_required &e) {
            if (close(msg_sock) < 0)
                cerr << "close";
        }
    }
}

