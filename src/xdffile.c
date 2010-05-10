#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>


/* To support those $!%@!!! systems that are not POSIX compliant
and that distinguish between text and binary files */
#ifndef O_BINARY
# ifdef _O_BINARY
#  define O_BINARY _O_BINARY
# else
#  define O_BINARY 0
# endif
#endif /* O_BINARY */

#include "xdfio.h"
#include "xdftypes.h"
#include "xdffile.h"
#include "xdferror.h"

static const char xdffileio_string[] = PACKAGE_STRING;

#define ORDER_QUIT	2
#define ORDER_TRANSFER	1
#define ORDER_NONE	0

struct data_batch {
	unsigned int len;
	unsigned int iarray;
	unsigned int foff, moff, mskip;
};

struct convertion_data {
	struct convprm prm;
//	unsigned int stride;
	unsigned int filetypesize, memtypesize;
};


static int write_record_to_disk(struct xdffile* xdf);
static int read_record_from_disk(struct xdffile* xdf);
static void* transfer_thread_fn(void* ptr);

/*! \param xdf	pointer to a valid xdffile structure
 * 
 * Transpose recorde data from (channel,sample) to a (sample,channel)
 * organisation, performs any necessary conversion and write the record on
 * the file.
 */
static int write_record_to_disk(struct xdffile* xdf)
{
	ssize_t wsize;
	size_t reqsize;
	unsigned ich;
	struct convertion_data* ch;
	char *fbuff, *src = xdf->backbuff;
	void* dst = xdf->tmpbuff[0];
	void* buff = xdf->tmpbuff[1];

	// Transfer, convert and copy to file each channel data
	for (ich = 0; ich < xdf->numch; ich++) {
		ch = xdf->convdata + ich;

		// Convert data
		transconv_data(xdf->ns_per_rec, dst, src, &(ch->prm), buff);

		// Write the converted data to the file. Continue writing
		// as long as not all data has been written
		reqsize = xdf->ns_per_rec * ch->filetypesize;
		fbuff = dst;
		do {
			wsize = write(xdf->fd, fbuff, reqsize);
			if (wsize == -1)
				return set_xdf_error(xdf, errno);
			reqsize -= wsize;
			fbuff += wsize;
		} while (reqsize);
	}

	// Make sure that the whole record has been sent to hardware
	fsync(xdf->fd);

	return 0;
}

static int read_record_from_disk(struct xdffile* xdf)
{
	(void)xdf;
	return 0;
}


/*! \param ptr	pointer to a valid xdffile structure
 *
 * This is the function implementing the background thread transfering data
 * from/to the underlying file. 
 * This performs the transfer of the back buffer whenever the condition is
 * signaled and order is ORDER_TRANSFER. The end of the transfer is notified by raising
 * the semaphore
 */
static void* transfer_thread_fn(void* ptr)
{
	int ret;
	struct xdffile* xdf = ptr;

	// Once the routine hold the mutex, it is in a ready state, notify
	// the main thread with the semaphore
 	pthread_mutex_lock(&(xdf->mtx));
	sem_post(&(xdf->sem));
	
 	// While a transfer is performed, this routine holds the mutex
	// preventing from any early buffer swap
	while (1) {
		// Wait for an order of transfer
		while (!xdf->order)
			pthread_cond_wait(&(xdf->cond), &(xdf->mtx));
	
		// break the transfer loop if the quit order has been sent
		if (xdf->order == ORDER_QUIT)
			break;

		// Write/Read a record
		// TODO handle failure in better way
		if (xdf->mode == XDF_WRITE)
			ret = write_record_to_disk(xdf);
		else
			ret = read_record_from_disk(xdf);

		// The transfer has been performed => clear the order
		// and notify the main thread through the semaphore
		xdf->order = 0;
		sem_post(&(xdf->sem));
	}
	pthread_mutex_unlock(&(xdf->mtx));
	return NULL;
}


/*! \param xdf	pointer to a valid xdffile structure
 *
 * Notify the background thread that a record has to be written or read,
 * depending on the mode of the xdf structure. This function will block if
 * the previous transfer is still being performed.
 */
static int disk_transfer(struct xdffile* xdf)
{
	void* buffer;

	// Wait for the previous operation to be finished
	sem_wait(&(xdf->sem));
	
	// If the mutex is hold by someone else, it means that the transfer
	// routine has still not in a ready state
	pthread_mutex_lock(&(xdf->mtx));

	// Swap front and back buffer
	buffer = xdf->backbuff;
	xdf->backbuff = xdf->buff;
	xdf->buff = buffer;

	// Signal for data transfer
	xdf->order = ORDER_TRANSFER;
	pthread_cond_signal(&(xdf->cond));

	// We are safe now, the transfer can start from now
	pthread_mutex_unlock(&(xdf->mtx));

	return 0;
}

static void reset_batch(struct data_batch* batch, unsigned int iarray, unsigned int foff)
{
	memset(batch, 0, sizeof(*batch));
	batch->iarray = iarray;
	batch->foff = foff;
	batch->len = 0;
}

