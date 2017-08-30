/*
 * Xen event channels (2-level ABI)
 *
 * Jeremy Fitzhardinge <jeremy@xensource.com>, XenSource Inc, 2007
 */

#define pr_fmt(fmt) "xen:" KBUILD_MODNAME ": " fmt

#include <linux/linkage.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>

#include <asm/sync_bitops.h>
#include <asm/xen/hypercall.h>
#include <asm/xen/hypervisor.h>

#include <xen/xen.h>
#include <xen/xen-ops.h>
#include <xen/events.h>
#include <xen/interface/xen.h>
#include <xen/interface/event_channel.h>

#include "events_internal.h"




/* Added by KevinJio */
#include <xen/mctop.h>
#include <linux/signal.h>
#include <linux/sched.h>
/**
The following is the content of mctop_serialization.c
It is the implementation of (mctop_serialization.h) the functions declared in xen/mctop.h
*/

/* Global variables  */
hwc_gs_t* hwcs_grps_array[MAX_HWCS_GRPS_NB];
sibling_t* siblings_array[MAX_SIBLINGS_NB];
int n_hwcs_grps = 0, n_siblings = 0;

/** Serialize a mctop structure to an array of bytes (char)
 * We assume dest to be enough long to contain all the resulting information
 *
 * Fields order: n_levels, latencies, n_hwcs, hwcs, socket_level,
 * n_sockets, has_mem, sockets, is_smt, n_hwc_per_core, node_to_socket,
 * n_siblings, siblings, cache, mem_bandwidths_r, mem_bandwidths1_r,
 * mem_bandwidths_w, mem_bandwidts1_w, pow_info
 */
void serialize_mctop(mctop_t* mctop, char** dest) {
    int len, i, id, flag;
    char* src;
    n_hwcs_grps = 0;
    n_siblings = 0;

    DEBUG_PRINT("serialize_mctop");

    SERIALIZE_FIELD(mctop->n_levels);
    len = mctop->n_levels * sizeof (uint);
    SERIALIZE_ARRAY_FIELD(mctop->latencies, len);
    SERIALIZE_FIELD(mctop->n_hwcs);


    for (i = 0; i < mctop->n_hwcs; i++) {
        serialize_hwc(mctop->hwcs + i, dest);
    }

    SERIALIZE_FIELD(mctop->socket_level);
    SERIALIZE_FIELD(mctop->n_sockets);

    // printf("+++++++   mctop->n_sockets %d\n", mctop->n_sockets);


    SERIALIZE_FIELD(mctop->has_mem);

    for (i = 0; i < mctop->n_sockets; i++) {
        serialize_socket(mctop->sockets + i, dest, mctop->has_mem);
    }


    SERIALIZE_FIELD(mctop->is_smt);
    SERIALIZE_FIELD(mctop->n_hwcs_per_core);

    len = mctop->n_sockets * sizeof (uint);
    SERIALIZE_ARRAY_FIELD(mctop->node_to_socket, len);

    SERIALIZE_FIELD(mctop->n_siblings);
    for (i = 0; i < mctop->n_siblings; i++) {
        SERIALIZE_RELATED_FIELD(*(mctop->siblings + i));
    }

    flag = mctop->cache ? 1 : 0;
    SERIALIZE_FIELD(flag);
    if (flag) {
        serialize_cache_info(mctop->cache, dest);
    }

    if (mctop->has_mem == BANDWIDTH) {
        len = mctop->n_sockets * sizeof (double);
        SERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths_r, len);
        SERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths1_r, len);
        SERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths_w, len);
        SERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths1_w, len);
    }

    flag = mctop->pow_info ? 1 : 0;
    SERIALIZE_FIELD(flag);
    if (flag) {
        SERIALIZE_FIELD(*(mctop->pow_info));
    }

    DEBUG_PRINT("End of serialize_mctop");
}

/**
 * Deserialize mctop
 */
