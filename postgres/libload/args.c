#include "libload.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

void
parseBenchOpts(const struct benchOpt *opts, int argc, char * const *argv)
{
        // Construct getopt option array
        int num;
        for (num = 0; opts[num].name; ++num);

        struct option *os = malloc(num * sizeof *os);
        if (!os)
                panic("Failed to allocate option array");

        for (int i = 0; i < num; ++i) {
                os[i].name = opts[i].name;
                if (opts[i].type == BENCHOPT_BOOL)
                        os[i].has_arg = optional_argument;
                else
                        os[i].has_arg = required_argument;
                os[i].flag = NULL;
                os[i].val = 0;
        }

        // Process arguments
        optind = 0;
        int res, opt;
        while ((res = getopt_long(argc, argv, "", os, &opt)) != -1) {
                switch (res) {
                case 0:
                        // Long option
                        switch (opts[opt].type) {
                        case BENCHOPT_BOOL:
                                if (!optarg)
                                        *(bool*)opts[opt].dest = true;
                                else if (strcasecmp(optarg, "true") == 0)
                                        *(bool*)opts[opt].dest = true;
                                else if (strcasecmp(optarg, "false") == 0)
                                        *(bool*)opts[opt].dest = false;
                                else {
                                        fprintf(stderr, "Option '%s' must be 'true' or 'false'\n",
                                                opts[opt].name);
                                        exit(2);
                                }
                                break;

                        case BENCHOPT_INT:
                        {
                                char *end;
                                int val = strtol(optarg, &end, 0);
                                if (*end) {
                                        fprintf(stderr, "Option '%s' requires an integer\n",
                                                opts[opt].name);
                                        exit(2);
                                }
                                *(int*)opts[opt].dest = val;
                                break;
                        }

                        case BENCHOPT_STRING:
                                *(const char**)opts[opt].dest = optarg;
                                break;

                        case BENCHOPT_CHOICES:
                        {
                                int i;
                                const char **choice;
                                for (choice = opts[opt].choices, i = 0;
                                     *choice; ++choice, ++i) {
                                        if (strcasecmp(optarg, *choice) == 0) {
                                                *(int*)opts[opt].dest = i;
                                                i = -1;
                                                break;
                                        }
                                }
                                if (i != -1) {
                                        fprintf(stderr, "Option '%s' must be one of:\n",
                                                opts[opt].name);
                                        for (choice = opts[opt].choices;
                                             *choice; ++choice)
                                                fprintf(stderr, "  %s", *choice);
                                        exit(2);
                                }
                                break;
                        }

                        default:
                                panic("Unknown opt type %d", opts[opt].type);
                        }
                        break;
                case '?':
                        exit(2);
                }
        }

        if (optind < argc) {
                fprintf(stderr, "Unexpected argument '%s'\n", argv[optind]);
                exit(2);
        }

        free(os);

        // Summarize full configuration
        printf("#");
        for (int i = 0; i < num; ++i) {
                printf(" --%s=", opts[i].name);
                switch (opts[i].type) {
                case BENCHOPT_BOOL:
                        if (*(bool*)opts[i].dest)
                                printf("true");
                        else
                                printf("false");
                        break;
                case BENCHOPT_INT:
                        printf("%d", *(int*)opts[i].dest);
                        break;
                case BENCHOPT_STRING:
                        // XXX Escaping?
                        printf("'%s'", *(const char **)opts[i].dest);
                        break;
                case BENCHOPT_CHOICES:
                        printf("'%s'", opts[i].choices[*(int*)opts[i].dest]);
                        break;
                default:
                        panic("Printing of opt type %d not implemented",
                              opts[i].type);
                }
        }
        printf("\n");
}
