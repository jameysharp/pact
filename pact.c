#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

static void __attribute__((__noreturn__)) oops(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

#define try(e) if(!(e)) oops("error: " #e)

static inline int child_exited(const siginfo_t *info)
{
	const int k = info->si_code;
	return k == CLD_EXITED || k == CLD_KILLED || k == CLD_DUMPED;
}

int main(int argc, char **argv)
{
	const char *const progname = *argv;
	++argv;

	/* Create a new process group that can be managed together later. */
	setpgid(0, 0);

	/* Defer signals until we're ready for them, but don't touch signals
	 * where POSIX says blocking them is undefined behavior. */
	sigset_t signals, default_signals;
	sigfillset(&signals);
	sigdelset(&signals, SIGFPE);
	sigdelset(&signals, SIGILL);
	sigdelset(&signals, SIGSEGV);
	try(sigprocmask(SIG_SETMASK, &signals, &default_signals) == 0);

	int forked = 0;

	while(*argv)
	{
		char **end = argv;
		for(; *end && **end; ++end)
			if(**end == ' ')
				++*end;

		/* Prevent SIGSEGV in execvp. */
		if(argv == end)
		{
			fprintf(stderr, "%s: error: empty command #%d\n", progname, forked + 1);
			try(kill(0, SIGTERM) == 0);
			exit(EXIT_FAILURE);
		}

		if(*end)
		{
			*end = 0;
			++end;
		}

		int child;
		try((child = fork()) >= 0);

		if(child == 0)
		{
			try(sigprocmask(SIG_SETMASK, &default_signals, 0) == 0);
			execvp(argv[0], argv);
			oops(argv[0]);
		}

		++forked;
		argv = end;
	}

	/* This program exits as soon as any child exits. If there are no
	 * children, then it should exit immediately. */
	if(forked == 0)
		return EXIT_SUCCESS;
	
	/* Wait to either receive a signal or to have a child exit. Ignore
	 * SIGCHLD signals which indicate a child was stopped, continued, or
	 * traced. */
	siginfo_t info;
	do {
		try(sigwaitinfo(&signals, &info) >= 0);
	} while(info.si_signo == SIGCHLD && !child_exited(&info));

	/* Stop everything in this process group, including this process
	 * itself. But since this parent process has all signals blocked, this
	 * won't actually cause it to exit. */
	try(kill(0, SIGTERM) == 0);

	/* If the children haven't actually stopped in a few seconds, make real
	 * sure they stop. At this point we only want to know about timeouts
	 * and child exits, so leave all signals blocked but only wait for
	 * those two. */
	alarm(5);
	sigemptyset(&signals);
	sigaddset(&signals, SIGCHLD);
	sigaddset(&signals, SIGALRM);

	int child;
	while((child = waitpid(0, 0, WNOHANG)) >= 0)
	{
		/* If waitpid returns 0, there are still children running, so
		 * let's block until either another SIGCHLD arrives or the
		 * above alarm fires. But if waitpid returned a positive number
		 * then it's just reporting that at least one process has
		 * exited, so don't block in case that was the last one. */
		if(child == 0)
		{
			int signal;
			try((signal = sigwaitinfo(&signals, 0)) >= 0);
			if(signal == SIGALRM)
			{
				fprintf(stderr, "%s: timed out waiting for children to exit, sending SIGKILL\n", progname);
				try(kill(0, SIGKILL) == 0);

				/* unreachable: the parent also received SIGKILL */
			}
		}
	}

	/* All children have exited. Exit with the same status as the child
	 * whose exit first triggered this shutdown. */
	if(info.si_signo == SIGCHLD && info.si_code == CLD_EXITED)
		return info.si_status;

	return EXIT_FAILURE;
}