mctop_t* deserialize_mctop(mctop_t* mctop, char** src) {
    int len, i, id, flag;
    char* dest;
    hw_context_t* hwc;
    socket_t* socket;
    sibling_t* sibling;

    DEBUG_PRINT("deserialize_mctop");
    init_null_ptrs_of_mctop(mctop);

    DESERIALIZE_FIELD(mctop->n_levels);
    len = mctop->n_levels * sizeof (uint);
    DESERIALIZE_ARRAY_FIELD(mctop->latencies, len);
    DESERIALIZE_FIELD(mctop->n_hwcs);

    mctop->hwcs = (hw_context_t*) kmalloc(mctop->n_hwcs * sizeof (hw_context_t), GFP_KERNEL);
    for (i = 0; i < mctop->n_hwcs; i++) {
        deserialize_hwc(mctop->hwcs + i, src);
    }

    DESERIALIZE_FIELD(mctop->socket_level);
    DESERIALIZE_FIELD(mctop->n_sockets);
    DESERIALIZE_FIELD(mctop->has_mem);

    mctop->sockets = (socket_t*) kmalloc(mctop->n_sockets * sizeof (socket_t), GFP_KERNEL);
    for (i = 0; i < mctop->n_sockets; i++) {
        deserialize_socket(mctop->sockets + i, src, mctop->has_mem);
    }

    DESERIALIZE_FIELD(mctop->is_smt);
    DESERIALIZE_FIELD(mctop->n_hwcs_per_core);

    len = mctop->n_sockets * sizeof (uint);
    DESERIALIZE_ARRAY_FIELD(mctop->node_to_socket, len);

    DESERIALIZE_FIELD(mctop->n_siblings);
    mctop->siblings = (sibling_t**) kmalloc(mctop->n_siblings * sizeof (sibling_t*), GFP_KERNEL);
    for (i = 0; i < mctop->n_siblings; i++) {
        deserialize_sibling(*(mctop->siblings + i), src);
    }

    DESERIALIZE_FIELD(flag);
    if (flag) {
        deserialize_cache_info(mctop->cache, src);
    } else {
    	mctop->cache = NULL;
    }

    if (mctop->has_mem == BANDWIDTH) {
        len = mctop->n_sockets * sizeof (double);
        DESERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths_r, len);
        DESERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths1_r, len);
        DESERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths_w, len);
        DESERIALIZE_ARRAY_FIELD(mctop->mem_bandwidths1_w, len);
    }


    DESERIALIZE_FIELD(flag);
    if (flag) {
        DESERIALIZE_FIELD(*(mctop->pow_info));
    } else {
    	mctop->pow_info = NULL;
    }


    /* Re-establishment of the pre-existing links */
    for (i = 0; i < mctop->n_hwcs; i++) {
        hwc = mctop->hwcs + i;
        GET_ID_IN_FIELD(hwc->socket);
        hwc->socket = get_socket_by_id(mctop->sockets, mctop->n_sockets, id);
        GET_ID_IN_FIELD(hwc->parent);
        hwc->parent = get_hwc_gs_by_id_2(hwcs_grps_array, n_hwcs_grps, id);
        GET_ID_IN_FIELD(hwc->next);
        hwc->next = get_hwc_by_id(mctop->hwcs, mctop->n_hwcs, id);

        // printf("++++++++++++++++++++hwc %p\n", hwc);
        // printf("++++++++++++++++++++hwc->socket %p\n", hwc->socket);
        // printf("++++++++++++++++++++hwc->parent %p\n", hwc->parent);
        // printf("++++++++++++++++++++hwc->next %p\n", hwc->next);
    }

    for (i = 0; i < mctop->n_sockets; i++) {
        socket = mctop->sockets + i;
        reestablish_links_for_socket(socket, mctop);
    }

    for (i = 0; i < mctop->n_siblings; i++) {
        sibling = mctop->siblings[i];
        deserialize_sibling(sibling, src);

        GET_ID_IN_FIELD(sibling->left);
        sibling->left = get_hwc_gs_by_id_2(hwcs_grps_array, n_hwcs_grps, id);
        GET_ID_IN_FIELD(sibling->right);
        sibling->right = get_hwc_gs_by_id_2(hwcs_grps_array, n_hwcs_grps, id);
        GET_ID_IN_FIELD(sibling->next);
        sibling->next = get_sibling_by_id_2(mctop->siblings, mctop->n_siblings, id);
    }

    DEBUG_PRINT("End of deserialize_mctop");

    return mctop;
}

/**
 * Serialize a socket
 * Fields order: id, level, type, latency, socket_id, parent_id, next_id, n_hwcs,
 * hwcs_ids, n_cores, n_children, n_siblings, siblings_ids, siblings_in_ids,
 * local_node_wrong, local_node, n_nodes, mem_latencies, mem_bandwidths_r,
 * mem_bandwidths1_r, mem_bandwidths_w, mem_bandwidts1_w, pow_info.
 * pow_info is preceeded by a boolean (0/1) flag to indicate if it exists or not 
 */
void serialize_socket(socket_t* socket, char** dest, uint has_mem) {
    int len, id, i, flag;
    char* src;

    DEBUG_PRINT("serialize_socket");

    SERIALIZE_FIELD(socket->id);
    SERIALIZE_FIELD(socket->level);
    SERIALIZE_FIELD(socket->type);
    SERIALIZE_FIELD(socket->latency);

    // printf("socket->socket %p\n", socket->socket);
    // if(socket->socket){
    // 	printf("socket->socket->id %d\n", socket->socket->id);
    // }
    SERIALIZE_RELATED_FIELD(socket->socket);
    SERIALIZE_RELATED_FIELD(socket->parent);
    SERIALIZE_RELATED_FIELD(socket->next);

    SERIALIZE_FIELD(socket->n_hwcs);

    for (i = 0; i < socket->n_hwcs; i++) {
        SERIALIZE_RELATED_FIELD(*(socket->hwcs + i));
    }

    SERIALIZE_FIELD(socket->n_cores);
    SERIALIZE_FIELD(socket->n_children);

    SERIALIZE_FIELD(socket->mem_size);
    SERIALIZE_FIELD(socket->nr_mem_ranges_elts);
    if(socket->nr_mem_ranges_elts){
        len = socket->nr_mem_ranges_elts * sizeof(*socket->mem_ranges);
        SERIALIZE_ARRAY_FIELD(socket->mem_ranges, len);
    }

    // printf("--------    socket->n_children ==> %d\n", socket->n_children);
    // printf("--------    socket->n_cores ==> %d\n", socket->n_cores);
    // printf("--------    socket->n_hwcs ==> %d\n", socket->n_hwcs);

    if (!socket->n_children) {
    	DEBUG_PRINT("End of serialize_socket");
        return;
    }

    for (i = 0; i < socket->n_children; i++) {
        // SERIALIZE_RELATED_FIELD(*(socket->children + i));
        serialize_socket(*(socket->children + i), dest, has_mem);
    }

    SERIALIZE_FIELD(socket->n_siblings);
    for (i = 0; i < socket->n_siblings; i++) {
        SERIALIZE_RELATED_FIELD(*(socket->siblings + i));
    }
    for (i = 0; i < socket->n_siblings; i++) {
        SERIALIZE_RELATED_FIELD(*(socket->siblings_in + i));
    }

    SERIALIZE_FIELD(socket->local_node_wrong);
    SERIALIZE_FIELD(socket->local_node);
    SERIALIZE_FIELD(socket->n_nodes);

    len = socket->n_nodes * sizeof (uint);
    SERIALIZE_ARRAY_FIELD(socket->mem_latencies, len);

    if (has_mem == BANDWIDTH) {
        len = socket->n_nodes * sizeof (double);
        SERIALIZE_ARRAY_FIELD(socket->mem_bandwidths_r, len);
        SERIALIZE_ARRAY_FIELD(socket->mem_bandwidths1_r, len);
        SERIALIZE_ARRAY_FIELD(socket->mem_bandwidths_w, len);
        SERIALIZE_ARRAY_FIELD(socket->mem_bandwidths1_w, len);
    }

    flag = socket->pow_info ? 1 : 0;
    SERIALIZE_FIELD(flag);
    if (flag) {
        SERIALIZE_FIELD(*(socket->pow_info));
    }

    DEBUG_PRINT("End of serialize_socket");
}

