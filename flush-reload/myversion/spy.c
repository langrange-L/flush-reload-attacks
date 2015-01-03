#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "args.h"
#include "flushreload.h"

#define MAX_PROBES 20

static struct option long_options[] = {
    { "help",               no_argument,        NULL, 'h' },
    { "elf",                required_argument,  NULL, 'e' },
    { "threshold",          required_argument,  NULL, 't' },
    { "probe",              required_argument,  NULL, 'p' },
    { "machine-readable",   no_argument,        NULL, 'm' },
    { NULL, 0, NULL, 0 }
};

const char *program_name;

void showHelp(const char *msg);
void parseArgs(int argc, char **argv, args_t *args);
void validateArgs(const args_t *args);

int main(int argc, char **argv)
{
    program_name = argv[0];

    args_t args;
    args.probes = malloc(MAX_PROBES * sizeof(probe_t));
    if (args.probes == NULL) {
        perror("Couldn't map probes memory.");
        return EXIT_FAILURE;
    }
    args.probe_count = 0;
    args.elf_path = NULL;
    args.threshold = 120; /* Default, will work for most systems. */
    args.machine_readable = 0;

    parseArgs(argc, argv, &args);
    validateArgs(&args);
    startSpying(&args);

    return EXIT_SUCCESS;
}

void showHelp(const char *msg)
{
    if (msg != NULL) {
        printf("[!] %s\n", msg);
    }
    printf("Usage: %s -e ELFPATH -t CYCLES -p PROBE [-p PROBE ...] [-m]\n", program_name);
    puts("    -e, --elf PATH\t\t\tPath to ELF binary to spy on.");
    puts("    -t, --threshold CYCLES\t\tMax. L3 latency.");
    puts("    -p, --probe N:0xDEADBEEF\t\tName character : Virtual address.");
    puts("    -m, --machine-readable\t\tBinary output.");
}

void parseArgs(int argc, char **argv, args_t *args)
{
    int optChar = 0;
    const char *argstr;
    args->probe_count = 0;
    while ( (optChar = getopt_long(argc, argv, "he:t:p:m", long_options, NULL)) != -1 ) {
        switch (optChar) {
            case 'h':
                /* Help menu. */
                showHelp(NULL);
                exit(EXIT_FAILURE);
            case 'e':
                /* ELF path. */
                args->elf_path = optarg;
                break;
            case 'p':
                /* Probe like A:0x400403 */
                argstr = optarg;

                probe_t *probe = &args->probes[args->probe_count];

                /* Grab the name character from the front. */
                if (strlen(argstr) >= 2 && isalpha(argstr[0]) && argstr[1] == ':') {
                    probe->name = argstr[0];
                } else {
                    showHelp("Give the probe a 1-character name like A:0xDEADBEEF.");
                    exit(EXIT_FAILURE);
                }

                /* Skip over the colon. */
                argstr += 2;

                /* Parse the remainder as an integer in hex. */
                if (sscanf(argstr, "%10li", &probe->virtual_address) != 1 || probe->virtual_address <= 0) {
                    showHelp("Bad probe address.");
                    exit(EXIT_FAILURE);
                }

                args->probe_count++;

                break;
            case 't':
                /* Threshold */
                if (sscanf(optarg, "%10u", &args->threshold) != 1 || args->threshold <= 0) {
                    showHelp("Bad threshold (must be an integer > 0).");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'm':
                args->machine_readable = 1;
                break;
            default:
                showHelp("Invalid argument.");
                exit(EXIT_FAILURE);
        }
    }
}

void validateArgs(const args_t *args)
{
    if (args->elf_path == NULL) {
        showHelp("Tell me what program to spy on with --elf.");
        exit(EXIT_FAILURE);
    }

    if (args->probe_count <= 0) {
        showHelp("Tell me which addresses you want me to probe with --probe.");
        exit(EXIT_FAILURE);
    }

    if ( ! (0 < args->threshold && args->threshold < 2000) ) {
        showHelp("Bad threshold cycles value. Try 120?");
        exit(EXIT_FAILURE);
    }

    /* Check for duplicated probe names or addresses. */
    int i = 0;
    int j = 0;
    for (i = 0; i < args->probe_count; i++) {
        for (j = i + 1; j < args->probe_count; j++) {
            if (args->probes[i].name == args->probes[j].name) {
                showHelp("Two probes share the same name. This is not allowed.");
                exit(EXIT_FAILURE);
            }

            if (args->probes[i].virtual_address == args->probes[j].virtual_address) {
                showHelp("Two probes share the same virtual address. This is not allowed.");
                exit(EXIT_FAILURE);
            }
        }
    }
}