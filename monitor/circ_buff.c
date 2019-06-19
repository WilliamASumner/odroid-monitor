#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "circ_buff.h"


// Code adapted from https://embeddedartistry.com/blog/2017/4/6/circular-buffers-in-cc

/// Pass in a storage buffer and size
/// Returns a circular buffer handle
cbuf_handle_t circular_buf_init(u_int8_t* buffer, size_t size, FILE * file) {
    assert(buffer && size && file);

    cbuf_handle_t handle = (cbuf_handle_t)malloc(sizeof(circular_buf_t));
    assert(handle);

    handle->buffer = buffer;
    handle->max = size;
    handle->file = file;
    handle->curr_size = 0;
    circular_buf_reset(handle);

    assert(circular_buf_empty(handle));

    return handle;
}

/// Free a circular buffer structure.
/// Does not free data buffer; owner is responsible for that
void circular_buf_free(cbuf_handle_t cbuf) {
    assert(cbuf);
    assert(cbuf->file);
    fwrite(cbuf->buffer,sizeof(u_int8_t),cbuf->curr_size,cbuf->file); // write out the buffer
    free(cbuf);
}

/// Reset the circular buffer to empty, head == tail
void circular_buf_reset(cbuf_handle_t cbuf) {
    assert(cbuf);
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->full = false;
    cbuf->curr_size = 0;
}

/// Retrieve a value from the buffer
/// Returns 0 on success, -1 if the buffer is empty
int circular_buf_get(cbuf_handle_t cbuf, u_int8_t * data);

/// Returns true if the buffer is empty
bool circular_buf_empty(cbuf_handle_t cbuf) {
    assert(cbuf);
    return (cbuf->head == cbuf->tail) && !cbuf->full;
}

/// Returns true if the buffer is full
bool circular_buf_full(cbuf_handle_t cbuf) {
    assert(cbuf);
    return cbuf->full;
}

/// Returns the maximum capacity of the buffer
size_t circular_buf_capacity(cbuf_handle_t cbuf) {
    assert(cbuf);
    return cbuf->max;
}

/// Returns the current number of elements in the buffer
size_t circular_buf_size(cbuf_handle_t cbuf) {
    assert(cbuf);

    size_t size = cbuf->max;
    if(!cbuf->full) {
        if(cbuf->head >= cbuf->tail)
            size = (cbuf->head - cbuf->tail);
        else
            size = (cbuf->max + cbuf->head - cbuf->tail);
    }
    return size;
}

static void advance_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);

    if(cbuf->full)
       {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }

    cbuf->head = (cbuf->head + 1) % cbuf->max;
    cbuf->full = (cbuf->head == cbuf->tail);
}

static void retreat_pointer(cbuf_handle_t cbuf) {
    assert(cbuf);

    cbuf->full = false;
    cbuf->tail = (cbuf->tail + 1) % cbuf->max;
}

void circular_buf_put_byte(cbuf_handle_t cbuf, u_int8_t data) {
    assert(cbuf && cbuf->buffer);

    cbuf->buffer[cbuf->head] = data;
    cbuf->curr_size += 1;
    advance_pointer(cbuf);
}

int circular_buf_put_byte_checked(cbuf_handle_t cbuf, u_int8_t data) {
    assert(cbuf && cbuf->buffer);

    int return_val = -1;

    if(!circular_buf_full(cbuf)) {
        cbuf->buffer[cbuf->head] = data;
        advance_pointer(cbuf);
	cbuf->curr_size += 1;
        return_val = 0;
    }

    return return_val;
}

size_t circular_buf_put_bytes(cbuf_handle_t cbuf, u_int8_t * data, size_t num_bytes) {
    assert(cbuf && cbuf->buffer && data);
    assert(num_bytes > 0 && num_bytes < cbuf->max);

    size_t i, bytes_written = 0;
    for (i = 0; i < num_bytes; i++,bytes_written++) {
        circular_buf_put_byte(cbuf,data[i]);
    }

    return bytes_written;
}

size_t circular_buf_put_bytes_checked(cbuf_handle_t cbuf, u_int8_t * data, size_t num_bytes) {
    assert(cbuf && cbuf->buffer && data);
    assert(num_bytes > 0 && num_bytes < cbuf->max);

    size_t i, bytes_written = 0;
    for (i = 0; i < num_bytes; i++) {
        if (circular_buf_put_byte_checked(cbuf,data[i]) == 0)
            bytes_written++;
        else {
            fwrite(cbuf->buffer,sizeof(char),cbuf->max,cbuf->file); // write out the buffer
            circular_buf_reset(cbuf); // empty the buffer
            assert(circular_buf_put_byte_checked(cbuf,data[i]) == 0); // try writing again
        }
    }

    return bytes_written;
}

int circular_buf_get(cbuf_handle_t cbuf, u_int8_t * data) {
    assert(cbuf && data && cbuf->buffer);

    int return_val = -1;

    if(!circular_buf_empty(cbuf)) {
        *data = cbuf->buffer[cbuf->tail];
        retreat_pointer(cbuf);

        return_val = 0;
    }

    return return_val;
}