/**
 * Deserialize a socket
 */
socket_t* deserialize_socket(socket_t* socket, char** src, uint has_mem) {
    int len, id, i, flag;
    char* dest;

    DEBUG_PRINT("deserialize_socket");
    init_null_ptrs_of_socket(socket);

    DESERIALIZE_FIELD(socket->id);
    DESERIALIZE_FIELD(socket->level);
    DESERIALIZE_FIELD(socket->type);
    DESERIALIZE_FIELD(socket->latency);

    DESERIALIZE_FIELD(id);
    *((int*) &socket->socket) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &socket->parent) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &socket->next) = id;

    // DESERIALIZE_FIELD(socket->n_hwcs);

    // if(socket->level == 1){
    // 	printf("Manual deserialization on level 1\n");
    //     socket->n_hwcs = 2;
    //     *src += sizeof(int);       
    //     // DESERIALIZE_FIELD(socket->n_hwcs);
    // } else if(socket->level == 0){
    // 	printf("Manual deserialization on level 0\n");
    //     socket->n_hwcs = 0;
    //     *src += sizeof(int);       
    //     // DESERIALIZE_FIELD(socket->n_hwcs);
    // } else {
    //     DESERIALIZE_FIELD(socket->n_hwcs);
    // }
    DESERIALIZE_FIELD(socket->n_hwcs);

    socket->hwcs = (hw_context_t**) kmalloc(socket->n_hwcs * sizeof (hw_context_t*), GFP_KERNEL);
    for (i = 0; i < socket->n_hwcs; i++) {
        DESERIALIZE_FIELD(id);
        *((int*) &socket->hwcs[i]) = id;
    }

    DESERIALIZE_FIELD(socket->n_cores);
    DESERIALIZE_FIELD(socket->n_children);

    DESERIALIZE_FIELD(socket->mem_size);
    DESERIALIZE_FIELD(socket->nr_mem_ranges_elts);
    if(socket->nr_mem_ranges_elts){
        len = socket->nr_mem_ranges_elts * sizeof(*socket->mem_ranges);
        DESERIALIZE_ARRAY_FIELD(socket->mem_ranges, len);
    }

    hwcs_grps_array[n_hwcs_grps++] = socket;
    if (!socket->n_children) {
    	DEBUG_PRINT("End of deserialize_socket");
        return socket;
    }

    socket->children = (hwc_gs_t**) kmalloc(socket->n_children * sizeof (hwc_gs_t*), GFP_KERNEL);
    for (i = 0; i < socket->n_children; i++) {
        // DESERIALIZE_FIELD(id);
        // *((int*) &socket->children[i]) = id;
        socket->children[i] = (hwc_gs_t*) kmalloc(sizeof (hwc_gs_t), GFP_KERNEL);
        deserialize_socket(socket->children[i], src, has_mem);
        hwcs_grps_array[n_hwcs_grps++] = *(socket->children + i);
    }

    DESERIALIZE_FIELD(socket->n_siblings);
    for (i = 0; i < socket->n_siblings; i++) {
        DESERIALIZE_FIELD(id);
        *((int*) &socket->siblings[i]) = id;
    }
    for (i = 0; i < socket->n_siblings; i++) {
        DESERIALIZE_FIELD(id);
        *((int*) &socket->siblings_in[i]) = id;
    }

    DESERIALIZE_FIELD(socket->local_node_wrong);
    DESERIALIZE_FIELD(socket->local_node);
    DESERIALIZE_FIELD(socket->n_nodes);

    len = socket->n_nodes * sizeof (uint);
    DESERIALIZE_ARRAY_FIELD(socket->mem_latencies, len);

    if (has_mem == BANDWIDTH) {
        len = socket->n_nodes * sizeof (double);
        DESERIALIZE_ARRAY_FIELD(socket->mem_bandwidths_r, len);
        DESERIALIZE_ARRAY_FIELD(socket->mem_bandwidths1_r, len);
        DESERIALIZE_ARRAY_FIELD(socket->mem_bandwidths_w, len);
        DESERIALIZE_ARRAY_FIELD(socket->mem_bandwidths1_w, len);
    }

    DESERIALIZE_FIELD(flag);
    if (flag) {
        DESERIALIZE_FIELD(*(socket->pow_info));
    } else{
    	socket->pow_info = NULL;
    }

    DEBUG_PRINT("End of deserialize_socket");

    return socket;
}

/**
 * Serialize a hardware context
 * Fields order: id, level, type, phy_id, socket_id, parent_id, next_id, local_wrong_node,
 * frequency, latency_to_disk, latency_to_network
 */
void serialize_hwc(hw_context_t* hwc, char** dest) {
    int len, id;
    char* src;

    DEBUG_PRINT("serialize_hwc");

    SERIALIZE_FIELD(hwc->id);

    SERIALIZE_FIELD(hwc->level);
    SERIALIZE_FIELD(hwc->type);
    SERIALIZE_FIELD(hwc->phy_id);

    SERIALIZE_RELATED_FIELD(hwc->socket);
    SERIALIZE_RELATED_FIELD(hwc->parent);
    SERIALIZE_RELATED_FIELD(hwc->next);

    SERIALIZE_FIELD(hwc->local_node_wrong);
    SERIALIZE_FIELD(hwc->frequency);
    SERIALIZE_FIELD(hwc->latency_to_disk);
    SERIALIZE_FIELD(hwc->latency_to_network);

    SERIALIZE_FIELD(hwc->vcpu_id);
    SERIALIZE_FIELD(hwc->pcpu_id);

    DEBUG_PRINT("End of serialize_hwc");
}

/**
 * Deserialize a hardware context
 */
