#ifndef __H_MCTOP__
#define __H_MCTOP__

// #include <xen/xmalloc.h>

// #include <linux/gfp.h>
// #include <linux/kernel.h>
#include <linux/slab.h>

// extern __always_inline void *kmalloc(size_t size, gfp_t flags);





// #include <stdio.h>
// #include <stdlib.h>
// #include <stdint.h>
// #include <assert.h>
// #include <unistd.h>
// #include <pthread.h>

// #include <helper.h> // Added by KevinJio

// #ifdef __x86_64__
// #  include <numa.h>
// #elif defined(__sparc__)
// #  include <sys/lgrp_user.h>
// #  include <numa_sparc.h>
// #endif

// typedef uint64_t ticks;
typedef uint64_t mctop_ticks;

#ifdef __cplusplus
extern "C" {
#endif


#define MCTOP_LVL_ID_MULTI 10000

  typedef enum
  {
    HW_CONTEXT,
    CORE,
    HWC_GROUP,
    SOCKET,
    CROSS_SOCKET,
  } mctop_type_t;

  static const char* mctop_type_desc[] =
  {
    "HW Context",
    "Core",
    "HW Context group",
    "Socket",
    "Cross Socket",
  };

  typedef enum
  {
    NO_MEMORY,
    LATENCY,
    BANDWIDTH,
  } mctop_mem_type_t;

  static inline const char*
  mctop_get_type_desc(mctop_type_t type)
  {
    return mctop_type_desc[type];
  }
  typedef unsigned int uint;
  typedef struct hwc_gs socket_t;
  typedef struct hwc_gs hwc_group_t;

  typedef struct mctop_cache_info
  {
    uint n_levels;
    uint64_t* latencies;
    uint64_t* sizes_OS;
    uint64_t* sizes_estimated;
  } mctop_cache_info_t;

  typedef enum 
  {
    CORES,
    REST,
    PACKAGE,
    DRAM,
    TOTAL,
    MCTOP_POW_COMP_TYPE_NUM
  } mctop_pow_type;
  #define MCTOP_POW_TYPE_NUM 6


  typedef struct mctop_pow_info
  {
    double idle[MCTOP_POW_TYPE_NUM];
    double first_core[MCTOP_POW_TYPE_NUM];
    double second_core[MCTOP_POW_TYPE_NUM];
    double second_hwc_core[MCTOP_POW_TYPE_NUM];
    double all_cores[MCTOP_POW_TYPE_NUM];
    double all_hwcs[MCTOP_POW_TYPE_NUM];
  } mctop_pow_info_t;

  typedef struct mctop
  {
    uint n_levels;		/* num. of latency lvls */
    uint* latencies;		/* latency per level */
    uint n_hwcs;		/* num. of hwcs in this machine */
    uint socket_level;		/* level of sockets */
    uint n_sockets;		/* num. of sockets/nodes */
    socket_t* sockets;		/* pointer to sockets/nodes */
    uint is_smt;		/* is SMT enabled CPU */
    uint n_hwcs_per_core;       /* if SMT, how many hw contexts per core? */
    uint has_mem;	        /* flag whether there are mem. latencies */
    uint* node_to_socket;       /* node-id to socket-id translation */
    struct mctop_hw_context* hwcs;	/* pointers to hwcs */
    uint n_siblings;		/* total number of sibling relationships */
    struct sibling** siblings;	/* pointers to sibling relationships */
    mctop_cache_info_t* cache;	/* pointer to cache information */
    double* mem_bandwidths_r;	/* Read mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_r;	/* Read mem. bandwidth of each socket, single threaded */
    double* mem_bandwidths_w;	/* Write mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_w;	/* Write mem. bandwidth of each socket, single threaded */
    mctop_pow_info_t* pow_info;	/* power info */
  } mctop_t;

  typedef struct hwc_gs		/* group / socket */
  {
    uint id;			/* mctop id */
    uint level;			/* latency hierarchy lvl */
    mctop_type_t type;		/* HWC_GROUP or SOCKET */
    uint latency;		/* comm. latency within group */
    socket_t* socket;		/* Group: pointer to parent socket */
    struct hwc_gs* parent;	/* Group: pointer to parent hwcgroup */
    struct hwc_gs* next;	/* link groups of a level to a list */
    uint n_hwcs;		/* num. of hwcs descendants */
    struct mctop_hw_context** hwcs;	/* descendant hwcs */
    uint n_cores;	        /* num. of physical cores (if !smt = n_hwcs */
    uint n_children;		/* num. of hwc_group descendants */
    struct hwc_gs** children;	/* pointer to children hwcgroup */
    mctop_t* topo;		/* link to topology socket only info */
    uint n_siblings;		/* number of other sockets */
    struct sibling** siblings;	/* pointers to other sockets, sorted closest 1st, max bw from this to sibling */
    struct sibling** siblings_in;	/* pointers to other sockets, sorted closest 1st, max bw sibling to this */
    uint local_node_wrong;	/* does the OS have correct mapping of cores to nodes? */
    uint local_node;		/* local NUMA mem. node */
    uint n_nodes;		/* num of nodes = topo->n_sockets */
    uint* mem_latencies;	/* mem. latencies to NUMA nodes */
    double* mem_bandwidths_r;	/* Read mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_r;	/* Read mem. bandwidth of each socket, single threaded */
    double* mem_bandwidths_w;	/* Write mem. bandwidth of each socket, maximum */
    double* mem_bandwidths1_w;	/* Write mem. bandwidth of each socket, single threaded */
    mctop_pow_info_t* pow_info;	/* power info */

	// Added by KevinJio
	unsigned long mem_size;  /* The size of the local memory */
	uint nr_mem_ranges_elts; // The number of addresses forming the memory ranges belonging to this node
	unsigned long *mem_ranges; // Ranges of memory belonging to this node. A range is made up of two consecutive elements of this array
  } hwc_gs_t;

  typedef struct sibling
  {
    uint id;			/* needed?? */
    uint level;			/* latency hierarchy lvl */
    uint latency;			/* comm. latency across this hop */
    socket_t* left;		/* left  -->sibling--> right */
    socket_t* right;		/* right -->sibling--> left */
    struct sibling* next;
  } sibling_t;

  typedef struct mctop_hw_context
  {
    uint id;			/* mctop id */
    uint level;			/* latency hierarchy lvl */
    mctop_type_t type;		/* HW_CONTEXT or CORE? */
    uint phy_id;		/* OS id (e.g., for set_cpu() */
    socket_t* socket;		/* pointer to parent socket */
    struct hwc_gs* parent;	/* pointer to parent hwcgroup */
    struct mctop_hw_context* next;	/* link hwcs to a list */
    uint local_node_wrong;	/* does the OS have correct mapping of hwc to local node? */


    // Added by KevinJio
    float frequency; /* Frequency of the hardware context */
    mctop_ticks latency_to_disk; /* Latency to reach the disk */
    mctop_ticks latency_to_network; /* Latency to reach the network adapter */

    // Virtual-to-physical mapping  information
    uint vcpu_id;
    uint pcpu_id;
  } hw_context_t;


  /* ******************************************************************************** */
  /* CDF fucntions */
  /* ******************************************************************************** */

  typedef struct cdf_point
  {
    uint64_t val;
    double percentile;
  } cdf_point_t;

  typedef struct cdf
  {
    size_t n_points;
    cdf_point_t* points;
  } cdf_t;

  typedef struct cdf_cluster_point
  {
    int idx;
    size_t size;
    uint64_t val_min;
    uint64_t val_max;
    uint64_t median;
  } cdf_cluster_point_t;

  typedef struct cdf_cluster
  {
    size_t n_clusters;
    cdf_cluster_point_t* clusters;
  } cdf_cluster_t;


  /* ******************************************************************************** */
  /* MCTOP CONSTRUCTION IF */
  /* ******************************************************************************** */

  mctop_t* mctop_construct(uint64_t** lat_table_norm, const size_t N,
    uint64_t** mem_lat_table, const uint n_sockets,
    cdf_cluster_t* cc, const int is_smt);
  mctop_t* mctop_load(const char* mct_file);
  void mctop_free(mctop_t* topo);
  void mctop_mem_bandwidth_add(mctop_t* topo, double** mem_bw_r, double** mem_bw_r1, double** mem_bw_w, double** mem_bw_w1);
  void mctop_mem_latencies_add(mctop_t* topo, uint64_t** mem_lat_table);
  void mctop_cache_info_add(mctop_t* topo, mctop_cache_info_t* mci);
  void mctop_pow_info_add(mctop_t* topo, double*** pow_measurements);

  void mctop_print(mctop_t* topo);
  void mctop_dot_graph_plot(mctop_t* topo,  const uint max_cross_socket_lvl);

  /* ******************************************************************************** */
  /* MCTOP CONTROL IF */
  /* ******************************************************************************** */

  /* topo getters ******************************************************************* */
  socket_t* mctop_get_socket(mctop_t* topo, const uint socket_n);
  socket_t* mctop_get_first_socket(mctop_t* topo);
  hwc_gs_t* mctop_get_first_gs_core(mctop_t* topo);
  hwc_gs_t* mctop_get_first_gs_at_lvl(mctop_t* topo, const uint lvl);
  sibling_t* mctop_get_first_sibling_lvl(mctop_t* topo, const uint lvl);
  sibling_t* mctop_get_sibling_with_sockets(mctop_t* topo, socket_t* s0, socket_t* s1);

  uint mctop_get_num_levels(mctop_t* topo);
  size_t mctop_get_num_nodes(mctop_t* topo);
  size_t mctop_get_num_cores(mctop_t* topo);
  size_t mctop_get_num_cores_per_socket(mctop_t* topo);
  size_t mctop_get_num_hwc_per_socket(mctop_t* topo);
  size_t mctop_get_num_hwc_per_core(mctop_t* topo);

  /* cache */
  typedef enum 
  {
      L1I,			/* L1 instruction */
      L1D,			/* L1D = L1 */
      L2,
      L3,			/* LLC = L3 */
  } mctop_cache_level_t;
#define L1  L1D
#define LLC L3

  size_t mctop_get_cache_size_kb(mctop_t* topo, mctop_cache_level_t level);
  /* estimated size and latency not defined for L1I */
  size_t mctop_get_cache_size_estimated_kb(mctop_t* topo, mctop_cache_level_t level);
  size_t mctop_get_cache_latency(mctop_t* topo, mctop_cache_level_t level);

  /* socket getters ***************************************************************** */
  hw_context_t* mctop_socket_get_first_hwc(socket_t* socket);
  hw_context_t* mctop_socket_get_nth_hwc(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_gs_core(socket_t* socket);
  hwc_gs_t* mctop_socket_get_nth_gs_core(socket_t* socket, const uint nth);
  hwc_gs_t* mctop_socket_get_first_child_lvl(socket_t* socket, const uint lvl);
  size_t mctop_socket_get_num_cores(socket_t* socket);
  size_t mctop_socket_get_num_hw_contexts(socket_t* socket);

  double mctop_socket_get_bw_local(socket_t* socket);
  double mctop_socket_get_bw_local_one(socket_t* socket);

  double mctop_socket_get_bw_to(socket_t* socket, socket_t* to);

  uint mctop_socket_get_local_node(socket_t* socket);

  /* node getters ******************************************************************* */
  socket_t* mctop_node_to_socket(mctop_t* topo, const uint nid);

  /* hwcid ************************************************************************** */
  uint mctop_hwcid_get_local_node(mctop_t* topo, const uint hwcid);
  socket_t* mctop_hwcid_get_socket(mctop_t* topo, const uint hwcid);
  hwc_gs_t* mctop_hwcid_get_core(mctop_t* topo, const uint hwcid);
  uint mctop_hwcid_get_nth_hwc_in_socket(mctop_t* topo, const uint hwcid);
  uint mctop_hwcid_get_nth_hwc_in_core(mctop_t* topo, const uint hwcid);
  uint mctop_hwcid_get_nth_core_in_socket(mctop_t* topo, const uint hwcid);

  /* mctop id *********************************************************************** */
  hwc_gs_t* mctop_id_get_hwc_gs(mctop_t* topo, const uint id);

  /* queries ************************************************************************ */
  uint mctop_hwcs_are_same_core(hw_context_t* a, hw_context_t* b);
  uint mctop_has_mem_lat(mctop_t* topo);
  uint mctop_has_mem_bw(mctop_t* topo);
  uint mctop_ids_get_latency(mctop_t* topo, const uint id0, const uint id1);

  /* sibling getters ***************************************************************** */
  socket_t* mctop_sibling_get_other_socket(sibling_t* sibling, socket_t* socket);
  uint mctop_sibling_contains_sockets(sibling_t* sibling, socket_t* s0, socket_t* s1);

  /* optimizing ********************************************************************** */
  int mctop_hwcid_fix_numa_node(mctop_t* topo, const uint hwcid);



  static inline uint
  mctop_create_id(uint seq_id, uint lvl)
  {
    return ((lvl * MCTOP_LVL_ID_MULTI) + seq_id);
  }

  static inline uint
  mctop_id_no_lvl(uint id)
  {
    return (id % MCTOP_LVL_ID_MULTI);
  }

  static inline uint
  mctop_id_get_lvl(uint id)
  {
    return (id / MCTOP_LVL_ID_MULTI);
  }

  static inline void
  mctop_print_id(uint id)
  {
    uint sid = mctop_id_no_lvl(id);
    uint lvl = mctop_id_get_lvl(id);
    printk("%u-%04u", lvl, sid);
  }

#define MCTOP_ID_PRINTER "%u-%04u"
#define MCTOP_ID_PRINT(id)  mctop_id_get_lvl(id), mctop_id_no_lvl(id)


  /* ******************************************************************************** */
  /* MCTOP Placement */
  /* ******************************************************************************** */
  int mctop_run_on_socket(mctop_t* topo, const uint socket_n);
  int mctop_run_on_socket_nm(mctop_t* topo, const uint socket_n); /* doea not set preferred node */
  int mctop_run_on_node(mctop_t* topo, const uint node_n);

  int mctop_run_on_socket_ref(socket_t* socket, const uint fix_mem);

  extern int mctop_set_cpu(mctop_t* topo, int cpu);


#if defined(__i386__)
  static inline mctop_ticks
  mctop_getticks(void)
  {
    mctop_ticks ret;

    __asm__ __volatile__("rdtsc" : "=A" (ret));
    return ret;
  }
#elif defined(__x86_64__)
  static inline mctop_ticks
  mctop_getticks(void)
  {
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
  }
#elif defined(__sparc__)
  static inline mctop_ticks
  mctop_getticks()
  {
    mctop_ticks ret = 0;
    __asm__ __volatile__ ("rd %%tick, %0" : "=r" (ret) : "0" (ret));
    return ret;
  }
#elif defined(__tile__)
#include <arch/cycle.h>
  static inline mctop_ticks
  mctop_getticks()
  {
    return get_cycle_count();
  }
#endif

#define MCTOP_PROF_STEP 0
#if MCTOP_PROF_STEP == 1
#define MCTOP_F_STEP(__steps, __a, __b)		\
  mctop_ticks __steps = 0, __b, __a = mctop_getticks();	    
  
#define MCTOP_P_STEP(str, __steps, __a, __b, doit)			\
  {									\
    if (doit)								\
      {									\
       __b = mctop_getticks();						\
       mctop_ticks __d = __b - __a;					\
       printk("Step %2zu (%-24s): %-10zu cycles ~= %10f ms\n",		\
        __steps++, str, __d, __d / 2100000.0);			\
       __a = __b;							\
       __b = mctop_getticks();						\
     }									\
   }
#define MCTOP_P_STEP_ND(str, node, __steps, __a, __b, doit)		\
   {									\
    if (doit)								\
      {									\
       __b = mctop_getticks();						\
       mctop_ticks __d = __b - __a;					\
       printk("Step %2zu (%-24s): %-10zu cycles ~= %10f ms :: on %2u node\n", \
        __steps++, str, __d, __d / 2100000.0, node);		\
       __a = __b;							\
       __b = mctop_getticks();						\
     }									\
   }
#else
#define MCTOP_F_STEP(__steps, __a, __b)
#define MCTOP_P_STEP(str, __steps, __a, __b, doit) 
#define MCTOP_P_STEP_ND(str, node, __steps, __a, __b, doit)
#endif









/* Added by KevinJio */

/* The following is the content of mctop_serialization.h*/
#define TOPOLOGY_ARRAY_SIZE 1200     /* The maximum size of the storage array */
#define NULL_ID -1                      /* Non existing related field id representation */
#define MAX_HWCS_GRPS_NB 50             /* Max number of intermediate hardware contexts (cores) */
#define MAX_SIBLINGS_NB 20              /* Max number of siblings relationships */

#define VERBOSE_PRINT 0

#define COPY_AND_SHIFT_DEST(dest, src, len) do{ \
    memcpy(*dest, src, len);                    \
    *dest += len;                               \
} while(0)
    /* printk("&&&&&&&&&& dest: %p\tsrc: %p\n", *dest, src); */

#define SERIALIZE_FIELD(field) do{          \
    len = sizeof(field);                    \
    src = (char*) &(field);                 \
    COPY_AND_SHIFT_DEST(dest, src, len);    \
    if(VERBOSE_PRINT){                      \
        printk("SERIALIZE_FIELD ");         \
        printk(#field);                     \
        printk("\n");                       \
    }                                       \
} while(0)
        /*printk("\t%p\n", field);            \*/

#define SERIALIZE_ARRAY_FIELD(field, len) do{   \
    src = (char*) field;                        \
    COPY_AND_SHIFT_DEST(dest, src, len);        \
} while(0)

