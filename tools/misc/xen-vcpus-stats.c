#include <err.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>

#include <xenctrl.h>
#include <xenforeignmemory.h>
#include <xen/vcpu.h>

#define rmb()   asm volatile("lfence":::"memory")

static sig_atomic_t interrupted;
static void close_handler(int signum)
{
    interrupted = 1;
}

int main(int argc, char **argv)
{
    xenforeignmemory_handle *fh;
    xenforeignmemory_resource_handle *res;
    size_t size;
    int rc, domid, period, vcpu;
    xen_shared_vcpustats_t *info;
    struct sigaction act;
    uint32_t version;
    uint64_t value;

    if (argc != 4 )
    {
        fprintf(stderr, "Usage: %s <domid> <vcpu> <period>\n", argv[0]);
        return 1;
    }

    domid = atoi(argv[1]);
    vcpu = atoi(argv[2]);
    period = atoi(argv[3]);

    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    fh = xenforeignmemory_open(NULL, 0);

    if ( !fh )
        err(1, "xenforeignmemory_open");

    rc = xenforeignmemory_resource_size(
        fh, domid, XENMEM_resource_stats_table,
        XENMEM_resource_stats_table_id_vcpustats, &size);

    if ( rc )
        err(1, "Fail: Get size");

    res = xenforeignmemory_map_resource(
        fh, domid, XENMEM_resource_stats_table,
        XENMEM_resource_stats_table_id_vcpustats, 0, size >> XC_PAGE_SHIFT,
        (void **)&info, PROT_READ, 0);

    if ( !res )
        err(1, "Fail: Map");

    while ( !interrupted )
    {
        sleep(period);
        do {
            version = info[vcpu].version;
            rmb();
            value = info[vcpu].runstate_running_time;
            rmb();
        } while ((info[vcpu].version & 1) ||
                (version != info[vcpu].version));
        printf("running_vcpu_time[%d]: %ld\n", vcpu, value);
    }

    rc = xenforeignmemory_unmap_resource(fh, res);
    if ( rc )
        err(1, "Fail: Unmap");

    return 0;
}