hw_context_t* deserialize_hwc(hw_context_t* hwc, char** src) {
    int len, id;
    char* dest;

    DEBUG_PRINT("deserialize_hwc");
    init_null_ptrs_of_hwc(hwc);

    DESERIALIZE_FIELD(hwc->id);

    DESERIALIZE_FIELD(hwc->level);
    DESERIALIZE_FIELD(hwc->type);
    DESERIALIZE_FIELD(hwc->phy_id);

    DESERIALIZE_FIELD(id);
    *((int*) &hwc->socket) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &hwc->parent) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &hwc->next) = id;

    DESERIALIZE_FIELD(hwc->local_node_wrong);
    DESERIALIZE_FIELD(hwc->frequency);
    DESERIALIZE_FIELD(hwc->latency_to_disk);
    DESERIALIZE_FIELD(hwc->latency_to_network);

    DESERIALIZE_FIELD(hwc->vcpu_id);
    DESERIALIZE_FIELD(hwc->pcpu_id);

//	printk("KEV hwc->vcpu_id: %d, hwc->phy_id: %d, hwc->pcpu_id: %d\n", hwc->vcpu_id, hwc->phy_id, hwc->pcpu_id);

    DEBUG_PRINT("End of deserialize_hwc");

    return hwc;
}

/**
 * Serialize a sibling structure
 * Fields order: id, level, latency, left, right, next
 */
void serialize_sibling(sibling_t* sibling, char** dest) {
    int len, id;
    char* src;

    DEBUG_PRINT("serialize_sibling");

    SERIALIZE_FIELD(sibling->id);
    SERIALIZE_FIELD(sibling->level);
    SERIALIZE_FIELD(sibling->latency);
    SERIALIZE_RELATED_FIELD(sibling->left);
    SERIALIZE_RELATED_FIELD(sibling->right);
    SERIALIZE_RELATED_FIELD(sibling->next);

    DEBUG_PRINT("End of serialize_sibling");
}

/**
 * Deserialize a sibling structure
 */
sibling_t* deserialize_sibling(sibling_t* sibling, char** src) {
    int len, id;
    char* dest;

    DEBUG_PRINT("deserialize_sibling");
    init_null_ptrs_of_sibling(sibling);

    DESERIALIZE_FIELD(sibling->id);
    DESERIALIZE_FIELD(sibling->level);
    DESERIALIZE_FIELD(sibling->latency);

    DESERIALIZE_FIELD(id);
    *((int*) &sibling->left) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &sibling->right) = id;
    DESERIALIZE_FIELD(id);
    *((int*) &sibling->next) = id;

    siblings_array[n_siblings++] = sibling;

    DEBUG_PRINT("End of deserialize_sibling");

    return sibling;
}

/**
 * Serialize a cache_info structure
 * Fields order: n_levels, latencies, sizes_OS, sizes_estimated
 */
void serialize_cache_info(mctop_cache_info_t* cache_info, char** dest) {
    int len;
    char* src;

    DEBUG_PRINT("serialize_cache_info");

    SERIALIZE_FIELD(cache_info->n_levels);

    len = cache_info->n_levels * sizeof (uint64_t);
    SERIALIZE_ARRAY_FIELD(cache_info->latencies, len);
    SERIALIZE_ARRAY_FIELD(cache_info->sizes_OS, len);
    SERIALIZE_ARRAY_FIELD(cache_info->sizes_estimated, len);

    DEBUG_PRINT("End of serialize_cache_info");
}

/**
 * Deserialize a cache_info structure
 */
mctop_cache_info_t* deserialize_cache_info(mctop_cache_info_t* cache_info, char** src) {
    int len;
    char* dest;

    DEBUG_PRINT("deserialize_cache_info");
    init_null_ptrs_of_cache_info(cache_info);

    DESERIALIZE_FIELD(cache_info->n_levels);

    len = cache_info->n_levels * sizeof (uint64_t);
    DESERIALIZE_ARRAY_FIELD(cache_info->latencies, len);
    DESERIALIZE_ARRAY_FIELD(cache_info->sizes_OS, len);
    DESERIALIZE_ARRAY_FIELD(cache_info->sizes_estimated, len);

    DEBUG_PRINT("End of deserialize_cache_info");

    return cache_info;
}

/**
 * Get one specific socket from an array of sockets
 */
socket_t* get_socket_by_id(socket_t* sockets_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_socket_by_id");

    for (i = 0; i < n; i++) {
        if (sockets_array[i].id == id) {
        	DEBUG_PRINT("End of get_socket_by_id");
            return &sockets_array[i];
        }
    }

    DEBUG_PRINT("End of get_socket_by_id");
    
    return NULL;
}

/**
 * Get one specific socket from an array of sockets pointers
 */
socket_t* get_socket_by_id_2(socket_t** sockets_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_socket_by_id_2");

    for (i = 0; i < n; i++) {
        if (sockets_array[i]->id == id) {
        	DEBUG_PRINT("End of get_socket_by_id_2");
            return sockets_array[i];
        }
    }

    DEBUG_PRINT("End of get_socket_by_id_2");

    return NULL;
}

/**
 * Get one specific hwc_gs from an array of hwc_gs
 */
hwc_gs_t* get_hwc_gs_by_id(hwc_gs_t* hwcs_grps_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_hwc_gs_by_id");

    for (i = 0; i < n; i++) {
        if (hwcs_grps_array[i].id == id) {
        	DEBUG_PRINT("End of get_hwc_gs_by_id");
            return &hwcs_grps_array[i];
        }
    }

    DEBUG_PRINT("End of get_hwc_gs_by_id");

    return NULL;
}

/**
 * Get one specific hwc_gs from an array of hwc_gs pointers
 */
hwc_gs_t* get_hwc_gs_by_id_2(hwc_gs_t** hwcs_grps_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_hwc_gs_by_id_2");

    for (i = 0; i < n; i++) {
        if (hwcs_grps_array[i]->id == id) {
        	DEBUG_PRINT("End of get_hwc_gs_by_id_2");
            return hwcs_grps_array[i];
        }
    }

    DEBUG_PRINT("End of get_hwc_gs_by_id_2");

    return NULL;
}

