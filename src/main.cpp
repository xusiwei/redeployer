#include "DeployWorker.h"
#include "RuntimeError.h"

#include <limits.h>
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

	for (size_t last = 0;;) {
		auto pos = line.find_first_of(sep, last);
		if (pos != line.npos) {
			v.push_back(line.substr(last, pos - last));
			last = pos + 1;
		} else {
			v.push_back(line.substr(last));
			break;
		}
	}
	return v;
}

bool startwith(std::string text, std::string prefix)
{
	return text.find(prefix) == 0;
}

std::string pwd()
{
	char path[PATH_MAX];
	getcwd(path, sizeof(PATH_MAX));
	return path;
}

int help(const char* prog)
{
	printf("Usage: \n\t%s -c,--cmd=args [-w,--watch=path]\n\n"
		       "    [-c|--cmd]=path\targs\tThe command to execute.\n\n"
		       "    [-w|--watch]=path\tpath\tThe path to monitor.\n\n", prog);
	return 0;
}

int main(int argc, char const* argv[])
{
	bool usage = true;
	std::string path = pwd();
	std::vector<std::string> args;

	if (argc < 3) {
		return help(argv[0]);
	}

	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if ("--cmd" == a || "-c" == a) {
			args = split(argv[++i]);
			usage = false;
		} else if ("--watch" == a || "-w" == a) {
			path = argv[++i];
		} else if (startwith(a, "-c=") || startwith(a, "--cmd=")) {
			args = split(a.substr(a.find('=') + 1));
			usage = false;
		} else if (startwith(a, "-w=") || startwith(a, "--watch=")) {
			path = a.substr(a.find('=') + 1);
		} else if ("--help" == a || "-h") {
			usage = true;
		}
	}

	if (usage) {
		return help(argv[0]);
	}

	printf("path: %s\n", path.c_str());
	printf("args: %zu\n", args.size());
	for (auto &a: args) {
		printf("%s\n", a.c_str());
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
