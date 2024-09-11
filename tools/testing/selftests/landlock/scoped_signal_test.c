// SPDX-License-Identifier: GPL-2.0
/*
 * Landlock tests - Signal Scoping
 *
 * Copyright © 2024 Tahera Fahimi <fahimitahera@gmail.com>
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <pthread.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "scoped_common.h"

/* This variable is used for handeling several signals. */
static volatile sig_atomic_t is_signaled;

/* clang-format off */
FIXTURE(scoping_signals) {};
/* clang-format on */

FIXTURE_VARIANT(scoping_signals)
{
	int sig;
};

/* clang-format off */
FIXTURE_VARIANT_ADD(scoping_signals, sigtrap) {
	/* clang-format on */
	.sig = SIGTRAP,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(scoping_signals, sigurg) {
	/* clang-format on */
	.sig = SIGURG,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(scoping_signals, sighup) {
	/* clang-format on */
	.sig = SIGHUP,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(scoping_signals, sigtstp) {
	/* clang-format on */
	.sig = SIGTSTP,
};

FIXTURE_SETUP(scoping_signals)
{
	is_signaled = 0;
}

FIXTURE_TEARDOWN(scoping_signals)
{
}

static void scope_signal_handler(int sig, siginfo_t *info, void *ucontext)
{
	if (sig == SIGTRAP || sig == SIGURG || sig == SIGHUP || sig == SIGTSTP)
		is_signaled = 1;
}

/*
 * In this test, a child process sends a signal to parent before and
 * after getting scoped.
 */
TEST_F(scoping_signals, send_sig_to_parent)
{
	pid_t child;
	pid_t parent = getpid();
	int status;
	struct sigaction action = {
		.sa_sigaction = scope_signal_handler,
		.sa_flags = SA_SIGINFO,

	};

	ASSERT_LE(0, sigaction(variant->sig, &action, NULL));

	/* The process should not have already been signaled. */
	EXPECT_EQ(0, is_signaled);

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		int err;

		/*
		 * The child process can send signal to parent when
		 * domain is not scoped.
		 */
		err = kill(parent, variant->sig);
		ASSERT_EQ(0, err);

		create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);

		/*
		 * The child process cannot send signal to the parent
		 * anymore.
		 */
		err = kill(parent, variant->sig);
		ASSERT_EQ(-1, err);
		ASSERT_EQ(EPERM, errno);

		/*
		 * No matter of the domain, a process should be able to
		 * send a signal to itself.
		 */
		ASSERT_EQ(0, is_signaled);
		ASSERT_EQ(0, raise(variant->sig));
		ASSERT_EQ(1, is_signaled);

		_exit(_metadata->exit_code);
		return;
	}

	while (!is_signaled && !usleep(1))
		;
	ASSERT_EQ(1, is_signaled);

	ASSERT_EQ(child, waitpid(child, &status, 0));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

/* clang-format off */
FIXTURE(scoped_domains) {};
/* clang-format on */

#include "scoped_base_variants.h"

FIXTURE_SETUP(scoped_domains)
{
}

FIXTURE_TEARDOWN(scoped_domains)
{
}

/*
 * This test ensures that a scoped process cannot send signal out of
 * scoped domain.
 */
TEST_F(scoped_domains, check_access_signal)
{
	pid_t child;
	pid_t parent = getpid();
	int status;
	bool can_signal_child, can_signal_parent;
	int pipe_parent[2], pipe_child[2];
	int err;
	char buf;

	can_signal_parent = !variant->domain_child;
	can_signal_child = !variant->domain_parent;

	if (variant->domain_both)
		create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		ASSERT_EQ(0, close(pipe_child[0]));
		ASSERT_EQ(0, close(pipe_parent[1]));

		if (variant->domain_child)
			create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);

		ASSERT_EQ(1, write(pipe_child[1], ".", 1));

		/* Waits for the parent to send signals. */
		ASSERT_EQ(1, read(pipe_parent[0], &buf, 1));

		err = kill(parent, 0);
		if (can_signal_parent) {
			ASSERT_EQ(0, err);
		} else {
			ASSERT_EQ(-1, err);
			ASSERT_EQ(EPERM, errno);
		}
		/*
		 * No matter of the domain, a process should be able to
		 * send a signal to itself.
		 */
		ASSERT_EQ(0, raise(0));

		_exit(_metadata->exit_code);
		return;
	}
	ASSERT_EQ(0, close(pipe_parent[0]));
	if (variant->domain_parent)
		create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);

	ASSERT_EQ(1, read(pipe_child[0], &buf, 1));

	err = kill(child, 0);
	if (can_signal_child) {
		ASSERT_EQ(0, err);
	} else {
		ASSERT_EQ(-1, err);
		ASSERT_EQ(EPERM, errno);
	}
	ASSERT_EQ(0, raise(0));

	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

