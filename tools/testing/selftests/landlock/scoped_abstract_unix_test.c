// SPDX-License-Identifier: GPL-2.0
/*
 * Landlock tests - Abstract Unix Socket
 *
 * Copyright © 2017-2020 Mickaël Salaün <mic@digikod.net>
 * Copyright © 2019-2020 ANSSI
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

static void create_unix_domain(struct __test_metadata *const _metadata)
{
	int ruleset_fd;
	const struct landlock_ruleset_attr ruleset_attr = {
		.scoped = LANDLOCK_SCOPED_ABSTRACT_UNIX_SOCKET,
	};

	ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	EXPECT_LE(0, ruleset_fd)
	{
		TH_LOG("Failed to create a ruleset: %s", strerror(errno));
	}
	enforce_ruleset(_metadata, ruleset_fd);
	EXPECT_EQ(0, close(ruleset_fd));
}

/* clang-format off */
FIXTURE(unix_socket)
{
	struct service_fixture stream_address, dgram_address;
	int server, client, dgram_server, dgram_client;
};

/* clang-format on */
FIXTURE_VARIANT(unix_socket)
{
	bool domain_both;
	bool domain_parent;
	bool domain_child;
	bool connect_to_parent;
};

FIXTURE_SETUP(unix_socket)
{
	memset(&self->stream_address, 0, sizeof(self->stream_address));
	memset(&self->dgram_address, 0, sizeof(self->dgram_address));

	set_unix_address(&self->stream_address, 0);
	set_unix_address(&self->dgram_address, 1);
}

FIXTURE_TEARDOWN(unix_socket)
{
	close(self->server);
	close(self->client);
	close(self->dgram_server);
	close(self->dgram_client);
}

/*
 *        No domain
 *
 *   P1-.               P1 -> P2 : allow
 *       \              P2 -> P1 : allow
 *        'P2
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_without_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = false,
	.domain_child = false,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_without_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = false,
	.domain_child = false,
	.connect_to_parent = false,
};

/*
 *        Child domain
 *
 *   P1--.              P1 -> P2 : allow
 *        \             P2 -> P1 : deny
 *        .'-----.
 *        |  P2  |
 *        '------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_one_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = false,
	.domain_child = true,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_with_one_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = false,
	.domain_child = true,
	.connect_to_parent = false,
};

/*
 *        Parent domain
 * .------.
 * |  P1  --.           P1 -> P2 : deny
 * '------'  \          P2 -> P1 : allow
 *            '
 *            P2
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_with_parent_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = true,
	.domain_child = false,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_parent_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = true,
	.domain_child = false,
	.connect_to_parent = false,
};

/*
 *        Parent + child domain (siblings)
 * .------.
 * |  P1  ---.          P1 -> P2 : deny
 * '------'   \         P2 -> P1 : deny
 *         .---'--.
 *         |  P2  |
 *         '------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_sibling_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = true,
	.domain_child = true,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_sibling_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = false,
	.domain_parent = true,
	.domain_child = true,
	.connect_to_parent = false,
};

/*
 *         Same domain (inherited)
 * .-------------.
 * | P1----.     |      P1 -> P2 : allow
 * |        \    |      P2 -> P1 : allow
 * |         '   |
 * |         P2  |
 * '-------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_inherited_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = false,
	.domain_child = false,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_inherited_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = false,
	.domain_child = false,
	.connect_to_parent = false,
};

/*
 *         Inherited + child domain
 * .-----------------.
 * |  P1----.        |  P1 -> P2 : allow
 * |         \       |  P2 -> P1 : deny
 * |        .-'----. |
 * |        |  P2  | |
 * |        '------' |
 * '-----------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_nested_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = false,
	.domain_child = true,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_nested_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = false,
	.domain_child = true,
	.connect_to_parent = false,
};

/*
 *         Inherited + parent domain
 * .-----------------.
 * |.------.         |  P1 -> P2 : deny
 * ||  P1  ----.     |  P2 -> P1 : allow
 * |'------'    \    |
 * |             '   |
 * |             P2  |
 * '-----------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, allow_with_nested_and_parent_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = true,
	.domain_child = false,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_nested_and_parent_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = true,
	.domain_child = false,
	.connect_to_parent = false,
};

/*
 *         Inherited + parent and child domain (siblings)
 * .-----------------.
 * | .------.        |  P1 -> P2 : deny
 * | |  P1  .        |  P2 -> P1 : deny
 * | '------'\       |
 * |          \      |
 * |        .--'---. |
 * |        |  P2  | |
 * |        '------' |
 * '-----------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_forked_domain_connect_to_parent) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = true,
	.domain_child = true,
	.connect_to_parent = true,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_socket, deny_with_forked_domain_connect_to_child) {
	/* clang-format on */
	.domain_both = true,
	.domain_parent = true,
	.domain_child = true,
	.connect_to_parent = false,
};

