#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>

#include <libpq-fe.h>

#include "libload/libload.h"

static struct
{
        int rows;
        int partitions;
        int clients;
        bool implicitxactions;
        int batchsize;
        bool correlated;
        int randomwritepct;
} opt = {
        .rows = 10000000,
        .partitions = 0,
        .clients = 1,
        .implicitxactions = false,
        .batchsize = 0,
        .correlated = false,
        .randomwritepct = 0,
};
static char *connectString = "";
static bool createInterrupt;

PGconn *pg;

void
usage(const char *name)
{
        fprintf(stderr, "Usage: %s {create,bench}\n", name);
        exit(2);
}

void
dbError(void)
{
        panic("%s", PQerrorMessage(pg));
}

void
dbCheckCmd(PGresult *res)
{
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
                dbError();
        PQclear(res);
}

void
dbConnect(void)
{
        // host=192.168.42.11 to connect to tom
        pg = PQconnectdb(connectString);
        if (PQstatus(pg) != CONNECTION_OK)
                dbError();
}

void
progressShow(int64_t v, int64_t of)
{
        static int64_t lastV = -1;
        if (v == lastV)
                return;
        lastV = v;
        
        const char *bar = "||||||||||||||||||||||||||||||||||||||||||||||||||";
        printf("\r[%-50s] %d%%", bar+50-(v*50/of), v*100/of);
        fflush(stdout);
}

void
progressFinish(void)
{
        progressShow(1, 1);
        printf("\n");
}

void
bulkLoadTable(const char *table, int start, int rows)
{
        PGresult *res;
        char query[128];

        sprintf(query, "DROP TABLE IF EXISTS %s", table);
        dbCheckCmd(PQexec(pg, query));
        sprintf(query, "CREATE TABLE %s (k integer, v integer)", table);
        dbCheckCmd(PQexec(pg, query));

        // Begin bulk load
        sprintf(query, "COPY %s FROM STDIN WITH BINARY", table);
        res = PQexec(pg, query);
        if (PQresultStatus(res) != PGRES_COPY_IN)
                dbError();
        const char hdr[] = "PGCOPY\n\377\r\n\0\0\0\0\0\0\0\0\0";
        if (PQputCopyData(pg, hdr, 19) != 1)
                dbError();

        // Prepare bulk load data
        const int batch = 1024;
        const int rowSize = 2+4+4+4+4;
        char copyBuf[rowSize * batch];
        char *bufPos = copyBuf;
        for (int row = 0; row < batch; ++row, bufPos += rowSize) {
                // Field count
                *(uint16_t*)bufPos = htons(2);
                // Field size
                *(uint32_t*)(bufPos+2) = htonl(4);
                *(uint32_t*)(bufPos+10) = htonl(4);
        }

        // Load
        bufPos = copyBuf;
        for (int32_t row = 0; row < rows; ++row) {
                *(uint32_t*)(bufPos+6) = htonl(row + start);
                *(uint32_t*)(bufPos+14) = rand();
                bufPos += rowSize;
                if (bufPos == copyBuf + rowSize * batch || row == rows - 1) {
                        if (PQputCopyData(pg, copyBuf, bufPos - copyBuf) != 1)
                                dbError();
                        bufPos = copyBuf;
                        progressShow(row + start, opt.rows);
                }
                if (createInterrupt)
                        goto abort;
        }

        // Finish bulk load
        if (PQputCopyEnd(pg, NULL) != 1)
                dbError();
        PQclear(res);
        while ((res = PQgetResult(pg)) != NULL)
          dbCheckCmd(res);

        // Add primary key
        printf("\n");
        sprintf(query, "ALTER TABLE %s ADD PRIMARY KEY (k)", table);
        dbCheckCmd(PQexec(pg, query));
        if (createInterrupt)
                goto abort;

        return;

abort:
        PQexec(pg, "ROLLBACK");
        PQfinish(pg);
        exit(1);
}

void
onCreateInt(int signum)
{
        signal(SIGINT, SIG_DFL);
        createInterrupt = true;
}