/**
 * Get one specific hwc from an array of hwcs
 */
hw_context_t* get_hwc_by_id(hw_context_t* hwcs_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_hwc_by_id");

    for (i = 0; i < n; i++) {
        if (hwcs_array[i].id == id) {
    		DEBUG_PRINT("End of get_hwc_by_id");
            return &hwcs_array[i];
        }
    }

    DEBUG_PRINT("End of get_hwc_by_id");

    return NULL;
}

/**
 * Get one specific hwc from an array of hwcs pointers
 */
hw_context_t* get_hwc_by_id_2(hw_context_t** hwcs_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_hwc_by_id_2");

    for (i = 0; i < n; i++) {
        if (hwcs_array[i]->id == id) {
        	DEBUG_PRINT("End of get_hwc_by_id_2");
            return hwcs_array[i];
        }
    }

    DEBUG_PRINT("End of get_hwc_by_id_2");

    return NULL;
}

/**
 * Get one specific sibling from an array of siblings
 */
sibling_t* get_sibling_by_id(sibling_t* siblings_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_sibling_by_id");

    for (i = 0; i < n; i++) {
        if (siblings_array[i].id == id) {
        	DEBUG_PRINT("End of get_sibling_by_id");
            return &siblings_array[i];
        }
    }

    DEBUG_PRINT("End of get_sibling_by_id");

    return NULL;
}

/**
 * Get one specific sibling from a array of siblings pointers
 */
sibling_t* get_sibling_by_id_2(sibling_t** siblings_array, int n, int id) {
    int i;

    DEBUG_PRINT("get_sibling_by_id_2");

    for (i = 0; i < n; i++) {
        if (siblings_array[i]->id == id) {
        	DEBUG_PRINT("End of get_sibling_by_id_2");
            return siblings_array[i];
        }
    }

    DEBUG_PRINT("End of get_sibling_by_id_2");

    return NULL;
}

void reestablish_links_for_socket(socket_t* socket, mctop_t* mctop) {
    int id, i;

    DEBUG_PRINT("reestablish_links_for_socket");

    GET_ID_IN_FIELD(socket->socket);
    socket->socket = get_socket_by_id(mctop->sockets, mctop->n_sockets, id);
    GET_ID_IN_FIELD(socket->parent);
    socket->parent = get_hwc_gs_by_id_2(hwcs_grps_array, n_hwcs_grps, id);
    GET_ID_IN_FIELD(socket->next);
    socket->next = get_hwc_gs_by_id_2(hwcs_grps_array, n_hwcs_grps, id);

    // printf("\n++++++++++++++++++++socket %p\n", socket);
    // printf("++++++++++++++++++++socket->id %d\n", socket->id);
    // printf("++++++++++++++++++++socket->socket %p\n", socket->socket);
    // printf("++++++++++++++++++++socket->parent %p\n", socket->parent);
    // printf("++++++++++++++++++++socket->next %p\n", socket->next);

    for (i = 0; i < socket->n_hwcs; i++) {
        GET_ID_IN_FIELD(socket->hwcs[i]);
        socket->hwcs[i] = get_hwc_by_id(mctop->hwcs, mctop->n_hwcs, id);
    }

    if (!socket->n_children) {
    	DEBUG_PRINT("End of reestablish_links_for_socket");
    	return;
    }

    for (i = 0; i < socket->n_children; i++) {
        reestablish_links_for_socket(socket->children[i], mctop);
    }

    socket->topo = mctop;
    for (i = 0; i < socket->n_siblings; i++) {
        GET_ID_IN_FIELD(socket->siblings[i]);
        socket->siblings[i] = get_sibling_by_id_2(mctop->siblings, mctop->n_siblings, id);
        GET_ID_IN_FIELD(socket->siblings_in[i]);
        socket->siblings_in[i] = get_sibling_by_id_2(mctop->siblings, mctop->n_siblings, id);
    }
    
    DEBUG_PRINT("End of reestablish_links_for_socket");
}

/** Initialize all the related fields of a mctop_t structure to NULL
*/
void init_null_ptrs_of_mctop(mctop_t* mctop){
    mctop->latencies = NULL;
    mctop->sockets = NULL;
    mctop->hwcs = NULL;
    mctop->cache = NULL;
    mctop->siblings = NULL;
    mctop->cache = NULL;
    mctop->mem_bandwidths_r = NULL;
    mctop->mem_bandwidths1_r = NULL;
    mctop->mem_bandwidths_w = NULL;
    mctop->mem_bandwidths1_w = NULL;
    mctop->pow_info = NULL;
}

/** Initialize all the related fields of a hw_context_t structure to NULL
*/
void init_null_ptrs_of_hwc(hw_context_t* hwc){
    hwc->socket = NULL;
    hwc->parent = NULL;
    hwc->next = NULL;
}

/** Initialize all the related fields of a socket_t structure to NULL
*/
void init_null_ptrs_of_socket(socket_t* socket){
    socket->parent = NULL;
    socket->next = NULL;
    socket->hwcs = NULL;
    socket->children = NULL;
    socket->topo = NULL;
    socket->siblings = NULL;
    socket->siblings_in = NULL;
    socket->mem_latencies = NULL;
    socket->mem_bandwidths_r = NULL;
    socket->mem_bandwidths1_r = NULL;
    socket->mem_bandwidths_w = NULL;
    socket->mem_bandwidths1_w = NULL;
}

/** Initialize all the related fields of a hwc_gs_t structure to NULL
*/
void init_null_ptrs_of_hwc_grp(hwc_gs_t* hwc_gs){
    init_null_ptrs_of_socket(hwc_gs);    
}

/** Initialize all the related fields of a sibling_t structure to NULL
*/
void init_null_ptrs_of_sibling(sibling_t* sibling){
    sibling->left = NULL;
    sibling->right = NULL;
    sibling->next = NULL;
}