/*
 * Test UNIX_STREAM_CONNECT and UNIX_MAY_SEND for parent and child,
 * when they have scoped domain or no domain.
 */
TEST_F(unix_socket, abstract_unix_socket)
{
	int status;
	pid_t child;
	bool can_connect_to_parent, can_connect_to_child;
	int err, err_dgram;
	int pipe_child[2], pipe_parent[2];
	char buf_parent;

	/*
	 * can_connect_to_child is true if a parent process can connect to its
	 * child process. The parent process is not isolated from the child
	 * with a dedicated Landlock domain.
	 */
	can_connect_to_child = !variant->domain_parent;
	/*
	 * can_connect_to_parent is true if a child process can connect to its
	 * parent process. This depends on the child process is not isolated from
	 * the parent with a dedicated Landlock domain.
	 */
	can_connect_to_parent = !variant->domain_child;

	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	if (variant->domain_both) {
		create_unix_domain(_metadata);
		if (!__test_passed(_metadata))
			return;
	}

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		char buf_child;

		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));
		if (variant->domain_child)
			create_unix_domain(_metadata);

		/* Waits for the parent to be in a domain, if any. */
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		if (variant->connect_to_parent) {
			self->client = socket(AF_UNIX, SOCK_STREAM, 0);
			self->dgram_client = socket(AF_UNIX, SOCK_DGRAM, 0);

			ASSERT_NE(-1, self->client);
			ASSERT_NE(-1, self->dgram_client);
			ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

			err = connect(self->client,
				      &self->stream_address.unix_addr,
				      (self->stream_address).unix_addr_len);
			err_dgram =
				connect(self->dgram_client,
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
		} else {
			self->server = socket(AF_UNIX, SOCK_STREAM, 0);
			self->dgram_server = socket(AF_UNIX, SOCK_DGRAM, 0);
			ASSERT_NE(-1, self->server);
			ASSERT_NE(-1, self->dgram_server);

			ASSERT_EQ(0,
				  bind(self->server,
				       &self->stream_address.unix_addr,
				       (self->stream_address).unix_addr_len));
			ASSERT_EQ(0, bind(self->dgram_server,
					  &self->dgram_address.unix_addr,
					  (self->dgram_address).unix_addr_len));
			ASSERT_EQ(0, listen(self->server, backlog));

			/* signal to parent that child is listening */
			ASSERT_EQ(1, write(pipe_child[1], ".", 1));
			/* wait to connect */
			ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));
		}
		_exit(_metadata->exit_code);
		return;
	}

	ASSERT_EQ(0, close(pipe_child[1]));
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (variant->domain_parent)
		create_unix_domain(_metadata);

	/* Signals that the parent is in a domain, if any. */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	if (!variant->connect_to_parent) {
		self->client = socket(AF_UNIX, SOCK_STREAM, 0);
		self->dgram_client = socket(AF_UNIX, SOCK_DGRAM, 0);

		ASSERT_NE(-1, self->client);
		ASSERT_NE(-1, self->dgram_client);

		/* Waits for the child to listen */
		ASSERT_EQ(1, read(pipe_child[0], &buf_parent, 1));
		err = connect(self->client, &self->stream_address.unix_addr,
			      (self->stream_address).unix_addr_len);
		err_dgram = connect(self->dgram_client,
				    &self->dgram_address.unix_addr,
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
	} else {
		self->server = socket(AF_UNIX, SOCK_STREAM, 0);
		self->dgram_server = socket(AF_UNIX, SOCK_DGRAM, 0);
		ASSERT_NE(-1, self->server);
		ASSERT_NE(-1, self->dgram_server);
		ASSERT_EQ(0, bind(self->server, &self->stream_address.unix_addr,
				  (self->stream_address).unix_addr_len));
		ASSERT_EQ(0, bind(self->dgram_server,
				  &self->dgram_address.unix_addr,
				  (self->dgram_address).unix_addr_len));
		ASSERT_EQ(0, listen(self->server, backlog));

		/* signal to child that parent is listening */
		ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	}

	ASSERT_EQ(child, waitpid(child, &status, 0));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

enum sandbox_type {
	NO_SANDBOX,
	SCOPE_SANDBOX,
	/* Any other type of sandboxing domain */
	OTHER_SANDBOX,
};

/* clang-format off */
FIXTURE(optional_scoping)
{
	struct service_fixture parent_address, child_address;
	int parent_server, child_server, client;
};

/* clang-format on */
FIXTURE_VARIANT(optional_scoping)
{
	const int domain_all;
	const int domain_parent;
	const int domain_children;
	const int domain_child;
	const int domain_grand_child;
	const int type;
};

FIXTURE_SETUP(optional_scoping)
{
	memset(&self->parent_address, 0, sizeof(self->parent_address));
	memset(&self->child_address, 0, sizeof(self->child_address));

	set_unix_address(&self->parent_address, 0);
	set_unix_address(&self->child_address, 1);
}

FIXTURE_TEARDOWN(optional_scoping)
{
	close(self->parent_server);
	close(self->child_server);
	close(self->client);
}

/*
 * .-----------------.
 * |         ####### |  P3 -> P2 : allow
 * |   P1----# P2  # |  P3 -> P1 : deny
 * |         #  |  # |
 * |         # P3  # |
 * |         ####### |
 * '-----------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, deny_scoped) {
	.domain_all = OTHER_SANDBOX,
	.domain_parent = NO_SANDBOX,
	.domain_children = SCOPE_SANDBOX,
	.domain_child = NO_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_DGRAM,
	/* clang-format on */
};

