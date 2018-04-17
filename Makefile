PROG=		baseline
BINDIR=		/usr/bin

SRCS=		baseline.c config.c common.c session.c objects.c helper.c
SRCS+=		cmd-add.c cmd-branch.c cmd-cat.c cmd-checkout.c cmd-commit.c
SRCS+=		cmd-diff.c cmd-help.c cmd-init.c cmd-ls.c cmd-log.c cmd-version.c
SRCS+=		objdb-fs.c
SRCS+=		dircache-simple.c

MAN=		baseline.1

#CFLAGS+=	-DDEBUG -g 
CFLAGS+=	-g
COPTS+=		-Wall

.include <bsd.prog.mk>

