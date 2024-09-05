// SPDX-License-Identifier: GPL-2.0
/*
 * Landlock tests - Abstract Unix Socket
 *
 * Copyright © 2024 Tahera Fahimi <fahimitahera@gmail.com>
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "scoped_common.h"

/* Number pending connections queue to be hold. */
const short backlog = 10;

static void create_fs_domain(struct __test_metadata *const _metadata)
{
	int ruleset_fd;
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_READ_DIR,
	};

	ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	EXPECT_LE(0, ruleset_fd)
	{
		TH_LOG("Failed to create a ruleset: %s", strerror(errno));
	}
	EXPECT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
	EXPECT_EQ(0, landlock_restrict_self(ruleset_fd, 0));
	EXPECT_EQ(0, close(ruleset_fd));
}

FIXTURE(scoped_domains)
{
	struct service_fixture stream_address, dgram_address;
};

#include "scoped_base_variants.h"

FIXTURE_SETUP(scoped_domains)
{
	memset(&self->stream_address, 0, sizeof(self->stream_address));
	memset(&self->dgram_address, 0, sizeof(self->dgram_address));
	set_unix_address(&self->stream_address, 0);
	set_unix_address(&self->dgram_address, 1);
}

FIXTURE_TEARDOWN(scoped_domains)
{
}

/*
 * Test unix_stream_connect() and unix_may_send() for a child connecting to its parent,
 * when they have scoped domain or no domain.
 */
TEST_F(scoped_domains, connect_to_parent)
{
	pid_t child;
	bool can_connect_to_parent;
	int err, err_dgram, status;
	int pipe_parent[2];
	int stream_socket, dgram_socket;

	drop_caps(_metadata);
	/*
	 * can_connect_to_parent is true if a child process can connect to its
	 * parent process. This depends on the child process is not isolated from
	 * the parent with a dedicated Landlock domain.
	 */
	can_connect_to_parent = !variant->domain_child;

	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	if (variant->domain_both) {
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);
		if (!__test_passed(_metadata))
			return;
	}

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		char buf_child;

		ASSERT_EQ(0, close(pipe_parent[1]));
		if (variant->domain_child)
			create_scoped_domain(
				_metadata,
				LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

		stream_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		dgram_socket = socket(AF_UNIX, SOCK_DGRAM, 0);

		ASSERT_NE(-1, stream_socket);
		ASSERT_NE(-1, dgram_socket);

		/* wait for the server */
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		err = connect(stream_socket, &self->stream_address.unix_addr,
			      (self->stream_address).unix_addr_len);
		err_dgram = connect(dgram_socket,
				    &self->dgram_address.unix_addr,
				    (self->dgram_address).unix_addr_len);
		if (can_connect_to_parent) {
			EXPECT_EQ(0, err);
			EXPECT_EQ(0, err_dgram);
		} else {
			EXPECT_EQ(-1, err);
			EXPECT_EQ(-1, err_dgram);
			EXPECT_EQ(EPERM, errno);
		}
		ASSERT_EQ(0, close(stream_socket));
		ASSERT_EQ(0, close(dgram_socket));
		_exit(_metadata->exit_code);
		return;
	}
	ASSERT_EQ(0, close(pipe_parent[0]));
	if (variant->domain_parent)
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

	stream_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	dgram_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	ASSERT_NE(-1, stream_socket);
	ASSERT_NE(-1, dgram_socket);
	ASSERT_EQ(0, bind(stream_socket, &self->stream_address.unix_addr,
			  (self->stream_address).unix_addr_len));
	ASSERT_EQ(0, bind(dgram_socket, &self->dgram_address.unix_addr,
			  (self->dgram_address).unix_addr_len));
	ASSERT_EQ(0, listen(stream_socket, backlog));

	/* signal to child that parent is listening */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	ASSERT_EQ(child, waitpid(child, &status, 0));
	ASSERT_EQ(0, close(stream_socket));
	ASSERT_EQ(0, close(dgram_socket));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

/*
 * Test unix_stream_connect() and unix_may_send() for a parent connecting to its child,
 * when they have scoped domain or no domain.
 */