/*
 * ###################
 * #         ####### #  P3 -> P2 : allow
 * #   P1----# P2  # #  P3 -> P1 : deny
 * #         #  |  # #
 * #         # P3  # #
 * #         ####### #
 * ###################
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, all_scoped) {
	.domain_all = SCOPE_SANDBOX,
	.domain_parent = NO_SANDBOX,
	.domain_children = SCOPE_SANDBOX,
	.domain_child = NO_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_DGRAM,
	/* clang-format on */
};

/*
 * .-----------------.
 * |         .-----. |  P3 -> P2 : allow
 * |   P1----| P2  | |  P3 -> P1 : allow
 * |         |     | |
 * |         | P3  | |
 * |         '-----' |
 * '-----------------'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, allow_with_other_domain) {
	.domain_all = OTHER_SANDBOX,
	.domain_parent = NO_SANDBOX,
	.domain_children = OTHER_SANDBOX,
	.domain_child = NO_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_DGRAM,
	/* clang-format on */
};

/*
 *  .----.    ######   P3 -> P2 : allow
 *  | P1 |----# P2 #   P3 -> P1 : allow
 *  '----'    ######
 *              |
 *              P3
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, allow_with_one_domain) {
	.domain_all = NO_SANDBOX,
	.domain_parent = OTHER_SANDBOX,
	.domain_children = NO_SANDBOX,
	.domain_child = SCOPE_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_DGRAM,
	/* clang-format on */
};

/*
 *  ######    .-----.   P3 -> P2 : allow
 *  # P1 #----| P2  |   P3 -> P1 : allow
 *  ######    '-----'
 *              |
 *              P3
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, allow_with_grand_parent_scoped) {
	.domain_all = NO_SANDBOX,
	.domain_parent = SCOPE_SANDBOX,
	.domain_children = NO_SANDBOX,
	.domain_child = OTHER_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_STREAM,
	/* clang-format on */
};

/*
 *  ######    ######   P3 -> P2 : allow
 *  # P1 #----# P2 #   P3 -> P1 : allow
 *  ######    ######
 *               |
 *             .----.
 *             | P3 |
 *             '----'
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, allow_with_parents_domain) {
	.domain_all = NO_SANDBOX,
	.domain_parent = SCOPE_SANDBOX,
	.domain_children = NO_SANDBOX,
	.domain_child = SCOPE_SANDBOX,
	.domain_grand_child = NO_SANDBOX,
	.type = SOCK_STREAM,
	/* clang-format on */
};