#define SERIALIZE_RELATED_FIELD(field) do{  \
    len = sizeof(uint);                     \
    if(field){                              \
        id = (field)->id;                   \
        src = (char*) &id;                  \
    } else {                                \
        id = NULL_ID;                       \
        src = (char*) &id;                  \
    }                                       \
    COPY_AND_SHIFT_DEST(dest, src, len);    \
    if(VERBOSE_PRINT){                      \
        printk("SERIALIZE_RELATED_FIELD "); \
        printk(#field);                     \
        printk("->id\t%d\n", id);           \
    }                                       \
} while(0)


#define ALLOC(field) do{                            \
    field = (typeof(field)) kmalloc(sizeof(field), GFP_KERNEL);  \
} while(0)


#define COPY_AND_SHIFT_SRC(dest, src, len) do{      \
    memcpy(dest, *src, len);                        \
    *src += len;                                    \
} while(0)
    /* printk("&&&&&&&&&& dest: %p\tsrc: %p\n", dest, *src); */

#define DESERIALIZE_FIELD(field) do{    \
    len = sizeof(field);                \
    dest = (char*) &(field);            \
    COPY_AND_SHIFT_SRC(dest, src, len); \
    if(VERBOSE_PRINT){                      \
        printk("DESERIALIZE_FIELD ");   \
        printk(#field);                 \
        printk("\n");                   \
    }                                   \
} while(0)
        /*printk("\t%p\n", field);        \*/

