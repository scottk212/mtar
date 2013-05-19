/*-
 * Copyright (c) 2013 John Holdsworth
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#undef VERSION
#include "pigz.c"

#include <semaphore.h>

static struct {

    int compression_level, running;
    struct archive_write_filter *f;
    sem_t *can_write, *can_read;
    pthread_t pigz_thread;

    const char *buff;
    time_t timestamp;
    size_t bytes, pos;

} _data, *data = &_data;

static char read_sname[] = "/bsdtar.read", write_sname[] = "/bsdtar.write";

static ssize_t pigz_write(int fdout, const void *buff, size_t bytes ) {

    int ret = __archive_write_filter(data->f->next_filter, buff, bytes);
    if (ret != ARCHIVE_OK)
        fprintf(stderr, "pigz filter write error\n");

    return bytes;
}

static ssize_t pigz_read(int fdin, void *buff, size_t bytes) {

    sem_post(data->can_write);
    sem_wait(data->can_read);

    if ( data->bytes == 0 )
        return 0; /* EOF */

    size_t length = data->bytes-data->pos;
    if ( length > bytes )
        length = bytes;

    memcpy(buff, data->buff+data->pos, length );
    data->pos += length;
    
    int done = data->pos == data->bytes;
    if ( done )
        sem_post( data->can_write );

    return length;
}

static void *pigz_thread( void *arg ) {

	struct archive_write *a = (struct archive_write *)data->f->archive;
    struct write_file_data {
        int		fd;
        struct archive_mstring filename;
    } *write_file_data = (struct write_file_data *)a->client_data;

    const char *filename = "";
    archive_mstring_get_mbs( data->f->archive, &write_file_data->filename, &filename );
    char tarname[PATH_MAX], *dot = strrchr( filename, '.' );

    /* try to set name of file zippped correctly */
    if ( dot && (strcmp(dot, ".tgz") == 0 || //strcmp(dot, ".gz") == 0 ||
        ((dot = strchr(filename, '.')) && strcmp(dot, ".tar.gz") == 0) ||
        (dot = strrchr( filename, '.' ))) ) {
        snprintf(tarname, sizeof tarname, "%.*s.tar", (int)(dot-filename), filename);
        filename = tarname;
    }
    else
        filename = "mtar.tar";

    pigz_main(filename, data->timestamp, data->compression_level, pigz_read, pigz_write);
    return NULL;
}

/*
 * Write data to the compressed stream.
 */
static int
archive_compressor_pigz_write(struct archive_write_filter *f, const void *buff,
                              size_t length)
{
    if ( !data->running++ && pthread_create(&data->pigz_thread, NULL, pigz_thread, NULL) )
        fprintf(stderr, "pigz thread create error\n");

    sem_wait(data->can_write);

    data->buff = buff;
    data->bytes = length;
    data->pos = 0;

    while ( data->pos<length ) {
        sem_post(data->can_read);
        sem_wait(data->can_write);
    }

	return (ARCHIVE_OK);
}

/*
 * Finish the compression...
 */
static int
archive_compressor_pigz_close(struct archive_write_filter *f)
{
	int ret = 0, r1;

    sem_wait(data->can_write);
    data->bytes = 0;
    sem_post(data->can_read);
    sem_post(data->can_read);

    if ( pthread_join(data->pigz_thread, NULL) )
        fprintf(stderr, "pigz thread join error\n");

    finish_jobs();

	r1 = __archive_write_close_filter(f->next_filter);
	return (r1 < ret ? r1 : ret);
}

static int
archive_compressor_pigz_free(struct archive_write_filter *f)
{
    sem_close(data->can_read);
    sem_close(data->can_write);
    sem_unlink(read_sname);
    sem_unlink(write_sname);
	return (ARCHIVE_OK);
}

/*
 * Set write options.
 */
static int
archive_compressor_pigz_options(struct archive_write_filter *f, const char *key,
    const char *value)
{
	if (strcmp(key, "compression-level") == 0) {
		if (value == NULL || !(value[0] >= '0' && value[0] <= '9') ||
		    value[1] != '\0')
			return (ARCHIVE_WARN);
		data->compression_level = value[0] - '0';
		return (ARCHIVE_OK);
	}
	if (strcmp(key, "timestamp") == 0) {
		data->timestamp = (value == NULL)?0:time(NULL);
		return (ARCHIVE_OK);
	}

	/* Note: The "warn" return is just to inform the options
	 * supervisor that we didn't handle it.  It will generate
	 * a suitable error if no one used this option. */
	return (ARCHIVE_WARN);
}

/*
 * Setup callback.
 */
static int
archive_compressor_pigz_open(struct archive_write_filter *f)
{
	int ret;

	ret = __archive_write_open_filter(f->next_filter);
	if (ret != ARCHIVE_OK)
		return (ret);

    sem_unlink(read_sname);
    sem_unlink(write_sname);
    if ( !(data->can_read = sem_open(read_sname, O_CREAT, 0644, 0)) )
        fprintf(stderr, "can_read sem %s\n", strerror(errno));
    if ( !(data->can_write = sem_open(write_sname, O_CREAT, 0644, 0)) )
        fprintf(stderr, "can_write sem %s\n", strerror(errno));

    return ARCHIVE_OK;
}

/*
 * Add a pigz compression filter to this write handle.
 */
int
archive_write_add_filter_pigz(struct archive *_a)
{
	struct archive_write *a = (struct archive_write *)_a;
	struct archive_write_filter *f = __archive_write_allocate_filter(_a);
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC,
                        ARCHIVE_STATE_NEW, "archive_write_add_filter_pigz");

    data->f = f;
	f->data = data;
	f->open = &archive_compressor_pigz_open;
	f->options = &archive_compressor_pigz_options;
	f->write = &archive_compressor_pigz_write;
	f->close = &archive_compressor_pigz_close;
	f->free = &archive_compressor_pigz_free;
	f->code = ARCHIVE_FILTER_PIGZ;
	f->name = "pigz";

	data->compression_level = Z_DEFAULT_COMPRESSION;
	return (ARCHIVE_OK);
}
