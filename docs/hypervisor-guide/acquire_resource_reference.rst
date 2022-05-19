.. SPDX-License-Identifier: CC-BY-4.0

Acquire resource reference
==========================

The acquire resource interface enables sharing resources that are specific to a
particular domain and belongs to Xen with a domain with enough privileges,
e.g., Dom0. This domain can map and hence access those resources. General
speaking, this corresponds with Dom0 that accesses resources from other DomU.
Resources are represented by pages that are mapped into user-space. This
document describes the API to build tools to access these resources. The
document also describes the software components required to create and expose a
domain's resource. This is not a tutorial or a how-to guide. It merely
describes the machinery that is already described in the code itself.

.. warning::

    The code in this document may already be out of date, however it may
    be enough to illustrate how the acquire resource interface works.


Tool API
-----------

This section describes the API to map a resource in a user-space tool in dom0.
The API is based on the following functions:

* xenforeignmemory_open()

* xenforeignmemory_resource_size()

* xenforeignmemory_map_resource()

* xenforeignmemory_unmap_resource()

* xenforeignmemory_close()

The ``xenforeignmemory_open()`` function gets the handler that is used by the
rest of the functions:

.. code-block:: c

   fh = xenforeignmemory_open(NULL, 0);

The ``xenforeignmemory_resource_size()`` function gets the size of the
type-specific resource for a given resource, i.e., ``vcpu``:

.. code-block:: c

    rc = xenforeignmemory_resource_size(fh, domid, XENMEM_resource_vmtrace_buf
                                        , vcpu, &size);

The size of the resource is returned in ``size`` in bytes.

The ``xenforeignmemory_map_resource()`` function maps a domain's type-specific
resource. The function is declared as follows:

.. code-block:: c

    xenforeignmemory_resource_handle *xenforeignmemory_map_resource(
        xenforeignmemory_handle *fmem, domid_t domid, unsigned int type,
        unsigned int id, unsigned long frame, unsigned long nr_frames,
        void **paddr, int prot, int flags);

The size of the resource is in number of frames. For example, **QEMU** uses it
to map the ioreq server between the domain and QEMU:

.. code-block:: c

    fres = xenforeignmemory_map_resource(xen_fmem, xen_domid,
                                         XENMEM_resource_ioreq_server,
                                         state->ioservid, 0, 2, &addr,
                                         PROT_READ | PROT_WRITE, 0);

The third parameter corresponds with the resource that we request from the
domain, e.g., ``XENMEM_resource_ioreq_server``. The fourth parameter
corresponds with the type-specific resource, e.g., ``state->ioservid``. The
seventh parameter returns a pointer-to-pointer to the address of the mapped
resource.

The ``xenforeignmemory_unmap_resource()`` function unmaps the resource:

.. code-block:: c
    :caption: tools/misc/xen-vmtrace.c

    if ( fres && xenforeignmemory_unmap_resource(fh, fres) )
        perror("xenforeignmemory_unmap_resource()");

Finally, ``xenforeignmemory_close()`` function is used to close the file's
descriptor:

.. code-block:: c

     rc = xenforeignmemory_close(fh);

Exposing a domain's resource
---------------------------------

In this section, we describe how to build a new resource and expose it to a
guest.  Resources are defined in ``xen/include/public/memory.h``. In Xen-4.16,
there are three resources:

.. code-block:: c
    :caption: xen/include/public/memory.h

    #define XENMEM_resource_ioreq_server 0
    #define XENMEM_resource_grant_table 1
    #define XENMEM_resource_vmtrace_buf 2

The ``resource_max_frames()`` function returns the size of a type-specific
resource. The resource provides a handler to get the size. This is the
definition of the ``resource_max_frame()`` function:

.. code-block:: c
    :linenos:
    :caption: xen/common/memory.c

    static unsigned int resource_max_frames(const struct domain *d,
                                            unsigned int type, unsigned int id)
    {
        switch ( type )
        {
        case XENMEM_resource_grant_table:
            return gnttab_resource_max_frames(d, id);

        case XENMEM_resource_ioreq_server:
            return ioreq_server_max_frames(d);

        case XENMEM_resource_vmtrace_buf:
            return d->vmtrace_size >> PAGE_SHIFT;

        default:
            return -EOPNOTSUPP;
        }
    }