#define DESERIALIZE_ARRAY_FIELD(field, len) do{ \
    field = (typeof(field)) kmalloc(len, GFP_KERNEL);        \
    dest = (char*) field;                       \
    COPY_AND_SHIFT_SRC(dest, src, len);         \
} while(0)
    /*field = xmalloc_array(typeof(*field), len/sizeof(*field));        \*/


#define GET_ID_IN_FIELD(field) memcpy(&id, &field, 4)

#define DEBUG_PRINT(string) if(VERBOSE_PRINT) printk("######### %s #########\n", string)


void serialize_mctop(mctop_t* mctop, char** dest);
void serialize_hwc(hw_context_t* hwc, char** dest);
void serialize_socket(socket_t* socket, char** dest, uint has_mem);
void serialize_sibling(sibling_t* sibling, char** dest);
void serialize_cache_info(mctop_cache_info_t* cache_info, char** dest);

mctop_t* deserialize_mctop(mctop_t* mctop, char** src);
hw_context_t* deserialize_hwc(hw_context_t* hwc, char** src);
socket_t* deserialize_socket(socket_t* socket, char** src, uint has_mem);
sibling_t* deserialize_sibling(sibling_t* sibling, char** src);
mctop_cache_info_t* deserialize_cache_info(mctop_cache_info_t* cache_info, char** src);

