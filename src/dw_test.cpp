#include "DeployWorker.h"

int main(int argc, char const *argv[])
{
	DeployWorker dw;
	
	dw.start();

	dw.deploy({"./hi.sh"});
	
	sleep(128);
	
	dw.stop();
	return 0;
}