/*
 *  ######		P3 -> P2 : deny
 *  # P1 #----P2	P3 -> P1 : deny
 *  ######     |
 *	       |
 *	     ######
 *           # P3 #
 *           ######
 */
/* clang-format off */
FIXTURE_VARIANT_ADD(optional_scoping, deny_with_self_and_parents_domain) {
	.domain_all = NO_SANDBOX,
	.domain_parent = SCOPE_SANDBOX,
	.domain_children = NO_SANDBOX,
	.domain_child = NO_SANDBOX,
	.domain_grand_child = SCOPE_SANDBOX,
	.type = SOCK_STREAM,
	/* clang-format on */
};

/*
 * Test UNIX_STREAM_CONNECT and UNIX_MAY_SEND for parent, child
 * and grand child processes when they can have scoped or non-scoped
 * domains.
 **/
TEST_F(optional_scoping, unix_scoping)
{
	pid_t child;
	int status;
	bool can_connect_to_parent, can_connect_to_child;
	int pipe_parent[2];

	can_connect_to_child =
		(variant->domain_grand_child == SCOPE_SANDBOX) ? false : true;

	can_connect_to_parent = (!can_connect_to_child ||
				 variant->domain_children == SCOPE_SANDBOX) ?
					false :
					true;

	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));

	if (variant->domain_all == OTHER_SANDBOX)
		create_fs_domain(_metadata);
	else if (variant->domain_all == SCOPE_SANDBOX)
		create_unix_domain(_metadata);

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		int pipe_child[2];

		ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
		pid_t grand_child;

		if (variant->domain_children == OTHER_SANDBOX)
			create_fs_domain(_metadata);
		else if (variant->domain_children == SCOPE_SANDBOX)
			create_unix_domain(_metadata);

		grand_child = fork();
		ASSERT_LE(0, grand_child);
		if (grand_child == 0) {
			ASSERT_EQ(0, close(pipe_parent[1]));
			ASSERT_EQ(0, close(pipe_child[1]));

			char buf1, buf2;
			int err;

			if (variant->domain_grand_child == OTHER_SANDBOX)
				create_fs_domain(_metadata);
			else if (variant->domain_grand_child == SCOPE_SANDBOX)
				create_unix_domain(_metadata);

			self->client = socket(AF_UNIX, variant->type, 0);
			ASSERT_NE(-1, self->client);

			ASSERT_EQ(1, read(pipe_child[0], &buf2, 1));
			err = connect(self->client,
				      &self->child_address.unix_addr,
				      (self->child_address).unix_addr_len);
			if (can_connect_to_child) {
				EXPECT_EQ(0, err);
			} else {
				EXPECT_EQ(-1, err);
				EXPECT_EQ(EPERM, errno);
			}

			if (variant->type == SOCK_STREAM) {
				EXPECT_EQ(0, close(self->client));
				self->client =
					socket(AF_UNIX, variant->type, 0);
				ASSERT_NE(-1, self->client);
			}

			ASSERT_EQ(1, read(pipe_parent[0], &buf1, 1));
			err = connect(self->client,
				      &self->parent_address.unix_addr,
				      (self->parent_address).unix_addr_len);
			if (can_connect_to_parent) {
				EXPECT_EQ(0, err);
			} else {
				EXPECT_EQ(-1, err);
				EXPECT_EQ(EPERM, errno);
			}
			EXPECT_EQ(0, close(self->client));

			_exit(_metadata->exit_code);
			return;
		}

		ASSERT_EQ(0, close(pipe_child[0]));
		if (variant->domain_child == OTHER_SANDBOX)
			create_fs_domain(_metadata);
		else if (variant->domain_child == SCOPE_SANDBOX)
			create_unix_domain(_metadata);

		self->child_server = socket(AF_UNIX, variant->type, 0);
		ASSERT_NE(-1, self->child_server);
		ASSERT_EQ(0, bind(self->child_server,
				  &self->child_address.unix_addr,
				  (self->child_address).unix_addr_len));
		if (variant->type == SOCK_STREAM)
			ASSERT_EQ(0, listen(self->child_server, backlog));

		ASSERT_EQ(1, write(pipe_child[1], ".", 1));
		ASSERT_EQ(grand_child, waitpid(grand_child, &status, 0));
		return;
	}
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (variant->domain_parent == OTHER_SANDBOX)
		create_fs_domain(_metadata);
	else if (variant->domain_parent == SCOPE_SANDBOX)
		create_unix_domain(_metadata);

	self->parent_server = socket(AF_UNIX, variant->type, 0);
	ASSERT_NE(-1, self->parent_server);
	ASSERT_EQ(0, bind(self->parent_server, &self->parent_address.unix_addr,
			  (self->parent_address).unix_addr_len));

	if (variant->type == SOCK_STREAM)
		ASSERT_EQ(0, listen(self->parent_server, backlog));

	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