/** Initialize all the related fields of a mctop_cache_info_t structure to NULL
*/
void init_null_ptrs_of_cache_info(mctop_cache_info_t* cache_info){
    cache_info->latencies = NULL;
    cache_info->sizes_OS = NULL;
    cache_info->sizes_estimated = NULL;
}

/* End of the addition */




/*
 * Note sizeof(xen_ulong_t) can be more than sizeof(unsigned long). Be
 * careful to only use bitops which allow for this (e.g
 * test_bit/find_first_bit and friends but not __ffs) and to pass
 * BITS_PER_EVTCHN_WORD as the bitmask length.
 */
#define BITS_PER_EVTCHN_WORD (sizeof(xen_ulong_t)*8)
/*
 * Make a bitmask (i.e. unsigned long *) of a xen_ulong_t
 * array. Primarily to avoid long lines (hence the terse name).
 */
#define BM(x) (unsigned long *)(x)
/* Find the first set bit in a evtchn mask */
#define EVTCHN_FIRST_BIT(w) find_first_bit(BM(&(w)), BITS_PER_EVTCHN_WORD)

static DEFINE_PER_CPU(xen_ulong_t [EVTCHN_2L_NR_CHANNELS/BITS_PER_EVTCHN_WORD],
		      cpu_evtchn_mask);

static unsigned evtchn_2l_max_channels(void)
{
	return EVTCHN_2L_NR_CHANNELS;
}

static void evtchn_2l_bind_to_cpu(struct irq_info *info, unsigned cpu)
{
	clear_bit(info->evtchn, BM(per_cpu(cpu_evtchn_mask, info->cpu)));
	set_bit(info->evtchn, BM(per_cpu(cpu_evtchn_mask, cpu)));
}

static void evtchn_2l_clear_pending(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	sync_clear_bit(port, BM(&s->evtchn_pending[0]));
}

static void evtchn_2l_set_pending(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	sync_set_bit(port, BM(&s->evtchn_pending[0]));
}

static bool evtchn_2l_is_pending(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	return sync_test_bit(port, BM(&s->evtchn_pending[0]));
}

static bool evtchn_2l_test_and_set_mask(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	return sync_test_and_set_bit(port, BM(&s->evtchn_mask[0]));
}

static void evtchn_2l_mask(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	sync_set_bit(port, BM(&s->evtchn_mask[0]));
}

static void evtchn_2l_unmask(unsigned port)
{
	struct shared_info *s = HYPERVISOR_shared_info;
	unsigned int cpu = get_cpu();
	int do_hypercall = 0, evtchn_pending = 0;

	BUG_ON(!irqs_disabled());

	if (unlikely((cpu != cpu_from_evtchn(port))))
		do_hypercall = 1;
	else {
		/*
		 * Need to clear the mask before checking pending to
		 * avoid a race with an event becoming pending.
		 *
		 * EVTCHNOP_unmask will only trigger an upcall if the
		 * mask bit was set, so if a hypercall is needed
		 * remask the event.
		 */
		sync_clear_bit(port, BM(&s->evtchn_mask[0]));
		evtchn_pending = sync_test_bit(port, BM(&s->evtchn_pending[0]));

		if (unlikely(evtchn_pending && xen_hvm_domain())) {
			sync_set_bit(port, BM(&s->evtchn_mask[0]));
			do_hypercall = 1;
		}
	}

	/* Slow path (hypercall) if this is a non-local port or if this is
	 * an hvm domain and an event is pending (hvm domains don't have
	 * their own implementation of irq_enable). */
	if (do_hypercall) {
		struct evtchn_unmask unmask = { .port = port };
		(void)HYPERVISOR_event_channel_op(EVTCHNOP_unmask, &unmask);
	} else {
		struct vcpu_info *vcpu_info = __this_cpu_read(xen_vcpu);

		/*
		 * The following is basically the equivalent of
		 * 'hw_resend_irq'. Just like a real IO-APIC we 'lose
		 * the interrupt edge' if the channel is masked.
		 */
		if (evtchn_pending &&
		    !sync_test_and_set_bit(port / BITS_PER_EVTCHN_WORD,
					   BM(&vcpu_info->evtchn_pending_sel)))
			vcpu_info->evtchn_upcall_pending = 1;
	}

	put_cpu();
}

static DEFINE_PER_CPU(unsigned int, current_word_idx);
static DEFINE_PER_CPU(unsigned int, current_bit_idx);

/*
 * Mask out the i least significant bits of w
 */
#define MASK_LSBS(w, i) (w & ((~((xen_ulong_t)0UL)) << i))

static inline xen_ulong_t active_evtchns(unsigned int cpu,
					 struct shared_info *sh,
					 unsigned int idx)
{
	return sh->evtchn_pending[idx] &
		per_cpu(cpu_evtchn_mask, cpu)[idx] &
		~sh->evtchn_mask[idx];
}

/*
 * Search the CPU's pending events bitmasks.  For each one found, map
 * the event number to an irq, and feed it into do_IRQ() for handling.
 *
 * Xen uses a two-level bitmap to speed searching.  The first level is
 * a bitset of words which contain pending event bits.  The second
 * level is a bitset of pending events themselves.
 */