TEST_F(scoped_domains, connect_to_child)
{
	pid_t child;
	bool can_connect_to_child;
	int err, err_dgram, status;
	int pipe_child[2], pipe_parent[2];
	char buf;
	int stream_socket, dgram_socket;

	drop_caps(_metadata);
	/*
	 * can_connect_to_child is true if a parent process can connect to its
	 * child process. The parent process is not isolated from the child
	 * with a dedicated Landlock domain.
	 */
	can_connect_to_child = !variant->domain_parent;

	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	if (variant->domain_both) {
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);
		if (!__test_passed(_metadata))
			return;
	}

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));
		if (variant->domain_child)
			create_scoped_domain(
				_metadata,
				LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

		/* Waits for the parent to be in a domain, if any. */
		ASSERT_EQ(1, read(pipe_parent[0], &buf, 1));

		stream_socket = socket(AF_UNIX, SOCK_STREAM, 0);
		dgram_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
		ASSERT_NE(-1, stream_socket);
		ASSERT_NE(-1, dgram_socket);
		ASSERT_EQ(0,
			  bind(stream_socket, &self->stream_address.unix_addr,
			       (self->stream_address).unix_addr_len));
		ASSERT_EQ(0, bind(dgram_socket, &self->dgram_address.unix_addr,
				  (self->dgram_address).unix_addr_len));
		ASSERT_EQ(0, listen(stream_socket, backlog));
		/* signal to parent that child is listening */
		ASSERT_EQ(1, write(pipe_child[1], ".", 1));
		/* wait to connect */
		ASSERT_EQ(1, read(pipe_parent[0], &buf, 1));
		ASSERT_EQ(0, close(stream_socket));
		ASSERT_EQ(0, close(dgram_socket));
		_exit(_metadata->exit_code);
		return;
	}
	ASSERT_EQ(0, close(pipe_child[1]));
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (variant->domain_parent)
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

	/* Signals that the parent is in a domain, if any. */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	stream_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	dgram_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	ASSERT_NE(-1, stream_socket);
	ASSERT_NE(-1, dgram_socket);

	/* Waits for the child to listen */
	ASSERT_EQ(1, read(pipe_child[0], &buf, 1));
	err = connect(stream_socket, &self->stream_address.unix_addr,
		      (self->stream_address).unix_addr_len);
	err_dgram = connect(dgram_socket, &self->dgram_address.unix_addr,
			    (self->dgram_address).unix_addr_len);
	if (can_connect_to_child) {
		EXPECT_EQ(0, err);
		EXPECT_EQ(0, err_dgram);
	} else {
		EXPECT_EQ(-1, err);
		EXPECT_EQ(-1, err_dgram);
		EXPECT_EQ(EPERM, errno);
	}
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(0, close(stream_socket));
	ASSERT_EQ(0, close(dgram_socket));

	ASSERT_EQ(child, waitpid(child, &status, 0));
	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

FIXTURE(scoped_vs_unscoped_sockets)
{
	struct service_fixture parent_stream_address, parent_dgram_address,
		child_stream_address, child_dgram_address;
};

#include "scoped_multiple_domain_variants.h"

FIXTURE_SETUP(scoped_vs_unscoped_sockets)
{
	memset(&self->parent_stream_address, 0,
	       sizeof(self->parent_stream_address));
	set_unix_address(&self->parent_stream_address, 0);
	memset(&self->parent_dgram_address, 0,
	       sizeof(self->parent_dgram_address));
	set_unix_address(&self->parent_dgram_address, 1);
	memset(&self->child_stream_address, 0,
	       sizeof(self->child_stream_address));
	set_unix_address(&self->child_stream_address, 2);
	memset(&self->child_dgram_address, 0,
	       sizeof(self->child_dgram_address));
	set_unix_address(&self->child_dgram_address, 3);
}

FIXTURE_TEARDOWN(scoped_vs_unscoped_sockets)
{
}

/*
 * Test unix_stream_connect and unix_may_send for parent, child and
 * grand child processes when they can have scoped or non-scoped domains.
 */