The ``_acquire_resource()`` function invokes the corresponding handler that
maps the resource. The handler relies on ``type`` to select the correct
handler:

.. code-block:: c
    :linenos:
    :caption: xen/common/memory.c

    static int _acquire_resource(
        struct domain *d, unsigned int type, unsigned int id, unsigned int frame,
        unsigned int nr_frames, xen_pfn_t mfn_list[])
    {
        switch ( type )
        {
        case XENMEM_resource_grant_table:
            return gnttab_acquire_resource(d, id, frame, nr_frames, mfn_list);

        case XENMEM_resource_ioreq_server:
            return acquire_ioreq_server(d, id, frame, nr_frames, mfn_list);

        case XENMEM_resource_vmtrace_buf:
            return acquire_vmtrace_buf(d, id, frame, nr_frames, mfn_list);

        default:
            return -EOPNOTSUPP;
        }
    }

Note that if a new resource has to be added, these two functions need to be
modified. These handlers have the common declaration:

.. code-block:: c
    :linenos:
    :caption: xen/common/memory.c

    static int acquire_vmtrace_buf(
        struct domain *d, unsigned int id, unsigned int frame,
        unsigned int nr_frames, xen_pfn_t mfn_list[])
    {

The function shall return in ``mfn_list[]`` a number of ``nr_frames`` of
pointers to mfn pages. These pages are designed to be mapped continuously. For
example, for the ``XENMEM_resource_vmtrace_buf`` resource, the handler is
defined as follows:

.. code-block:: c
    :linenos:
    :caption: xen/common/memory.c

    static int acquire_vmtrace_buf(
        struct domain *d, unsigned int id, unsigned int frame,
        unsigned int nr_frames, xen_pfn_t mfn_list[])
    {
        const struct vcpu *v = domain_vcpu(d, id);
        unsigned int i;
        mfn_t mfn;

        if ( !v )
            return -ENOENT;

        if ( !v->vmtrace.pg ||
             (frame + nr_frames) > (d->vmtrace_size >> PAGE_SHIFT) )
            return -EINVAL;

        mfn = page_to_mfn(v->vmtrace.pg);

        for ( i = 0; i < nr_frames; i++ )
            mfn_list[i] = mfn_x(mfn) + frame + i;

        return nr_frames;
    }

Note that the handler only returns the mfn pages that have been previously
allocated in ``vmtrace.pg``. For this resource, the allocation happens during
the instantiation of the vcpu. A set of pages is allocated during the
instantiation of each vcpu. For allocating the page, we use the domheap with
the ``MEMF_no_refcount`` flag:

.. code-block:: c

    v->vmtrace.pg = alloc_domheap_page(s->target, MEMF_no_refcount);

This allocates a page with the `PGC_allocated` bit set and a single reference
count. Then, we grab a general reference count as well as a writable type
count, so that it does not get used as a special page.

.. code-block:: c

    for ( i = 0; i < (d->vmtrace_size >> PAGE_SHIFT); i++ )
        if ( unlikely(!get_page_and_type(&pg[i], d, PGT_writable_page)) )
            /*
             * The domain can't possibly know about this page yet, so failure
             * here is a clear indication of something fishy going on.
             */
            goto refcnt_err;

Note the case above correspond for a resource that is only available for HVM
guests. When the resource is guest-agnostic, pages would be accessible by
an "owning" PV domain which might be tolerable if it then can't write to the
page. That would require locking the page r/o (from guest pov) by setting
PGT_none instead of PGT_writable_page.

To access the pages in the context of Xen, we map the page by
using:

.. code-block:: c

    va_page = __map_domain_page_global(page);

The ``va_page`` pointer is used in the context of Xen. In Xen, there are two
heaps: the domheap and the xenheap. The distinction between the two is how you
access them. Xenheap is always mapped into Xen, so it is safe to say `d->foo =
alloc_xenheap_page()`; and use the d->foo pointer anywhere in Xen. Domheap is
not (always) mapped, so you must access it's contents using `ptr =
map_domheap_page(); ...; unmap_domheap_page(ptr)`.**. The page is usually
cleaned before to be globally exposed:

.. code-block:: c

    clear_page(va_page);

To release the page, we first unmap the frame with ``unmap_domheap_map()``, and
we drop first the allocation and second the own type count, thus:

.. code-block:: c

    put_page_alloc_ref(pg);
    put_page_and_type(pg);

Note that we cannot free the page until all references have dropped and that
includes the reference from tools in dom0. The ``put_page()`` brings the
reference count to 0 thus automatically freeing the page.

Acquire Resources
-----------------

This section briefly describes the resources that rely on the acquire resource
interface. These resources are mapped by domain tools like QEMU.

Intel Processor Trace (IPT)
```````````````````````````

This resource is named ``XENMEM_resource_vmtrace_buf`` and its size in bytes is
set in ``d->vmtrace_size``. It contains the traces generated by the IPT. These
traces are generated by each vcpu. The pages are allocated during
``vcpu_create()``. The pages are stored in the ``vcpu`` structure in
``sched.h``:

.. code-block:: c

   struct {
        struct page_info *pg; /* One contiguous allocation of d->vmtrace_size */
    } vmtrace;

During ``vcpu_create()``, the pg is allocated by using the per-domain heap:

.. code-block:: c

    pg = alloc_domheap_pages(d, get_order_from_bytes(d->vmtrace_size), MEMF_no_refcount);

For a given vcpu, the page is loaded into the guest at
``vmx_restore_guest_msrs()``:

.. code-block:: c
    :caption: xen/arch/x86/hvm/vmx/vmx.c

    wrmsrl(MSR_RTIT_OUTPUT_BASE, page_to_maddr(v->vmtrace.pg));

The releasing of the pages happens during the vcpu teardown.

Grant Table
```````````

The grant tables are represented by the ``XENMEM_resource_grant_table``
resource. Grant tables are special since guests can map grant tables. Dom0 also
needs to write into the grant table to set up the grants for xenstored and
xenconsoled. When acquiring the resource, the pages are allocated from the xen
heap in ``gnttab_get_shared_frame_mfn()``:

.. code-block:: c
    :linenos:
    :caption: xen/common/grant_table.c

    gt->shared_raw[i] = alloc_xenheap_page()
    share_xen_page_with_guest(virt_to_page(gt->shared_raw[i]), d, SHARE_rw);

Then, pages are shared with the guest. These pages are then converted from virt
to mfn before returning:

.. code-block:: c
    :linenos:

    for ( i = 0; i < nr_frames; ++i )
         mfn_list[i] = virt_to_mfn(vaddrs[frame + i]);

Ioreq server
````````````

The ioreq server is represented by the ``XENMEM_resource_ioreq_server``
resource. An ioreq server provides emulated devices to HVM and PVH guests. The
allocation is done in ``ioreq_server_alloc_mfn()``. The following code partially
shows the allocation of the pages that represent the ioreq server:

.. code-block:: c
    :linenos:
    :caption: xen/common/ioreq.c

    page = alloc_domheap_page(s->target, MEMF_no_refcount);

    iorp->va = __map_domain_page_global(page);
    if ( !iorp->va )
        goto fail;

    iorp->page = page;
    clear_page(iorp->va);
    return 0;

The function above is invoked from ``ioreq_server_get_frame()`` which is called
from ``acquire_ioreq_server()``. For acquiring, the function returns the
allocated pages as follows:

.. code-block:: c

    *mfn = page_to_mfn(s->bufioreq.page);

The ``ioreq_server_free_mfn()`` function releases the pages as follows:

.. code-block:: c
    :linenos:
    :caption: xen/common/ioreq.c

    unmap_domain_page_global(iorp->va);
    iorp->va = NULL;

    put_page_alloc_ref(page);
    put_page_and_type(page);