void
create(void)
{
        // Set up a signal handler to shut down cleanly on SIGINT
        signal(SIGINT, onCreateInt);

        dbConnect();

        dbCheckCmd(PQexec(pg, "BEGIN"));

        if (opt.partitions == 0) {
                bulkLoadTable("simplebench", 0, opt.rows);
        } else {
                char table[32];
                int rowsPerPart = opt.rows/opt.partitions;
                for (int part = 0; part < opt.partitions; ++part) {
                        sprintf(table, "simplebench%d", part);
                        bulkLoadTable(table, rowsPerPart * part, rowsPerPart);
                }
        }

        progressFinish();

        printf("Committing...\n");
        dbCheckCmd(PQexec(pg, "COMMIT"));
        printf("Analyzing...\n");
        dbCheckCmd(PQexec(pg, "ANALYZE"));
        PQfinish(pg);
}

enum xactionState
{
        NONE,
        READ_WRITE,
        READ_ONLY,
        IMPLICIT,
};

struct queryBuf
{
        char *head, *pos;
        enum xactionState xactionState;
        struct queryBufExpect {
                ExecStatusType status;
                int nTuples;
        } *expectHead, *expect;
};

void
queryBuf_reset(struct queryBuf *buf)
{
        buf->pos = buf->head;
        buf->expect = buf->expectHead;
}

void
queryBuf_init(struct queryBuf *buf, int nQueries)
{
        buf->head = malloc(nQueries * 1024);
        buf->expectHead = malloc(nQueries * sizeof *buf->expectHead);
        if (!buf->head || !buf->expectHead)
                panic("Failed to allocate query buffer");
        buf->xactionState = NONE;
        queryBuf_reset(buf);
}

void
queryBuf_expect(struct queryBuf *buf, ExecStatusType status, int nTuples)
{
        buf->expect->status = status;
        buf->expect->nTuples = nTuples;
        ++buf->expect;
}

void
queryBuf_addQ(struct queryBuf *buf, ExecStatusType status, int nTuples, const char *q)
{
        strcpy(buf->pos, q);
        buf->pos += strlen(q);
        queryBuf_expect(buf, status, nTuples);
}

void
queryBuf_send(struct queryBuf *buf)
{
        PGresult *res;

        if (buf->head == buf->pos)
                return;

        if (PQsendQuery(pg, buf->head) != 1)
                dbError();
        for (struct queryBufExpect *e = buf->expectHead; e < buf->expect; ++e) {
                res = PQgetResult(pg);
                if (PQresultStatus(res) != e->status)
                        dbError();
                if (e->status == PGRES_TUPLES_OK && PQntuples(res) != e->nTuples)
                        panic("Expected %d tuple(s), got %d", e->nTuples, PQntuples(res));
                PQclear(res);
                // Only count queries
                if (e->status == PGRES_TUPLES_OK)
                        addToCount(1);
        }
        if (PQgetResult(pg) != NULL)
                panic("Got more results than expected");
        queryBuf_reset(buf);
}

void
beginXaction(struct queryBuf *buf, enum xactionState typ)
{
        if (buf->xactionState == IMPLICIT)
                return;
        if (buf->xactionState == READ_ONLY && typ == READ_ONLY)
                return;

        if (buf->xactionState != NONE)
                queryBuf_addQ(buf, PGRES_COMMAND_OK, 0, "COMMIT;");
        if (typ == READ_WRITE)
                queryBuf_addQ(buf, PGRES_COMMAND_OK, 0, "BEGIN;");
        else if (typ == READ_ONLY)
                queryBuf_addQ(buf, PGRES_COMMAND_OK, 0, "BEGIN READ ONLY;");
        buf->xactionState = typ;
}

char *
tableOfRow(int32_t k)
{
        static char buf[32];

        if (opt.partitions == 0) {
                if (!buf[0])
                        strcpy(buf, "simplebench");
                return buf;
        }

        int rowsPerPart = opt.rows/opt.partitions;
        sprintf(buf, "simplebench%d", k / rowsPerPart);
        return buf;
}