/*
 * Since the special case of scoping only happens when the connecting socket
 * is scoped, the client's domain is true for all the following test cases.
 */
/* clang-format off */
FIXTURE(unix_sock_special_cases) {
	int server_socket, client;
	int stream_server, stream_client;
	struct service_fixture address, transit_address;
};

/* clang-format on */
FIXTURE_VARIANT(unix_sock_special_cases)
{
	const bool domain_server;
	const bool domain_server_socket;
	const int type;
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_sock_special_cases, allow_dgram_server_sock_domain) {
	/* clang-format on */
	.domain_server = false,
	.domain_server_socket = true,
	.type = SOCK_DGRAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_sock_special_cases, deny_dgram_server_domain) {
	/* clang-format on */
	.domain_server = true,
	.domain_server_socket = false,
	.type = SOCK_DGRAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_sock_special_cases, allow_stream_server_sock_domain) {
	/* clang-format on */
	.domain_server = false,
	.domain_server_socket = true,
	.type = SOCK_STREAM,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(unix_sock_special_cases, deny_stream_server_domain) {
	/* clang-format on */
	.domain_server = true,
	.domain_server_socket = false,
	.type = SOCK_STREAM,
};

FIXTURE_SETUP(unix_sock_special_cases)
{
	memset(&self->transit_address, 0, sizeof(self->transit_address));
	memset(&self->address, 0, sizeof(self->address));
	set_unix_address(&self->transit_address, 0);
	set_unix_address(&self->address, 1);
}

FIXTURE_TEARDOWN(unix_sock_special_cases)
{
	close(self->client);
	close(self->server_socket);
	close(self->stream_server);
	close(self->stream_client);
}

/* Test UNIX_STREAM_CONNECT and UNIX_MAY_SEND for parent and
 * child processes when connecting socket has different domain
 * than the process using it.
 **/
TEST_F(unix_sock_special_cases, socket_with_different_domain)
{
	pid_t child;
	int err, status;
	int pipe_child[2], pipe_parent[2];
	char buf_parent;

	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		char buf_child;

		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));

		/* client always has domain */
		create_unix_domain(_metadata);

		if (variant->domain_server_socket) {
			int data_socket;
			int fd_sock = socket(AF_UNIX, variant->type, 0);

			ASSERT_NE(-1, fd_sock);

			self->stream_server = socket(AF_UNIX, SOCK_STREAM, 0);

			ASSERT_NE(-1, self->stream_server);
			ASSERT_EQ(0,
				  bind(self->stream_server,
				       &self->transit_address.unix_addr,
				       (self->transit_address).unix_addr_len));
			ASSERT_EQ(0, listen(self->stream_server, backlog));

			ASSERT_EQ(1, write(pipe_child[1], ".", 1));

			data_socket = accept(self->stream_server, NULL, NULL);

			ASSERT_EQ(0, send_fd(data_socket, fd_sock));
			ASSERT_EQ(0, close(fd_sock));
		}

		self->client = socket(AF_UNIX, variant->type, 0);
		ASSERT_NE(-1, self->client);
		/* wait for parent signal for connection */
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		err = connect(self->client, &self->address.unix_addr,
			      (self->address).unix_addr_len);
		if (!variant->domain_server_socket) {
			EXPECT_EQ(-1, err);
			EXPECT_EQ(EPERM, errno);
		} else {
			EXPECT_EQ(0, err);
		}
		_exit(_metadata->exit_code);
		return;
	}

	ASSERT_EQ(0, close(pipe_child[1]));
	ASSERT_EQ(0, close(pipe_parent[0]));

	if (!variant->domain_server_socket) {
		self->server_socket = socket(AF_UNIX, variant->type, 0);
	} else {
		int cli = socket(AF_UNIX, SOCK_STREAM, 0);

		ASSERT_NE(-1, cli);
		ASSERT_EQ(1, read(pipe_child[0], &buf_parent, 1));
		ASSERT_EQ(0, connect(cli, &self->transit_address.unix_addr,
				     (self->transit_address).unix_addr_len));

		self->server_socket = recv_fd(cli);
		ASSERT_LE(0, self->server_socket);
		ASSERT_EQ(0, close(cli));
	}

	ASSERT_NE(-1, self->server_socket);

	if (variant->domain_server)
		create_unix_domain(_metadata);

	ASSERT_EQ(0, bind(self->server_socket, &self->address.unix_addr,
			  (self->address).unix_addr_len));
	if (variant->type == SOCK_STREAM)
		ASSERT_EQ(0, listen(self->server_socket, backlog));
	/* signal to child that parent is listening */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	ASSERT_EQ(child, waitpid(child, &status, 0));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

