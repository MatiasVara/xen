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
    xen_vcpu_shmemstats_t *info_shmem;
    xen_shared_vcpustats_t *info;
    struct sigaction act;
    uint32_t seq;
    uint64_t value;

    if ( argc != 4 )
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
        (void **)&info_shmem, PROT_READ, 0);

    if ( !res )
        err(1, "Fail: Map");

    if ( info_shmem->magic != VCPU_STATS_MAGIC )
    {
        fprintf(stderr, "Wrong magic number\n");
        return 1;
    }

    if ( offsetof(struct vcpu_stats, runstate_running_time) > info_shmem->size )
    {
        fprintf(stderr, "The counter is not produced\n");
        return 1;
    }

    info = (xen_shared_vcpustats_t*)((void*)info_shmem
                                     + info_shmem->offset
                                     + info_shmem->size * vcpu);

    if ( info->runstate_running_time & ((uint64_t)1 << 63) )
    {
        fprintf(stderr, "The counter is inactived or has overflowed\n");
        return 1;
    }

    while ( !interrupted )
    {
        sleep(period);
        do {
            seq = info[vcpu].seq;
            /* Ordering is required between the hypevisor and the tool domain */
            xen_rmb();
            value = info[vcpu].runstate_running_time;
            xen_rmb();
        } while ( (info[vcpu].seq & 1) ||
                  (seq != info[vcpu].seq) );
        if ( value & ((uint64_t)1 << 63) )
            break;
        printf("running_vcpu_time[%d]: %ld\n", vcpu, value);
    }

    rc = xenforeignmemory_unmap_resource(fh, res);
    if ( rc )
        err(1, "Fail: Unmap");

    rc = xenforeignmemory_close(fh);
    if ( rc )
        err(1, "Fail: Close");

    return 0;
}
