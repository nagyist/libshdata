/**
 * Copyright (c) 2015 Parrot S.A.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @file shd_sync.c
 *
 */

#include <stddef.h>		/* For NULL pointer */
#include <stdlib.h>		/* For memory allocation functions */
#include <errno.h>

#include "shd_private.h"
#include "shd_utils.h"
#include "shd_data.h"
#include "shd_section.h"
#include "shd_sync.h"

static int builtin_add_and_fetch(int *ptr, int value)
{
	return __sync_add_and_fetch(ptr, value);
}

int shd_sync_hdr_init(struct shd_sync_hdr *sync_hdr)
{
	sync_hdr->write_index = -1;
	sync_hdr->nb_ongoing_writes = 0;

	return 0;
}

struct shd_sync_ctx *shd_sync_ctx_new(const struct shd_section_id *id)
{
	struct shd_sync_ctx *ctx;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL)
		return NULL;

	ctx->index = -1;
	ctx->prev_index = -1;
	ctx->primitives = id->primitives;

	return ctx;
}

int shd_sync_ctx_destroy(struct shd_sync_ctx *ctx)
{
	if (ctx != NULL)
		free(ctx);

	return 0;
}

int shd_sync_start_write_session(struct shd_sync_ctx *ctx,
				 struct shd_sync_hdr *hdr)
{
	/* Number of writes currently going on is atomically incremented, and
	 * its new value should be 1 (since only one write can be in progress
	 * at a given moment) */
	if (ctx->primitives.add_and_fetch(&hdr->nb_ongoing_writes, 1) != 1) {
		if (ctx->index != -1) {
			ULOGW("A write is already in progress within "
					"this producer");
			return -EALREADY;
		} else {
			ULOGE("Abnormal write already in progress by another "
					"producer");
			return -EPERM;
		}
	}

	return 0;
}

int shd_sync_start_sample_write(struct shd_sync_ctx *ctx,
				struct shd_sync_hdr *hdr,
				struct shd_sync_sample *samp,
				const struct shd_data_section_desc *desc)
{
	uint32_t unexpected_samples;

	if (ctx == NULL || hdr == NULL)
		return -EINVAL;
	if (ctx->index != -1)
		return -EALREADY;

	/* Invalidate sample */
	ctx->primitives.add_and_fetch(&samp->nb_writes, 1);
	/* Update local index */
	index_increment_from(&ctx->index, &hdr->write_index, desc->nb_samples);

	unexpected_samples = interval_between(ctx->prev_index,
					ctx->index,
					desc->nb_samples) - 1;

	/* Only one sample should have been written since the previous write,
	 * otherwise another thread has written into the section */
	if (unexpected_samples > 0 && ctx->prev_index != -1) {
		ULOGW("%d sample(s) unexpectedly written since last write "
				"operation in this thread",
				unexpected_samples);
		return -EFAULT;
	}

	ULOGD("Starting write on sample at index : %d, nb_writes : %d",
			ctx->index, samp->nb_writes);

	return 0;
}

int shd_sync_end_write_session(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr)
{
	if (ctx == NULL || hdr == NULL)
		return -EINVAL;

	ULOGD("End of write on sample at index : %d", ctx->index);
	hdr->write_index = ctx->index;
	hdr->nb_ongoing_writes = 0;
	ctx->prev_index = ctx->index;
	ctx->index = -1;
	return 0;
}

int shd_sync_start_read_session(struct shd_sync_ctx *ctx,
					struct shd_sync_sample *samp)
{
	if (ctx == NULL || samp == NULL)
		return -EINVAL;

	ctx->nb_writes = samp->nb_writes;
	ULOGD("Starting read session, nb_writes = %d", ctx->nb_writes);
	return 0;
}

int shd_sync_end_read_session(struct shd_sync_ctx *ctx,
					struct shd_sync_sample *samp)
{
	int ret = -1;
	if (ctx == NULL || samp == NULL) {
		ret = -EINVAL;
		goto exit;
	}

	if (ctx->nb_writes != samp->nb_writes) {
		ULOGW("Current sample has been overwritten during read : "
			"expected value : %d, read : %d", ctx->nb_writes,
							samp->nb_writes);
		ret = -EFAULT;
	} else {
		ret = 0;
	}

	ctx->nb_writes = -1;

exit:
	return ret;
}

int shd_sync_get_nb_writes(const struct shd_sync_sample *samp)
{
	return samp->nb_writes;
}

int shd_sync_invalidate_sample(struct shd_sync_sample *samp)
{
	samp->nb_writes = -1;
	return 0;
}

bool shd_sync_is_sample_valid(const struct shd_sync_sample *samp)
{
	return samp->nb_writes != -1;
}

int shd_sync_invalidate_section(struct shd_sync_ctx *ctx,
				struct shd_sync_hdr *hdr, bool creation)
{
	if (ctx == NULL || hdr == NULL)
		return -EINVAL;

	if (creation)
		hdr->revision.nb_creations = 1;
	else
		ctx->primitives.add_and_fetch(&hdr->revision.nb_creations, 1);
	return 0;
}

int shd_sync_update_global_revision_nb(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr)
{
	if (ctx == NULL || hdr == NULL)
		return -EINVAL;

	/* The revision number should be odd after a call to this function */
	if (ctx->primitives.add_and_fetch(&hdr->revision.nb_creations, 1) % 2
			!= 0)
		return -EFAULT;
	else
		return hdr->revision.nb_creations;
}

int shd_sync_update_local_revision_nb(struct shd_sync_ctx *ctx,
					struct shd_sync_hdr *hdr)
{
	/* If the revision number is odd, the section is being updated */
	if (hdr->revision.nb_creations % 2 == 1)
		return -EAGAIN;

	ctx->revision.nb_creations = hdr->revision.nb_creations;
	return ctx->revision.nb_creations;
}

int shd_sync_check_revision_nb(struct shd_revision *rev,
					struct shd_sync_hdr *hdr)
{
	if (rev->nb_creations == hdr->revision.nb_creations)
		return 0;
	else
		return -ENODEV;
}

int shd_sync_get_local_write_index(const struct shd_sync_ctx *ctx)
{
	if (ctx == NULL)
		return -1;

	return ctx->index;
}

int shd_sync_get_next_write_index(const struct shd_sync_hdr *hdr,
				const struct shd_data_section_desc *desc)
{
	if (hdr == NULL)
		return -1;

	return index_next(hdr->write_index, desc->nb_samples);
}

int shd_sync_get_last_write_index(const struct shd_sync_hdr *hdr)
{
	if (hdr == NULL)
		return -1;

	return hdr->write_index;
}

int shd_sync_primitives_set_builtin(struct shd_sync_primitives *primitives)
{
	if (!primitives)
		return -EINVAL;

	primitives->add_and_fetch = builtin_add_and_fetch;

	return 0;
}
