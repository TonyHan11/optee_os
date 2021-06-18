// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2021, Microchip
 */

#include <drivers/clk.h>
#include <drivers/scmi.h>
#include <drivers/scmi-msg.h>
#include <kernel/boot.h>
#include <string.h>
#include <sys/queue.h>

struct scmi_clk {
	struct clk *clk;
	unsigned int channel_id;
	unsigned int scmi_id;
	bool enabled;
	SLIST_ENTRY(scmi_clk) link;
};

static SLIST_HEAD(, scmi_clk) scmi_clk_list =
					SLIST_HEAD_INITIALIZER(scmi_clk_list);

#define SCMI_MAX_CLK_NAME_LEN	16

size_t plat_scmi_clock_count(unsigned int channel_id)
{
	unsigned int count = 0;
	unsigned int max_id = 0;
	struct scmi_clk *clk = NULL;

	SLIST_FOREACH(clk, &scmi_clk_list, link) {
		if (clk->channel_id != channel_id)
			continue;
		if (clk->scmi_id > max_id)
			max_id = clk->scmi_id;
		count++;
	}

	if (count == 0)
		return 0;

	/* IDs are starting from 0 so we need to return id + 1 for count */
	return max_id + 1;
}

static struct scmi_clk *clk_scmi_get_by_id(unsigned int channel_id,
					   unsigned int scmi_id)
{
	struct scmi_clk *clk = NULL;

	SLIST_FOREACH(clk, &scmi_clk_list, link) {
		if (clk->channel_id == channel_id &&
		    clk->scmi_id == scmi_id)
			return clk;
	}

	return NULL;
}

const char *plat_scmi_clock_get_name(unsigned int channel_id,
				     unsigned int scmi_id)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return NULL;

	return clk_get_name(clk->clk);
}

int32_t plat_scmi_clock_rates_array(unsigned int channel_id,
				    unsigned int scmi_id,
				    size_t start_index,
				    unsigned long *rates,
				    size_t *nb_elts)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	if (clk_get_rates_array(clk->clk, start_index, rates, nb_elts))
		return SCMI_GENERIC_ERROR;

	return SCMI_SUCCESS;
}

unsigned long plat_scmi_clock_get_rate(unsigned int channel_id,
				       unsigned int scmi_id)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_get_rate(clk->clk);
}

int32_t plat_scmi_clock_set_rate(unsigned int channel_id,
				 unsigned int scmi_id,
				 unsigned long rate)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk_set_rate(clk->clk, rate);
}

int32_t plat_scmi_clock_get_state(unsigned int channel_id,
				  unsigned int scmi_id)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	return clk->enabled;
}

int32_t plat_scmi_clock_set_state(unsigned int channel_id,
				  unsigned int scmi_id,
				  bool enable_not_disable)
{
	struct scmi_clk *clk = NULL;

	clk = clk_scmi_get_by_id(channel_id, scmi_id);
	if (!clk)
		return SCMI_NOT_FOUND;

	if (enable_not_disable) {
		if (!clk->enabled) {
			if (clk_enable(clk->clk))
				return SCMI_GENERIC_ERROR;
			clk->enabled = true;
		}
	} else {
		if (clk->enabled) {
			clk_disable(clk->clk);
			clk->enabled = false;
		}
	}

	return SCMI_SUCCESS;
}

static TEE_Result clk_check_scmi_id(struct clk *new_clk,
				    unsigned int channel_id,
				    unsigned int scmi_id)
{
	struct scmi_clk *clk = NULL;

	SLIST_FOREACH(clk, &scmi_clk_list, link) {
		if (clk->channel_id == channel_id && clk->scmi_id == scmi_id) {
			EMSG("Clock for SCMI channel %d, id %d already registered !",
			     channel_id, scmi_id);
			return TEE_ERROR_BAD_PARAMETERS;
		}
	}

	if (strlen(clk_get_name(new_clk)) >= SCMI_MAX_CLK_NAME_LEN)
		return TEE_ERROR_BAD_PARAMETERS;

	return TEE_SUCCESS;
}

TEE_Result scmi_clk_add(struct clk *clk, unsigned int channel_id,
			unsigned int scmi_id)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct scmi_clk *scmi_clk = NULL;

	res = clk_check_scmi_id(clk, channel_id, scmi_id);
	if (res)
		return res;

	scmi_clk = calloc(1, sizeof(*scmi_clk));
	if (!scmi_clk)
		return TEE_ERROR_OUT_OF_MEMORY;

	scmi_clk->clk = clk;
	scmi_clk->channel_id = channel_id;
	scmi_clk->scmi_id = scmi_id;
	scmi_clk->enabled = 0;

	SLIST_INSERT_HEAD(&scmi_clk_list, scmi_clk, link);

	return TEE_SUCCESS;
}