TEST_F(scoped_vs_unscoped_sockets, unix_scoping)
{
	pid_t child;
	int status;
	bool can_connect_to_parent, can_connect_to_child;
	int pipe_parent[2];
	int stream_server, dgram_server;

	drop_caps(_metadata);
	can_connect_to_child = (variant->domain_grand_child != SCOPE_SANDBOX);
	can_connect_to_parent = (can_connect_to_child &&
				 (variant->domain_children != SCOPE_SANDBOX));

	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));

	if (variant->domain_all == OTHER_SANDBOX)
		create_fs_domain(_metadata);
	else if (variant->domain_all == SCOPE_SANDBOX)
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		int pipe_child[2];
		pid_t grand_child;

		ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));

		if (variant->domain_children == OTHER_SANDBOX)
			create_fs_domain(_metadata);
		else if (variant->domain_children == SCOPE_SANDBOX)
			create_scoped_domain(
				_metadata,
				LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

		grand_child = fork();
		ASSERT_LE(0, grand_child);
		if (grand_child == 0) {
			char buf;
			int err, dgram_err;
			int stream_client, dgram_client;

			ASSERT_EQ(0, close(pipe_parent[1]));
			ASSERT_EQ(0, close(pipe_child[1]));

			if (variant->domain_grand_child == OTHER_SANDBOX)
				create_fs_domain(_metadata);
			else if (variant->domain_grand_child == SCOPE_SANDBOX)
				create_scoped_domain(
					_metadata,
					LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

			stream_client = socket(AF_UNIX, SOCK_STREAM, 0);
			ASSERT_NE(-1, stream_client);
			dgram_client = socket(AF_UNIX, SOCK_DGRAM, 0);
			ASSERT_NE(-1, dgram_client);

			ASSERT_EQ(1, read(pipe_child[0], &buf, 1));
			err = connect(stream_client,
				      &self->child_stream_address.unix_addr,
				      self->child_stream_address.unix_addr_len);
			dgram_err = connect(
				dgram_client,
				&self->child_dgram_address.unix_addr,
				self->child_dgram_address.unix_addr_len);
			if (can_connect_to_child) {
				EXPECT_EQ(0, err);
				EXPECT_EQ(0, dgram_err);
			} else {
				EXPECT_EQ(-1, err);
				EXPECT_EQ(-1, dgram_err);
				EXPECT_EQ(EPERM, errno);
			}

			EXPECT_EQ(0, close(stream_client));
			stream_client = socket(AF_UNIX, SOCK_STREAM, 0);
			ASSERT_NE(-1, stream_client);

			ASSERT_EQ(1, read(pipe_parent[0], &buf, 1));
			err = connect(
				stream_client,
				&self->parent_stream_address.unix_addr,
				self->parent_stream_address.unix_addr_len);
			dgram_err = connect(
				dgram_client,
				&self->parent_dgram_address.unix_addr,
				self->parent_dgram_address.unix_addr_len);

			if (can_connect_to_parent) {
				EXPECT_EQ(0, err);
				EXPECT_EQ(0, dgram_err);
			} else {
				EXPECT_EQ(-1, err);
				EXPECT_EQ(-1, dgram_err);
				EXPECT_EQ(EPERM, errno);
			}
			EXPECT_EQ(0, close(stream_client));
			EXPECT_EQ(0, close(dgram_client));

			_exit(_metadata->exit_code);
			return;
		}
		ASSERT_EQ(0, close(pipe_child[0]));
		if (variant->domain_child == OTHER_SANDBOX)
			create_fs_domain(_metadata);
		else if (variant->domain_child == SCOPE_SANDBOX)
			create_scoped_domain(
				_metadata,
				LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

		stream_server = socket(AF_UNIX, SOCK_STREAM, 0);
		ASSERT_NE(-1, stream_server);
		dgram_server = socket(AF_UNIX, SOCK_DGRAM, 0);
		ASSERT_NE(-1, dgram_server);

		ASSERT_EQ(0, bind(stream_server,
				  &self->child_stream_address.unix_addr,
				  self->child_stream_address.unix_addr_len));
		ASSERT_EQ(0, bind(dgram_server,
				  &self->child_dgram_address.unix_addr,
				  self->child_dgram_address.unix_addr_len));
		ASSERT_EQ(0, listen(stream_server, backlog));

		ASSERT_EQ(1, write(pipe_child[1], ".", 1));
		ASSERT_EQ(grand_child, waitpid(grand_child, &status, 0));
		ASSERT_EQ(0, close(stream_server))
		ASSERT_EQ(0, close(dgram_server));
		return;
	}
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (variant->domain_parent == OTHER_SANDBOX)
		create_fs_domain(_metadata);
	else if (variant->domain_parent == SCOPE_SANDBOX)
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

	stream_server = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_NE(-1, stream_server);
	dgram_server = socket(AF_UNIX, SOCK_DGRAM, 0);
	ASSERT_NE(-1, dgram_server);
	ASSERT_EQ(0, bind(stream_server, &self->parent_stream_address.unix_addr,
			  self->parent_stream_address.unix_addr_len));
	ASSERT_EQ(0, bind(dgram_server, &self->parent_dgram_address.unix_addr,
			  self->parent_dgram_address.unix_addr_len));

	ASSERT_EQ(0, listen(stream_server, backlog));

	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	ASSERT_EQ(0, close(stream_server));
	ASSERT_EQ(0, close(dgram_server));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

FIXTURE(outside_socket)
{
	struct service_fixture address, transit_address;
};

FIXTURE_VARIANT(outside_socket)
{
	const bool domain_server;
	const bool domain_server_socket;
	const int type;
};

/* clang-format off */
FIXTURE_VARIANT_ADD(outside_socket, allow_dgram_server_sock_domain) {
	/* clang-format on */
	.domain_server = false,
	.domain_server_socket = true,
	.type = SOCK_DGRAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(outside_socket, deny_dgram_server_domain) {
	/* clang-format on */
	.domain_server = true,
	.domain_server_socket = false,
	.type = SOCK_DGRAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(outside_socket, allow_stream_server_sock_domain) {
	/* clang-format on */
	.domain_server = false,
	.domain_server_socket = true,
	.type = SOCK_STREAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(outside_socket, deny_stream_server_domain) {
	/* clang-format on */
	.domain_server = true,
	.domain_server_socket = false,
	.type = SOCK_STREAM,
};

FIXTURE_SETUP(outside_socket)
{
	memset(&self->transit_address, 0, sizeof(self->transit_address));
	set_unix_address(&self->transit_address, 0);
	memset(&self->address, 0, sizeof(self->address));
	set_unix_address(&self->address, 1);
}

FIXTURE_TEARDOWN(outside_socket)
{
}

/*
 * Test unix_stream_connect and unix_may_send for parent and child processes
 * when connecting socket has different domain than the process using it.
 */
TEST_F(outside_socket, socket_with_different_domain)
{
	pid_t child;
	int err, status;
	int pipe_child[2], pipe_parent[2];
	char buf_parent;
	int sock;

	drop_caps(_metadata);
	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		char buf_child;

		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));

		/* client always has domain */
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

		if (variant->domain_server_socket) {
			int data_socket, stream_server;
			int fd_sock = socket(AF_UNIX, variant->type, 0);

			ASSERT_NE(-1, fd_sock);

			stream_server = socket(AF_UNIX, SOCK_STREAM, 0);

			ASSERT_NE(-1, stream_server);
			ASSERT_EQ(0, bind(stream_server,
					  &self->transit_address.unix_addr,
					  self->transit_address.unix_addr_len));
			ASSERT_EQ(0, listen(stream_server, backlog));

			ASSERT_EQ(1, write(pipe_child[1], ".", 1));

			data_socket = accept(stream_server, NULL, NULL);

			ASSERT_EQ(0, send_fd(data_socket, fd_sock));
			ASSERT_EQ(0, close(fd_sock));
			ASSERT_EQ(0, close(stream_server));
		}

		sock = socket(AF_UNIX, variant->type, 0);
		ASSERT_NE(-1, sock);
		/* wait for parent signal for connection */
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		err = connect(sock, &self->address.unix_addr,
			      self->address.unix_addr_len);
		if (!variant->domain_server_socket) {
			EXPECT_EQ(-1, err);
			EXPECT_EQ(EPERM, errno);
		} else {
			EXPECT_EQ(0, err);
		}
		ASSERT_EQ(0, close(sock));
		_exit(_metadata->exit_code);
		return;
	}
	ASSERT_EQ(0, close(pipe_child[1]));
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (!variant->domain_server_socket) {
		sock = socket(AF_UNIX, variant->type, 0);
	} else {
		int cli = socket(AF_UNIX, SOCK_STREAM, 0);

		ASSERT_NE(-1, cli);
		ASSERT_EQ(1, read(pipe_child[0], &buf_parent, 1));
		ASSERT_EQ(0, connect(cli, &self->transit_address.unix_addr,
				     self->transit_address.unix_addr_len));

		sock = recv_fd(cli);
		ASSERT_LE(0, sock);
		ASSERT_EQ(0, close(cli));
	}

	ASSERT_NE(-1, sock);

	if (variant->domain_server)
		create_scoped_domain(_metadata,
				     LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET);

	ASSERT_EQ(0, bind(sock, &self->address.unix_addr,
			  self->address.unix_addr_len));
	if (variant->type == SOCK_STREAM)
		ASSERT_EQ(0, listen(sock, backlog));
	/* signal to child that parent is listening */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	ASSERT_EQ(child, waitpid(child, &status, 0));
	ASSERT_EQ(0, close(sock));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

TEST_HARNESS_MAIN
