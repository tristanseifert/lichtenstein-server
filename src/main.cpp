/**
 * Main entrypoint for Lichtenstein server
 */

#include <glog/logging.h>
#include <gflags/gflags.h>

int main (int argc, char *argv[]) {
	// set up logging
	FLAGS_logtostderr = 1;
	google::InitGoogleLogging(argv[0]);
}
