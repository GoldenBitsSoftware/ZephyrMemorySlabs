/*
 * Copyright (c) 2023 Golden Bits Software, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>


#define SLAB_ALIGNMENT              (4U)

/**
 * Struct to contain slab pool ptr, handy way to keep pool
 * with buffer pointer.  Provides nice check when freeing
 * slab, ensures slab freed to correct slab pool. Example: Freeing
 * large slab to small slab pool.
 */
struct __attribute__((aligned(SLAB_ALIGNMENT), packed))  slab_info {
    struct k_mem_slab *slab_pool;
};

#define SLAB_INFO_LEN               (sizeof(struct slab_info))

#define SMALL_SLAB_SIZE             (64U + SLAB_INFO_LEN)
#define MEDIUM_SLAB_SIZE            (256U + SLAB_INFO_LEN)
#define LARGE_SLAB_SIZE             (1024U + SLAB_INFO_LEN)

/* maximum buffer size possible to alloc */
#define MAX_BUFFER_LEN              (LARGE_SLAB_SIZE - SLAB_INFO_LEN)

#define NUM_SLABS                   (10u)


/**
 * Simple slab API, only two functions.
 */
int slab_buf_alloc(void **bufptr, size_t buff_size);
int slab_buf_free(void *bufptr);


LOG_MODULE_REGISTER(slab_demo);

/**
 * Statically define three slab pools, small, medium, large
 */
K_MEM_SLAB_DEFINE(small_slab_pool, SMALL_SLAB_SIZE, NUM_SLABS, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(medium_slab_pool, MEDIUM_SLAB_SIZE, NUM_SLABS, SLAB_ALIGNMENT);
K_MEM_SLAB_DEFINE(large_slab_pool, LARGE_SLAB_SIZE, NUM_SLABS, SLAB_ALIGNMENT);


/**
 * Returns slab pool buffer which can accommodate byte requset.
 *
 * @param buff_size Size to alloc.
 *
 * @return Slab pool pointer on success, else NULL.
 */
static struct k_mem_slab *slab_buf_get_pool(size_t buff_size)
{
    if (buff_size <= (SMALL_SLAB_SIZE - SLAB_INFO_LEN)) {
        // can we alloc from small buffer?
        if (k_mem_slab_num_free_get(&small_slab_pool) != 0) {
            return &small_slab_pool;
        }
    }

    /* Fall through if not enough small slabs to accommodate request.
     * Try next larger slab pool.
     */
    if (buff_size <= (MEDIUM_SLAB_SIZE - SLAB_INFO_LEN)) {
        // can we alloc from small buffer?
        if (k_mem_slab_num_free_get(&medium_slab_pool) != 0) {
            return &medium_slab_pool;
        }
    }

    if (buff_size <= (LARGE_SLAB_SIZE - SLAB_INFO_LEN)) {
        // can we alloc from small buffer?
        if (k_mem_slab_num_free_get(&large_slab_pool) != 0) {
            return &large_slab_pool;
        }
    }

    /* no free slab buffers */
    return NULL;
}


/**
 * Simple slab API, hides the details of allocating and freeing slabs
 */

/**
 * Allocate a slab buffer of buf_size
 *
 * @param bufptr     Pointer to allocated buffer return here.
 * @param buf_size   Byte length of buffer requested.
 *
 * @return 0 on success, else negative error number.
 */
int slab_buf_alloc(void **bufptr, size_t buf_size)
{
    struct slab_info *s_info;
    int retval;

    /* check input params */

    if ((bufptr == NULL) || (buf_size > MAX_BUFFER_LEN)) {
        return -EINVAL;
    }

    /* Get a pool that will satisfy the byte request */
    struct k_mem_slab *slab_pool_ptr = slab_buf_get_pool(buf_size);

    /* if out of slab memory */
    if (slab_pool_ptr == NULL) {
        return -ENOMEM;
    }

    retval = k_mem_slab_alloc(slab_pool_ptr, (void **)&s_info, K_FOREVER);

    if (retval == 0) {
        s_info->slab_pool = slab_pool_ptr;
        *bufptr = (s_info + 1);  /* usable buffer after info struct */
    }

    return retval;
}


/**
 * Frees slab buffer.
 *
 * @param buf_ptr  Pointer to the buffer.
 *
 * @return 0 on success, else negative error number.
 */
int slab_buf_free(void *buf_ptr)
{
    struct slab_info *s_info;

    if (buf_ptr == NULL) {
        return -EINVAL;
    }

    s_info = (struct slab_info *)buf_ptr;
    s_info--; /* info before usable buffer */

    /* validate k_mem_slab pointer */
    if ((s_info->slab_pool != &small_slab_pool) &&
            (s_info->slab_pool != &medium_slab_pool) &&
            (s_info->slab_pool != &large_slab_pool)) {

        LOG_ERR("slab_free() failed, invalid buffer");
        return -EINVAL;
    }

    k_mem_slab_free(s_info->slab_pool, &buf_ptr);

    return 0;
}

/* max number of possible slab buffers */
#define SLAB_BUFFER_TEST_CNT           (NUM_SLABS * 3U)

/**
 * Demonstrates using Zephyr slabs.
 */
void slab_demo(void)
{
    int retval;
    uint32_t cnt;
    size_t test_alloc_size = 20;
    void *buffers[SLAB_BUFFER_TEST_CNT] = {0};

    /**
     * Allocate from pools, this should success.  Future tests
     * should try different buffer sizes.
     */
    for (cnt = 0; cnt < SLAB_BUFFER_TEST_CNT; cnt++) {

        retval = slab_buf_alloc(&buffers[cnt], test_alloc_size);

        if (retval != 0) {
            LOG_ERR("Failed to allocate slab.");
            break;
        }
    }

    /**
     * Test slabs are allocated, ready for use.
     * For demonstration purposes, we'll just fill the slabs with
     * dummy data.
     */
    for (cnt = 0; cnt < SLAB_BUFFER_TEST_CNT; cnt++) {
        memset(buffers[cnt], 42, SLAB_BUFFER_TEST_CNT);
    }

    /* Free slab buffers */
    for (cnt = 0; cnt < SLAB_BUFFER_TEST_CNT; cnt++) {

        retval = slab_buf_free(buffers[cnt]);

        if (retval != 0) {
            LOG_ERR("Failed to free slab buffer");
        }
    }

}
