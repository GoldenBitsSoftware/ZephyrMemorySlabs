# ZephyrMemorySlabs
Sample code to demonstrate the use of Zephyr memory slabs.

This code example shows how to use the Zephyr RTOS slab API.   A light weight wrapper API is used to provide a simple API to perform general buffer allocation.  In this example, slabs are not dedicated to a particular module as discussed previously. The API is just two functions:

     int slab_buf_alloc(void **bufptr, size_t buff_size);
     int slab_buf_free(void *bufptr);
