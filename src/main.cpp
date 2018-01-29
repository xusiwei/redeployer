#include "DeployWorker.h"
#include "RuntimeError.h"

#include <fcntl.h>
#include <signal.h>
#include <vector>
#include <string>

static int sig_pipe[2] = {-1, -1};

void handle_signal(int sig)
{
	write(sig_pipe[1], &sig, sizeof(sig));
}

std::vector<std::string> split(std::string line, char sep = ' ')
{
	std::vector<std::string> v;

	for (size_t last = (size_t) -1; last < line.length(); ) {
		auto pos = line.find_first_of(sep, ++last);
		if (pos != line.npos) {
			v.push_back(line.substr(last, pos - last));
		} else {
			v.push_back(line.substr(last));
		}
		last = pos;
	}
}

int main(int argc, char const *argv[])
{
	std::string path;
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if ("--cmd" == a || "-c" == a) {
			args = split(argv[++i]);
		} else if ("--watch" == a || "-w" == a) {
			path = argv[++i];
		}
	}
	if (argc < 4) {
		printf("Usage: %s -c,--cmd args -w,--watch path\n"
			"    -c,--cmd args    The command to execute.\n"
			"    -w,--watch path  The path to monitor.\n", argv[0]);
		exit(1);
	}

	if (pipe2(sig_pipe, O_CLOEXEC) < 0) {
		throw RuntimeError("pipe2 failed");
	}
	signal(SIGTERM, handle_signal);

	DeployWorker worker;
	worker.start();
	
	worker.deploy(args, path);

	int sig = 0;
	while (read(sig_pipe[0], &sig, sizeof(sig)) >= 0) {
		break;
	}

	worker.stop();
	return 0;
}