socket_t* get_socket_by_id(socket_t* sockets_array, int n, int id);
socket_t* get_socket_by_id_2(socket_t** sockets_array, int n, int id);
hwc_gs_t* get_hwc_gs_by_id(hwc_gs_t* hwcs_grps_array, int n, int id);
hwc_gs_t* get_hwc_gs_by_id_2(hwc_gs_t** hwcs_grps_array, int n, int id);
hw_context_t* get_hwc_by_id(hw_context_t* hwcs_array, int n, int id);
hw_context_t* get_hwc_by_id_2(hw_context_t** hwcs_array, int n, int id);
sibling_t* get_sibling_by_id(sibling_t* siblings_array, int n, int id);
sibling_t* get_sibling_by_id_2(sibling_t** siblings_array, int n, int id);

void reestablish_links_for_socket(socket_t* socket, mctop_t* mctop);

void init_null_ptrs_of_mctop(mctop_t* mctop);
void init_null_ptrs_of_hwc(hw_context_t* hwc);
void init_null_ptrs_of_socket(socket_t* socket);
void init_null_ptrs_of_hwc_grp(hwc_gs_t* hwc_gs);
void init_null_ptrs_of_sibling(sibling_t* sibling);
void init_null_ptrs_of_cache_info(mctop_cache_info_t* cache_info);

void free_mctop(mctop_t* mctop, int free_whole_topology);
void free_socket(socket_t* socket);



// Used to maintain the pids of the programs that are using the mctop topology
typedef struct task_list{
  struct task_struct *task;
  struct task_list* prev;
  struct task_list* next;
  int pid;
} task_list_t;

/* End of the addition */


#ifdef __cplusplus
 }
#endif

#endif	/* __H_MCTOP__ */


