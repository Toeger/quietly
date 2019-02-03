#include <algorithm>
#include <array>
#include <fcntl.h>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <pty.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using std::literals::string_view_literals::operator""sv;

static termios get_termios_settings() {
	termios terminal_settings{};
	//TODO: figure out why we cannot simply use cfmakeraw(&terminal_settings);
	terminal_settings.c_iflag = ICRNL | IXOFF | IXON | IUTF8;
	terminal_settings.c_oflag = OPOST | ONLCR;
	terminal_settings.c_cflag = B38400 | CSIZE | CREAD;
	terminal_settings.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;
	const auto cc_data = {VKILL,
						  VREPRINT | VEOL2,
						  127, // why 127?
						  VTIME | VEOL2,
						  VEOF,
						  VINTR,
						  VQUIT,
						  VINTR,
						  VEOL2 | VQUIT,
						  VQUIT | VERASE | VKILL | VEOL2,
						  VERASE | VSTART | VSUSP | VEOL2,
						  VINTR,
						  VERASE | VEOL2,
						  VLNEXT,
						  VQUIT | VERASE | VKILL | VEOF | VTIME | VMIN | VSWTC | VEOL2,
						  VERASE | VEOF | VMIN | VEOL2};
	std::copy(std::begin(cc_data), std::end(cc_data), std::begin(terminal_settings.c_cc));
	terminal_settings.c_ispeed = B38400;
	terminal_settings.c_line = 0;
	terminal_settings.c_ospeed = B38400;
	return terminal_settings;
}

char **get_exec_args(char *argv[]) {
	if (argv[1] == "-sh"sv) {
		//we are supposed to start the program in a shell
		static std::string command;
		for (char **arg = argv + 2; *arg != nullptr; arg++) {
			command += *arg;
			command += ' ';
		}
		command += "| 2>&1";
		static char sh[] = "/bin/sh";
		static char mc[] = "-c";
		static char *args[] = {sh, mc, command.data(), nullptr};
		return args;
	}
	//not a shell, just start the program
	return argv + 1;
}

static void broken_pipe_signal_handler(int) {
	//don't do anything in the handler, it just exists so the program doesn't get killed when reading or writing a pipe fails and instead receives an error code
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		std::cout << "Usage: " << argv[0] << " [program] [args...]\n";
		return 0;
	}
	signal(SIGPIPE, &broken_pipe_signal_handler);
	termios terminal_settings = get_termios_settings();
	winsize window_size;
	std::array<int, 2> file_descriptors;
	if (ioctl(0, TIOCGWINSZ, &window_size) == -1) { //not running in a shell
		if (pipe(file_descriptors.data()) != 0) {
			std::cerr << argv[0] << ": Failed opening regular pipe\n";
			return -1;
		}
	} else { //running in a shell
		if (openpty(&file_descriptors[0], &file_descriptors[1], nullptr, &terminal_settings, &window_size) != 0) {
			std::cerr << argv[0] << ": Failed opening pty pipe\n";
			return -1;
		}
	}

	auto &read_channel = file_descriptors[0];
	auto &write_channel = file_descriptors[1];

	const int child_pid = fork();
	if (child_pid == -1) {
		std::cerr << argv[0] << ": Failed forking\n";
		return -1;
	}
	if (child_pid == 0) { // in child
		if (::close(read_channel) != 0) {
			std::cerr << argv[0] << ": Failed to close file descriptor";
		}
		//TODO: add some error checking
		fcntl(write_channel, F_SETFD, FD_CLOEXEC);
		dup2(write_channel, STDOUT_FILENO);
		dup2(write_channel, STDERR_FILENO);

		auto exec_args = get_exec_args(argv);
		execvp(*exec_args, exec_args);
		exit(-1);
	}

	// in parent
	if (::close(write_channel) != 0) {
		std::cerr << argv[0] << ": Failed to close file descriptor";
	}
	std::string data;
	constexpr auto chunk_size = 1024;
	std::string current_line;
	for (char buffer[chunk_size]; const auto bytes_read = ::read(read_channel, buffer, chunk_size);) {
		if (bytes_read <= 0) {
			::close(read_channel);
			break;
		}
		data.append(buffer, bytes_read);
		for (int i = 0; i < bytes_read; i++) {
			if (buffer[i] == '\n') {
				if (current_line == "\r") {
					current_line.clear();
				}
				if (current_line.empty()) {
					continue; // no point printing empty lines
				}
				if (current_line.back() == '\r') {
					current_line.pop_back();
				}
				std::cout << "\r\033[K" << current_line << std::flush;
			} else {
				std::cout << buffer[i];
			}
		}
	}
	int status;
	while (wait(&status) != child_pid)
		;
	status = WEXITSTATUS(status);
	if (status != 0) {
		std::cout << data;
	}
	return status;
}
