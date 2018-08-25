#include "mpdas.h"

bool running = true;

void got_signal(int)
{
	running = false;
}

void onclose()
{
	iprintf("%s", "Closing mpdas.");

	if(MPD) delete MPD;
	if(AudioScrobbler) delete AudioScrobbler;
	if(Cache) delete Cache;
}

void setid(const char* username)
{
	passwd* userinfo = 0;

	if(strlen(username) == 0)
		return;

	if(getuid() != 0) {
		eprintf("%s", "You are not root. Not changing user ..");
		return;
	}

	userinfo = getpwnam(username);
	if(!userinfo) {
		eprintf("%s", "The user you specified does not exist.");
		exit(EXIT_FAILURE);
	}

	if(setgid(userinfo->pw_gid) == -1 || setuid(userinfo->pw_uid) == -1) {
		eprintf("%s %s", "Could not switch to user", username);
		exit(EXIT_FAILURE);
	}

	setenv("HOME", userinfo->pw_dir, 1);
}

void printversion()
{
	fprintf(stdout, "mpdas-" VERSION", (C) 2010-2017 Henrik Friedrichsen.\n");
	fprintf(stdout, "Global config path is set to \"%s/mpdasrc\"\n", CONFDIR);
}

void printhelp()
{
	fprintf(stderr, "\nusage: mpdas [-h] [-v] [-c config]\n");

	fprintf(stderr, "\n\th: print this help");
	fprintf(stderr, "\n\tv: print program version");
	fprintf(stderr, "\n\tc: load specified config file");

	fprintf(stderr, "\n");
}

int main(int argc, char* argv[])
{
	int i;
	char* config = 0;
	bool go_daemon = false;

	if(argc >= 2) {
		for(i = 1; i <=  argc-1; i++) {
			if(strstr(argv[i], "-h") == argv[i]) {
				printversion();
				printhelp();
				return EXIT_SUCCESS;
			}
			if(strstr(argv[i], "-v") == argv[i]) {
				printversion();
				return EXIT_SUCCESS;
			}

			else if(strstr(argv[i], "-c") == argv[i]) {
				if(i >= argc-1) {
					fprintf(stderr, "mpdas: config path missing!\n");
					printhelp();
					return EXIT_FAILURE;
				}
				config = argv[i+1];
			}

			else if(strstr(argv[i], "-d") == argv[i]) {
				go_daemon = true;
			}
		}
	}

	atexit(onclose);

	CConfig *cfg = new CConfig(config);

	setid(cfg->Get("runas").c_str());

	// Load config in home dir as well (if possible)
	if(config == 0) {
		std::string home = getenv("HOME");
		std::string xdgconfig = home + "/.config";

		if(getenv("XDG_CONFIG_HOME")) {
			xdgconfig = std::string(getenv("XDG_CONFIG_HOME"));
		}

		std::string path = home + "/.mpdasrc";
		std::string xdgpath = xdgconfig + "/mpdasrc";

		cfg->LoadConfig(xdgpath);
		cfg->LoadConfig(path);
	}

	if(!cfg->gotNecessaryData()) {
		eprintf("%s", "AudioScrobbler username or password not set.");
		return EXIT_FAILURE;
	}

	iprintf("Using %s service URL", cfg->getService() == LastFm ? "Last.fm" : "Libre.fm");

	if (go_daemon) {
		if (daemon(1, 0)) {
			perror("daemon");
			return EXIT_FAILURE;
		}
	}

	MPD = new CMPD(cfg);
	if(!MPD->isConnected())
		return EXIT_FAILURE;

	AudioScrobbler = new CAudioScrobbler(cfg);
	AudioScrobbler->Handshake();
	Cache = new CCache();
	Cache->LoadCache();

	// catch sigint
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = got_signal;
	sigfillset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	while(running) {
		MPD->Update();
		Cache->WorkCache();
		usleep(500000);
	}

	return EXIT_SUCCESS;
}