static int add_to_batch(struct data_batch *curr, const struct xdf_channel *ch, unsigned int foff)
{
	unsigned int datalen = get_data_size(ch->inmemtype);

	if (!curr)
		return 0;


	if ((curr->iarray == ch->iarray) || (curr->len == 0)) {
		// Check for the start of the batch
		if (curr->len == 0) {
			curr->iarray = ch->iarray;
			curr->foff = foff;
			curr->moff = ch->offset;
			curr->len = datalen;
			return 1;
		}

		// Add channel to batch
	    	if ((curr->foff+curr->len == foff)
	    	   && (curr->moff+curr->len == ch->offset)) {
			curr->len += datalen;
			return 1;
		}
	}
	return 0;
}

static void link_batches(struct xdffile* xdf, unsigned int nbatch)
{
	unsigned int i;
	struct data_batch* batch = xdf->batch;
	unsigned int* stride = xdf->array_stride; 

	if (!nbatch)
		return;

	for (i=0; i<nbatch-1; i++) {
		if (batch[i].iarray == batch[i+1].iarray)
			batch[i].mskip = batch[i+1].moff - batch[i].moff;
		else
			batch[i].mskip = stride[batch[i].iarray] - batch[i].moff;
	}
	batch[nbatch-1].mskip = stride[batch[nbatch-1].iarray] - batch[nbatch-1].moff;
}

static int compute_batches(struct xdffile* xdf, int assign)
{
	struct data_batch curr, *currb;
	unsigned int nbatch = 0, iarr, moff, foff, dlen;
	const struct xdf_channel* ch;

	currb = assign ? xdf->batch : &curr;
	reset_batch(&curr, 0, 0);

	for (iarr=0; iarr < xdf->narrays; iarr++) {
		moff = foff = 0;
		
		// Scan channels in the xdffile order to find different batches
		for (ch=xdf->channels; ch; ch=ch->next) {
			foff += dlen = get_data_size(ch->inmemtype);

			// Consistency checks
			if (ch->iarray > xdf->narrays
			    || ch->offset + dlen > xdf->array_stride[ch->iarray])
				return -1;

			// Linearize the processing of channel sourcing
			// the same input array
			if ((iarr == ch->iarray)
			   && !add_to_batch(currb, ch, foff)) {
				nbatch++;
				if (assign)
					currb++;
				reset_batch(currb, iarr, foff);
				add_to_batch(currb, ch, foff);
			}
		}
	}
	if (assign)
		link_batches(xdf, nbatch);

	return nbatch;
}

static void setup_convdata(struct xdffile* xdf)
{
	unsigned int i, in_str, out_str;
	enum xdftype in_tp, out_tp;
	const double *in_mm, *out_mm;
	struct xdf_channel* ch = xdf->channels;

	for (i=0; i<xdf->numch; i++) {
		if (xdf->mode == XDF_WRITE) {
			// In write mode, convertion in 
			// from mem/physical to file/digital
			in_tp = ch->inmemtype;
			in_str = xdf->sample_size;
			in_mm = ch->physical_mm;
			out_tp = ch->infiletype;
			out_str = get_data_size(out_tp);
			out_mm = ch->digital_mm;
		} else {
			// In read mode, convertion in 
			// from file/digital to mem/physical
			in_tp = ch->infiletype;
			in_str = get_data_size(out_tp);
			in_mm = ch->digital_mm;
			out_tp = ch->inmemtype;
			out_str = xdf->sample_size;
			out_mm = ch->physical_mm;
		}
		
		xdf->convdata[i].filetypesize = get_data_size(ch->infiletype);
		xdf->convdata[i].memtypesize = get_data_size(ch->inmemtype);
		setup_transform(&(xdf->convdata[i].prm),
		                in_str, in_tp, in_mm,
		                out_str, out_tp, out_mm);
		ch = ch->next;
	}
}

static int setup_transfer_thread(struct xdffile* xdf)
{
	int ret;
	int minit = 0, cinit = 0;

	xdf->order = ORDER_NONE;
	if ((ret = pthread_mutex_init(&(xdf->mtx), NULL)))
		goto error;
	minit = 1;

	if ((ret = pthread_cond_init(&(xdf->cond), NULL)))
		goto error;
	cinit = 1;

	if ((ret = pthread_create(&(xdf->thid), NULL, transfer_thread_fn, xdf)))
		goto error;

	return 0;

error:
	if (minit)
		pthread_mutex_destroy(&(xdf->mtx));
	if (cinit)
		pthread_cond_destroy(&(xdf->cond));
	set_xdf_error(xdf, ret);
	return -1;
}

static int finish_record(struct xdffile* xdf)
{
	char* buffer = (char*)xdf->buff + xdf->sample_size * xdf->ns_buff;
	unsigned int ns = xdf->ns_buff - xdf->ns_per_rec;
	int retval;

	if (!xdf->ns_buff)
		return 0;

	// Fill the remaining of the record with 0 values
	do {
		memset(buffer, 0, xdf->sample_size);
		buffer += xdf->sample_size;
	} while (--ns);

	retval = disk_transfer(xdf);
	xdf->ns_buff = 0;
	return retval;
}