static void evtchn_2l_handle_events(unsigned cpu)
{
	int irq;
	xen_ulong_t pending_words;
	xen_ulong_t pending_bits;
	int start_word_idx, start_bit_idx;
	int word_idx, bit_idx;
	int i;
	struct shared_info *s = HYPERVISOR_shared_info;
	struct vcpu_info *vcpu_info = __this_cpu_read(xen_vcpu);

	/* Timer interrupt has highest priority. */
	irq = irq_from_virq(cpu, VIRQ_TIMER);
	if (irq != -1) {
		unsigned int evtchn = evtchn_from_irq(irq);
		word_idx = evtchn / BITS_PER_LONG;
		bit_idx = evtchn % BITS_PER_LONG;
		if (active_evtchns(cpu, s, word_idx) & (1ULL << bit_idx))
			generic_handle_irq(irq);
	}

	/*
	 * Master flag must be cleared /before/ clearing
	 * selector flag. xchg_xen_ulong must contain an
	 * appropriate barrier.
	 */
	pending_words = xchg_xen_ulong(&vcpu_info->evtchn_pending_sel, 0);

	start_word_idx = __this_cpu_read(current_word_idx);
	start_bit_idx = __this_cpu_read(current_bit_idx);

	word_idx = start_word_idx;

	for (i = 0; pending_words != 0; i++) {
		xen_ulong_t words;

		words = MASK_LSBS(pending_words, word_idx);

		/*
		 * If we masked out all events, wrap to beginning.
		 */
		if (words == 0) {
			word_idx = 0;
			bit_idx = 0;
			continue;
		}
		word_idx = EVTCHN_FIRST_BIT(words);

		pending_bits = active_evtchns(cpu, s, word_idx);
		bit_idx = 0; /* usually scan entire word from start */
		/*
		 * We scan the starting word in two parts.
		 *
		 * 1st time: start in the middle, scanning the
		 * upper bits.
		 *
		 * 2nd time: scan the whole word (not just the
		 * parts skipped in the first pass) -- if an
		 * event in the previously scanned bits is
		 * pending again it would just be scanned on
		 * the next loop anyway.
		 */
		if (word_idx == start_word_idx) {
			if (i == 0)
				bit_idx = start_bit_idx;
		}

		do {
			xen_ulong_t bits;
			int port;

			bits = MASK_LSBS(pending_bits, bit_idx);

			/* If we masked out all events, move on. */
			if (bits == 0)
				break;

			bit_idx = EVTCHN_FIRST_BIT(bits);

			/* Process port. */
			port = (word_idx * BITS_PER_EVTCHN_WORD) + bit_idx;
			irq = get_evtchn_to_irq(port);

			if (irq != -1)
				generic_handle_irq(irq);

			bit_idx = (bit_idx + 1) % BITS_PER_EVTCHN_WORD;

			/* Next caller starts at last processed + 1 */
			__this_cpu_write(current_word_idx,
					 bit_idx ? word_idx :
					 (word_idx+1) % BITS_PER_EVTCHN_WORD);
			__this_cpu_write(current_bit_idx, bit_idx);
		} while (bit_idx != 0);

		/* Scan start_l1i twice; all others once. */
		if ((word_idx != start_word_idx) || (i != 0))
			pending_words &= ~(1UL << word_idx);

		word_idx = (word_idx + 1) % BITS_PER_EVTCHN_WORD;
	}
}

irqreturn_t xen_debug_interrupt(int irq, void *dev_id)
{
	struct shared_info *sh = HYPERVISOR_shared_info;
	int cpu = smp_processor_id();
	xen_ulong_t *cpu_evtchn = per_cpu(cpu_evtchn_mask, cpu);
	int i;
	unsigned long flags;
	static DEFINE_SPINLOCK(debug_lock);
	struct vcpu_info *v;

	spin_lock_irqsave(&debug_lock, flags);

	printk("\nvcpu %d\n  ", cpu);

	for_each_online_cpu(i) {
		int pending;
		v = per_cpu(xen_vcpu, i);
		pending = (get_irq_regs() && i == cpu)
			? xen_irqs_disabled(get_irq_regs())
			: v->evtchn_upcall_mask;
		printk("%d: masked=%d pending=%d event_sel %0*"PRI_xen_ulong"\n  ", i,
		       pending, v->evtchn_upcall_pending,
		       (int)(sizeof(v->evtchn_pending_sel)*2),
		       v->evtchn_pending_sel);
	}
	v = per_cpu(xen_vcpu, cpu);

	printk("\npending:\n   ");
	for (i = ARRAY_SIZE(sh->evtchn_pending)-1; i >= 0; i--)
		printk("%0*"PRI_xen_ulong"%s",
		       (int)sizeof(sh->evtchn_pending[0])*2,
		       sh->evtchn_pending[i],
		       i % 8 == 0 ? "\n   " : " ");
	printk("\nglobal mask:\n   ");
	for (i = ARRAY_SIZE(sh->evtchn_mask)-1; i >= 0; i--)
		printk("%0*"PRI_xen_ulong"%s",
		       (int)(sizeof(sh->evtchn_mask[0])*2),
		       sh->evtchn_mask[i],
		       i % 8 == 0 ? "\n   " : " ");

	printk("\nglobally unmasked:\n   ");
	for (i = ARRAY_SIZE(sh->evtchn_mask)-1; i >= 0; i--)
		printk("%0*"PRI_xen_ulong"%s",
		       (int)(sizeof(sh->evtchn_mask[0])*2),
		       sh->evtchn_pending[i] & ~sh->evtchn_mask[i],
		       i % 8 == 0 ? "\n   " : " ");

	printk("\nlocal cpu%d mask:\n   ", cpu);
	for (i = (EVTCHN_2L_NR_CHANNELS/BITS_PER_EVTCHN_WORD)-1; i >= 0; i--)
		printk("%0*"PRI_xen_ulong"%s", (int)(sizeof(cpu_evtchn[0])*2),
		       cpu_evtchn[i],
		       i % 8 == 0 ? "\n   " : " ");

	printk("\nlocally unmasked:\n   ");
	for (i = ARRAY_SIZE(sh->evtchn_mask)-1; i >= 0; i--) {
		xen_ulong_t pending = sh->evtchn_pending[i]
			& ~sh->evtchn_mask[i]
			& cpu_evtchn[i];
		printk("%0*"PRI_xen_ulong"%s",
		       (int)(sizeof(sh->evtchn_mask[0])*2),
		       pending, i % 8 == 0 ? "\n   " : " ");
	}

	printk("\npending list:\n");
	for (i = 0; i < EVTCHN_2L_NR_CHANNELS; i++) {
		if (sync_test_bit(i, BM(sh->evtchn_pending))) {
			int word_idx = i / BITS_PER_EVTCHN_WORD;
			printk("  %d: event %d -> irq %d%s%s%s\n",
			       cpu_from_evtchn(i), i,
			       get_evtchn_to_irq(i),
			       sync_test_bit(word_idx, BM(&v->evtchn_pending_sel))
			       ? "" : " l2-clear",
			       !sync_test_bit(i, BM(sh->evtchn_mask))
			       ? "" : " globally-masked",
			       sync_test_bit(i, BM(cpu_evtchn))
			       ? "" : " locally-masked");
		}
	}

	spin_unlock_irqrestore(&debug_lock, flags);

	return IRQ_HANDLED;
}