static const char path1[] = TMP_DIR "/s1_variant1";
static const char path2[] = TMP_DIR "/s2_variant1";

/* clang-format off */
FIXTURE(pathname_address_sockets) {
	struct service_fixture stream_address, dgram_address;
};

/* clang-format on */
FIXTURE_VARIANT(pathname_address_sockets)
{
	const int domain;
};

/* clang-format off */
FIXTURE_VARIANT_ADD(pathname_address_sockets, pathname_socket_scoped_domain) {
	/* clang-format on */
	.domain = SCOPE_SANDBOX,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(pathname_address_sockets, pathname_socket_other_domain) {
	/* clang-format on */
	.domain = OTHER_SANDBOX,
};

/* clang-format off */
FIXTURE_VARIANT_ADD(pathname_address_sockets, pathname_socket_no_domain) {
	/* clang-format on */
	.domain = NO_SANDBOX,
};

FIXTURE_SETUP(pathname_address_sockets)
{
	/* setup abstract addresses */
	memset(&self->stream_address, 0, sizeof(self->stream_address));
	set_unix_address(&self->stream_address, 0);

	memset(&self->dgram_address, 0, sizeof(self->dgram_address));
	set_unix_address(&self->dgram_address, 0);

	disable_caps(_metadata);
	umask(0077);
	ASSERT_EQ(0, mkdir(TMP_DIR, 0700));

	ASSERT_EQ(0, mknod(path1, S_IFREG | 0700, 0))
	{
		TH_LOG("Failed to create file \"%s\": %s", path1,
		       strerror(errno));
		ASSERT_EQ(0, unlink(TMP_DIR) & rmdir(TMP_DIR));
	}
	ASSERT_EQ(0, mknod(path2, S_IFREG | 0700, 0))
	{
		TH_LOG("Failed to create file \"%s\": %s", path2,
		       strerror(errno));
		ASSERT_EQ(0, unlink(TMP_DIR) & rmdir(TMP_DIR));
	}
}

FIXTURE_TEARDOWN(pathname_address_sockets)
{
	ASSERT_EQ(0, unlink(path1) & rmdir(path1));
	ASSERT_EQ(0, unlink(path2) & rmdir(path2));
	ASSERT_EQ(0, unlink(TMP_DIR) & rmdir(TMP_DIR));
}

TEST_F(pathname_address_sockets, scoped_pathname_sockets)
{
	const char *const stream_path = path1;
	const char *const dgram_path = path2;
	int srv_fd, srv_fd_dg;
	socklen_t size, size_dg;
	struct sockaddr_un srv_un, srv_un_dg;
	int pipe_parent[2];
	pid_t child;
	int status;
	char buf_child;
	int socket_fds_stream[2];
	int server, client, dgram_server, dgram_client;
	int recv_data;

	ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
				socket_fds_stream));

	srv_un.sun_family = AF_UNIX;
	snprintf(srv_un.sun_path, sizeof(srv_un.sun_path), "%s", stream_path);
	size = offsetof(struct sockaddr_un, sun_path) + strlen(srv_un.sun_path);

	srv_un_dg.sun_family = AF_UNIX;
	snprintf(srv_un_dg.sun_path, sizeof(srv_un_dg.sun_path), "%s",
		 dgram_path);
	size_dg = offsetof(struct sockaddr_un, sun_path) +
		  strlen(srv_un_dg.sun_path);

	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		int cli_fd, cli_fd_dg;
		int err, err_dg;
		int sample = socket(AF_UNIX, SOCK_STREAM, 0);

		ASSERT_LE(0, sample);
		ASSERT_EQ(0, close(pipe_parent[1]));

		/* scope the domain */
		if (variant->domain == SCOPE_SANDBOX)
			create_unix_domain(_metadata);
		else if (variant->domain == OTHER_SANDBOX)
			create_fs_domain(_metadata);

		ASSERT_EQ(0, close(socket_fds_stream[1]));
		ASSERT_EQ(0, send_fd(socket_fds_stream[0], sample));
		ASSERT_EQ(0, close(sample));
		ASSERT_EQ(0, close(socket_fds_stream[0]));

		/* wait for server to listen */
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		/* connect with pathname sockets */
		cli_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		ASSERT_LE(0, cli_fd);
		ASSERT_EQ(0, connect(cli_fd, (struct sockaddr *)&srv_un, size));
		ASSERT_EQ(0, close(cli_fd));

		cli_fd_dg = socket(AF_UNIX, SOCK_DGRAM, 0);
		ASSERT_LE(0, cli_fd_dg);
		ASSERT_EQ(0, connect(cli_fd_dg, (struct sockaddr *)&srv_un_dg,
				     size_dg));

		ASSERT_EQ(0, close(cli_fd_dg));

		/* check connection with abstract sockets */
		client = socket(AF_UNIX, SOCK_STREAM, 0);
		dgram_client = socket(AF_UNIX, SOCK_DGRAM, 0);

		ASSERT_NE(-1, client);
		ASSERT_NE(-1, dgram_client);

		err = connect(client, &self->stream_address.unix_addr,
			      (self->stream_address).unix_addr_len);
		err_dg = connect(dgram_client, &self->dgram_address.unix_addr,
				 (self->dgram_address).unix_addr_len);
		if (variant->domain == SCOPE_SANDBOX) {
			EXPECT_EQ(-1, err);
			EXPECT_EQ(-1, err_dg);
			EXPECT_EQ(EPERM, errno);
		} else {
			EXPECT_EQ(0, err);
			EXPECT_EQ(0, err_dg);
		}
		ASSERT_EQ(0, close(client));
		ASSERT_EQ(0, close(dgram_client));

		_exit(_metadata->exit_code);
		return;
	}

	ASSERT_EQ(0, close(pipe_parent[0]));

	recv_data = recv_fd(socket_fds_stream[1]);
	ASSERT_LE(0, recv_data);
	ASSERT_LE(0, close(socket_fds_stream[1]));

	/* Sets up a server */
	ASSERT_EQ(0, unlink(stream_path));
	srv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ASSERT_LE(0, srv_fd);
	ASSERT_EQ(0, bind(srv_fd, (struct sockaddr *)&srv_un, size));
	ASSERT_EQ(0, listen(srv_fd, 10));

	/* set up a datagram server */
	ASSERT_EQ(0, unlink(dgram_path));
	srv_fd_dg = socket(AF_UNIX, SOCK_DGRAM, 0);
	ASSERT_LE(0, srv_fd_dg);
	size_dg = offsetof(struct sockaddr_un, sun_path) +
		  strlen(srv_un_dg.sun_path);
	ASSERT_EQ(0, bind(srv_fd_dg, (struct sockaddr *)&srv_un_dg, size_dg));

	/*set up abstract servers */
	server = socket(AF_UNIX, SOCK_STREAM, 0);
	dgram_server = socket(AF_UNIX, SOCK_DGRAM, 0);
	ASSERT_NE(-1, server);
	ASSERT_NE(-1, dgram_server);
	ASSERT_EQ(0, bind(server, &self->stream_address.unix_addr,
			  self->stream_address.unix_addr_len));
	ASSERT_EQ(0, bind(dgram_server, &self->dgram_address.unix_addr,
			  self->dgram_address.unix_addr_len));
	ASSERT_EQ(0, listen(server, backlog));

	/* servers are listening, signal to child */
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	ASSERT_EQ(0, close(srv_fd));
	ASSERT_EQ(0, close(srv_fd_dg));
	ASSERT_EQ(0, close(server));
	ASSERT_EQ(0, close(dgram_server));

	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->exit_code = KSFT_FAIL;
}

TEST_HARNESS_MAIN