int xdf_close(struct xdffile* xdf)
{
	int fd;

	if (!xdf)
		return set_xdf_error(NULL, EBADF);

	fd = xdf->fd;

	if (xdf->ready) {
		if (xdf->mode == XDF_WRITE) {
			if (finish_record(xdf))
				return -1;
		}

		// Wait for the last transfer to be done and 
		sem_wait(&(xdf->sem));

		// Stop the transfer thread and wait for its end
		pthread_mutex_lock(&(xdf->mtx));
		xdf->order = ORDER_QUIT;
		pthread_cond_signal(&(xdf->cond));
		pthread_mutex_unlock(&(xdf->mtx));
		pthread_join(xdf->thid, NULL);

		// Destroy synchronization primitives
		pthread_mutex_destroy(&(xdf->mtx));
		pthread_cond_destroy(&(xdf->cond));
		sem_close(&(xdf->sem));
	}

	// Finish and close the file
	xdf->ops->close_file(xdf);

	// TODO Handle close failure
	close(fd);

	return 0;
}

int xdf_define_arrays(struct xdffile* xdf, unsigned int numarrays, unsigned int* strides)
{
	unsigned int* newstrides;
	if (!(newstrides = malloc(numarrays*sizeof(*(xdf->array_stride)))))
		return -1;

	free(xdf->array_stride);
	xdf->array_stride = newstrides;
	xdf->narrays = numarrays;
	memcpy(xdf->array_stride, strides, numarrays*sizeof(*(xdf->array_stride)));

	return 0;
}



/*!
 * xdf->array_pos, xdf->convdata and xdf->batch are assumed to be NULL
 */
int xdf_prepare_transfer(struct xdffile* xdf)
{
	int nbatch;
	if (xdf->ready)
		return -1;

	// Compute the number of batches. Don't assign since no memory is
	// allocated for them in the xdf
	if ( (nbatch = compute_batches(xdf, 0)) < 0 )
		goto error;

	// Alloc of temporary entities needed for convertion
	if ( !(xdf->array_pos = malloc(xdf->narrays*sizeof(*(xdf->array_pos))))
  	    || !(xdf->convdata = malloc(xdf->numch*sizeof(*(xdf->convdata))))
	    || !(xdf->batch = malloc(xdf->nbatch*sizeof(*(xdf->batch)))) ) {
	    	
		goto error;
	}

	// Setup batches, convertion parameters
	xdf->nbatch = nbatch;
	compute_batches(xdf, 1); // assign batches: memory is now allocated
	setup_convdata(xdf);

	// Write header if open for writing 
	if (xdf->mode == XDF_WRITE)
		if (xdf->ops->write_header(xdf))
			goto error;

	if (setup_transfer_thread(xdf))
		goto error;

	xdf->ready = 1;
	return 0;

error:
	free(xdf->array_pos);
	free(xdf->convdata);
	free(xdf->batch);
	xdf->nbatch = 0;
	xdf->array_pos = NULL;
	xdf->convdata = NULL;
	xdf->batch = NULL;
	return -1;
}

/*! \param xdf 	pointer to a valid xdffile structure
 *  \param ns	number of samples to be added
 *  \param other pointer to the arrays holding the input samples
 *
 * Add samples coming from one or several input arrays containing the
 * samples. The number of arrays that must be provided on the call depends
 * on the specification of the channels.
 *
 * \warning Make sure the mode of the xdf is XDF_WRITE 
 */
int xdf_write(struct xdffile* xdf, unsigned int ns, ...)
{
	unsigned int i, k, ia, ns_buff = xdf->ns_buff, nbatch = xdf->nbatch;
	char* buffer = (char*)xdf->buff + xdf->sample_size * xdf->ns_buff;
	const char** buff_in = xdf->array_pos;
	struct data_batch* batch = xdf->batch;
	va_list ap;

	// Initialization of the input buffers
	va_start(ap, ns);
	for (ia=0; ia<xdf->num_array_input; ia++)
		buff_in[ia] = va_arg(ap, const char*);
	va_end(ap);

	for (i=0; i<ns; i++) {
		// Transfer the sample to the buffer by chunk
		for (k=0; k<nbatch; k++) {
			ia = batch[k].iarray;
			memcpy(buffer+batch[k].foff, buff_in[ia], batch[k].len);
			buff_in[ia] += batch[k].mskip;
		}
		buffer += xdf->sample_size;

		// Write the content of the buffer if full
		if (++ns_buff == xdf->ns_per_rec) {
			disk_transfer(xdf);
			buffer = xdf->buff;
			ns_buff = 0;
		}
	}

	xdf->ns_buff = ns_buff;
	return 0;
}


const char* xdf_get_string(void)
{
	return xdffileio_string;
}