static int thread_pipe[2];

enum thread_return {
	THREAD_INVALID = 0,
	THREAD_SUCCESS = 1,
	THREAD_ERROR = 2,
};

void *thread_func(void *arg)
{
	char buf;

	if (read(thread_pipe[0], &buf, 1) != 1)
		return (void *)THREAD_ERROR;

	return (void *)THREAD_SUCCESS;
}

TEST(signal_scoping_threads)
{
	pthread_t no_sandbox_thread, scoped_thread;
	enum thread_return ret = THREAD_INVALID;

	ASSERT_EQ(0, pipe2(thread_pipe, O_CLOEXEC));

	ASSERT_EQ(0,
		  pthread_create(&no_sandbox_thread, NULL, thread_func, NULL));
	/* Restrict the domain after creating the first thread. */
	create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);

	ASSERT_EQ(EPERM, pthread_kill(no_sandbox_thread, 0));
	ASSERT_EQ(1, write(thread_pipe[1], ".", 1));

	ASSERT_EQ(0, pthread_create(&scoped_thread, NULL, thread_func, NULL));
	ASSERT_EQ(0, pthread_kill(scoped_thread, 0));
	ASSERT_EQ(1, write(thread_pipe[1], ".", 1));

	EXPECT_EQ(0, pthread_join(no_sandbox_thread, (void **)&ret));
	EXPECT_EQ(THREAD_SUCCESS, ret);
	EXPECT_EQ(0, pthread_join(scoped_thread, (void **)&ret));
	EXPECT_EQ(THREAD_SUCCESS, ret);

	EXPECT_EQ(0, close(thread_pipe[0]));
	EXPECT_EQ(0, close(thread_pipe[1]));
}

// FIXME:
#define SOCKET_PATH "/tmp/unix_sock_test"

const short backlog = 10;

static volatile sig_atomic_t signal_received;

static void handle_sigurg(int sig)
{
	if (sig == SIGURG)
		signal_received = 1;
	else
		signal_received = -1;
}

static int setup_signal_handler(int signal)
{
	struct sigaction sa;

	sa.sa_handler = handle_sigurg;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	return sigaction(SIGURG, &sa, NULL);
}

/*
 * Sending an out of bound message will trigger the SIGURG signal
 * through file_send_sigiotask.
 */
TEST(test_sigurg_socket)
{
	int sock_fd, recv_sock;
	struct sockaddr_un addr, paddr;
	socklen_t size;
	char oob_buf, buffer;
	int status;
	int pipe_parent[2], pipe_child[2];
	pid_t child;

	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", SOCKET_PATH);
	unlink(SOCKET_PATH);
	size = sizeof(addr);

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		oob_buf = '.';

		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));

		sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		ASSERT_NE(-1, sock_fd);

		ASSERT_EQ(1, read(pipe_parent[0], &buffer, 1));
		ASSERT_EQ(0, connect(sock_fd, &addr, sizeof(addr)));

		ASSERT_EQ(1, read(pipe_parent[0], &buffer, 1));
		ASSERT_NE(-1, send(sock_fd, &oob_buf, 1, MSG_OOB));
		ASSERT_EQ(1, write(pipe_child[1], ".", 1));

		EXPECT_EQ(0, close(sock_fd));

		_exit(_metadata->exit_code);
		return;
	}
	ASSERT_EQ(0, close(pipe_parent[0]));
	ASSERT_EQ(0, close(pipe_child[1]));

	sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_NE(-1, sock_fd);
	ASSERT_EQ(0, bind(sock_fd, &addr, size));
	ASSERT_EQ(0, listen(sock_fd, backlog));

	ASSERT_NE(-1, setup_signal_handler(SIGURG));
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	recv_sock = accept(sock_fd, &paddr, &size);
	ASSERT_NE(-1, recv_sock);

	create_scoped_domain(_metadata, LANDLOCK_SCOPED_SIGNAL);

	ASSERT_NE(-1, fcntl(recv_sock, F_SETOWN, getpid()));
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(1, read(pipe_child[0], &buffer, 1));
	ASSERT_EQ(1, recv(recv_sock, &oob_buf, 1, MSG_OOB));

	ASSERT_EQ(1, signal_received);
	EXPECT_EQ(0, close(sock_fd));
	EXPECT_EQ(0, close(recv_sock));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

TEST_HARNESS_MAIN