/* Added by KevinJio */
#include <xen/mctop.h>
mctop_t topology;
extern task_list_t* task_list_head; // The list head of app tasks using mctop

irqreturn_t xen_topology_interrupt_handler(int irq, void *dev_id){
	struct shared_info *s = HYPERVISOR_shared_info;
	char *src;
	int i, j;
	socket_t *socket;

	printk("KEV ============== s->topology_array %s ===============\n", s->topology_array);
	printk("KEV ============== s->topology_array[0] %o ===============\n", s->topology_array[0]);
	printk("KEV ============== s->topology_array[1] %o ===============\n", s->topology_array[1]);
	printk("KEV ============== s->topology_array[2] %o ===============\n", s->topology_array[2]);
	printk("KEV ============== s->topology_array[3] %o ===============\n", s->topology_array[3]);

	src = s->topology_array;
	deserialize_mctop(&topology, &src);

	printk("KEV ============== topology.n_levels %d ===============\n", topology.n_levels);
	printk("KEV ============== topology.n_hwcs %d ===============\n", topology.n_hwcs);
	printk("KEV ============== topology.n_sockets %d ===============\n", topology.n_sockets);
	printk("KEV ============== topology.latencies %p ===============\n", topology.latencies);
	printk("KEV ============== topology.latencies[0] %d ===============\n", topology.latencies[0]);
	printk("KEV ============== topology.sockets[0].mem_size %ld ===============\n", topology.sockets[0].mem_size);

	for(i=0; i<topology.n_hwcs; i++){
		printk("KEV topology.hwcs[%d].vcpu_id: %d, topology.hwcs[%d].phy_id: %d, topology.hwcs[%d].pcpu_id: %d\n", i, topology.hwcs[i].vcpu_id, i, topology.hwcs[i].phy_id, i, topology.hwcs[i].pcpu_id);
	}

	for(i=0; i<topology.n_sockets; i++){
		socket = &topology.sockets[i];
		printk("KEV ================== SOCKET %d ==================\n", socket->id);
		printk("KEV 	mem_size: %ld\n", socket->mem_size);
		printk("KEV 	nr_mem_ranges_elts: %ld\n", socket->nr_mem_ranges_elts);
		for(j=0; j<socket->nr_mem_ranges_elts; j++){
			printk("Element %d: %ld\n", j, socket->mem_ranges[j]);
		}
		for(j=0; j<socket->nr_mem_ranges_elts; j++){
			printk("KEV Range %d: from 0x%llx to 0x%llx ==> size= %ld\n", j/2, socket->mem_ranges[j], socket->mem_ranges[j+1], socket->mem_ranges[j+1] - socket->mem_ranges[j] + 1);
			j++;
		}
	}

	if(task_list_head != NULL){
		task_list_t* curr_task_list;
		curr_task_list = task_list_head;
		struct task_struct* curr_task;
		printk("KEV Entering the loop on registered task\n");
		while(curr_task_list != NULL){
			rcu_read_lock();
			curr_task = find_task_by_vpid(curr_task_list->pid);
			rcu_read_unlock();

			//Test if the task is still alive and runnable
			if(curr_task != NULL && pid_alive(curr_task)){
				printk("KEV Task with pid %d is alive\n", curr_task_list->pid);

				// Send SIGHUP to the task
				siginfo_t info;
				info.si_signo	= SIGHUP;
				info.si_errno	= 0;
				send_sig_info(SIGHUP, &info, curr_task);
			} else {
				// Remove this task from the list
				printk("KEV Removing task with pid %d from the tasks list\n", curr_task_list->pid);

				if(curr_task_list == task_list_head){
					task_list_head = curr_task_list->next;
				}

				if(curr_task_list->next != NULL){
					curr_task_list->next->prev = curr_task_list->prev;
				}

				if(curr_task_list->prev != NULL){
					curr_task_list->prev->next = curr_task_list->next;
				}

				kfree(curr_task_list);
			}

			curr_task_list = curr_task_list->next;
		}
	} else {
		printk("KEV Empty task list\n");
	}

	return IRQ_HANDLED;
}
/* End of the addition */



static void evtchn_2l_resume(void)
{
	int i;

	for_each_online_cpu(i)
		memset(per_cpu(cpu_evtchn_mask, i), 0, sizeof(xen_ulong_t) *
				EVTCHN_2L_NR_CHANNELS/BITS_PER_EVTCHN_WORD);
}

static const struct evtchn_ops evtchn_ops_2l = {
	.max_channels      = evtchn_2l_max_channels,
	.nr_channels       = evtchn_2l_max_channels,
	.bind_to_cpu       = evtchn_2l_bind_to_cpu,
	.clear_pending     = evtchn_2l_clear_pending,
	.set_pending       = evtchn_2l_set_pending,
	.is_pending        = evtchn_2l_is_pending,
	.test_and_set_mask = evtchn_2l_test_and_set_mask,
	.mask              = evtchn_2l_mask,
	.unmask            = evtchn_2l_unmask,
	.handle_events     = evtchn_2l_handle_events,
	.resume	           = evtchn_2l_resume,
};

void __init xen_evtchn_2l_init(void)
{
	pr_info("Using 2-level ABI\n");
	evtchn_ops = &evtchn_ops_2l;
}