void
benchWorker(void)
{
        PGresult *res;
        struct queryBuf query;
        char buf[128];

        if (!opt.correlated)
                srand(workerID);

        queryBuf_init(&query, opt.batchsize ? opt.batchsize * 3 : 1);

        dbConnect();

        if (opt.implicitxactions)
                query.xactionState = IMPLICIT;

        if (opt.batchsize == 0) {
                // XXX This path is out of date
                if (opt.randomwritepct)
                        panic("Random write has not been implemented for batchsize 0");
                beginXaction(&query, READ_ONLY);
        }

        /* int nextXaction = rand() % MAX_XACTION_LENGTH; */
        while (!shutdownQueued()) {
                /* 
                 * if (bigXaction && nextXaction < 0) {
                 *         dbCheckCmd(PQexec(pg, "COMMIT"));
                 *         PQreset(pg);
                 *         dbCheckCmd(PQexec(pg, "BEGIN READ ONLY"));
                 *         nextXaction = rand() % MAX_XACTION_LENGTH;
                 * }
                 */
                if (opt.batchsize == 0) {
                        int32_t k = rand() % opt.rows;
                        int32_t kFlip = htonl(k);
                        const char *values[] = {(char*)&kFlip};
                        const int lengths[] = {sizeof k};
                        const int formats[] = {1};
                        sprintf(buf, "SELECT v FROM %s WHERE k = $1", tableOfRow(k));
                        res = PQexecParams(pg, buf, 1, NULL, values, lengths, formats, 0);
                        if (PQresultStatus(res) != PGRES_TUPLES_OK)
                                dbError();
                        if (PQntuples(res) != 1)
                                panic("Expected 1 tuple, got %d", PQntuples(res));
                        PQclear(res);
                        addToCount(1);
                        /* --nextXaction; */
                } else {
                        const char *tmplR = "SELECT v FROM %s WHERE k = %d;";
                        const char *tmplW = "UPDATE %s SET v = %d WHERE k = %d;";
                        for (int i = 0; i < opt.batchsize; ++i) {
                                int32_t k = rand() % opt.rows;
                                int dist = rand() % 100;
                                if (dist < opt.randomwritepct) {
                                        // Random write
                                        beginXaction(&query, READ_WRITE);
                                        query.pos += sprintf(query.pos, tmplW, tableOfRow(k), k, rand());
                                        queryBuf_expect(&query, PGRES_COMMAND_OK, 0);
                                        beginXaction(&query, NONE);
                                } else {
                                        // Random read
                                        beginXaction(&query, READ_ONLY);
                                        query.pos += sprintf(query.pos, tmplR, tableOfRow(k), k);
                                        queryBuf_expect(&query, PGRES_TUPLES_OK, 1);
                                }
                        }

                        queryBuf_send(&query);
                        /* nextXaction -= opt.batchsize; */
                }
        }

        beginXaction(&query, NONE);
        queryBuf_send(&query);

        PQfinish(pg);
}

void
bench(void)
{
        createWorkerProcs(opt.clients, benchWorker);
}

int
main(int argc, char **argv)
{
        int optname;
        while ((optname = getopt(argc, argv, "+p:")) != -1) {
                switch (optname) {
                case 'p':
                        connectString = optarg;
                        break;
                default:
                        usage(argv[0]);
                }
        }

        if (optind >= argc)
                usage(argv[0]);

        const struct benchOpt *opts;
        void (*fn)(void);
        if (strcmp(argv[optind], "create") == 0) {
                static const struct benchOpt copts[] = {
                        { "rows", &opt.rows, BENCHOPT_INT },
                        { "partitions", &opt.partitions, BENCHOPT_INT },
                        { }
                };
                opts = copts;
                fn = create;
        } else if (strcmp(argv[optind], "bench") == 0) {
                static const struct benchOpt copts[] = {
                        { "rows", &opt.rows, BENCHOPT_INT },
                        { "partitions", &opt.partitions, BENCHOPT_INT },
                        { "clients", &opt.clients, BENCHOPT_INT },
                        { "implicitxactions", &opt.implicitxactions, BENCHOPT_BOOL },
                        { "batchsize", &opt.batchsize, BENCHOPT_INT },
                        { "correlated", &opt.correlated, BENCHOPT_BOOL },
                        { "randomwritepct", &opt.randomwritepct, BENCHOPT_INT },
                        { }
                };
                opts = copts;
                fn = bench;
        } else {
                usage(argv[0]);
        }

        if (opt.partitions != 0 && opt.rows % opt.partitions != 0)
                panic("Rows must be a multiple of partitions");

        parseBenchOpts(opts, argc - optind, &argv[optind]);
        fn();

        return 0;
}
