/*
 * Copyright (C) 2001-2004 Sistina Software, Inc. All rights reserved.
 * Copyright (C) 2004-2007 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "tools.h"

#define DEFAULT_EXTENT 4096	/* In KB */

static int validate_vg_create_params(struct cmd_context *cmd,
				     const char *vg_name,
				     const uint32_t extent_size,
				     size_t *max_pv,
				     size_t *max_lv,
				     const alloc_policy_t alloc)
{
	if (!validate_new_vg_name(cmd, vg_name)) {
		log_error("New volume group name \"%s\" is invalid", vg_name);
		return 0;
	}

	if (alloc == ALLOC_INHERIT) {
		log_error("Volume Group allocation policy cannot inherit "
			  "from anything");
		return 0;
	}

	if (!extent_size) {
		log_error("Physical extent size may not be zero");
		return 0;
	}

	if (!(cmd->fmt->features & FMT_UNLIMITED_VOLS)) {
		if (!*max_lv)
			*max_lv = 255;
		if (!*max_pv)
			*max_pv = 255;
		if (*max_lv > 255 || *max_pv > 255) {
			log_error("Number of volumes may not exceed 255");
			return 0;
		}
	}

	return 1;
}

int vgcreate(struct cmd_context *cmd, int argc, char **argv)
{
	size_t max_lv, max_pv;
	uint32_t extent_size;
	char *vg_name;
	struct volume_group *vg;
	const char *tag;
	alloc_policy_t alloc;
	int clustered;

	if (!argc) {
		log_error("Please provide volume group name and "
			  "physical volumes");
		return EINVALID_CMD_LINE;
	}

	if (argc == 1) {
		log_error("Please enter physical volume name(s)");
		return EINVALID_CMD_LINE;
	}

	vg_name = skip_dev_dir(cmd, argv[0], NULL);
	max_lv = arg_uint_value(cmd, maxlogicalvolumes_ARG, 0);
	max_pv = arg_uint_value(cmd, maxphysicalvolumes_ARG, 0);
	alloc = arg_uint_value(cmd, alloc_ARG, ALLOC_NORMAL);

	if (arg_sign_value(cmd, physicalextentsize_ARG, 0) == SIGN_MINUS) {
		log_error("Physical extent size may not be negative");
		return EINVALID_CMD_LINE;
	}

	if (arg_sign_value(cmd, maxlogicalvolumes_ARG, 0) == SIGN_MINUS) {
		log_error("Max Logical Volumes may not be negative");
		return EINVALID_CMD_LINE;
	}

	if (arg_sign_value(cmd, maxphysicalvolumes_ARG, 0) == SIGN_MINUS) {
		log_error("Max Physical Volumes may not be negative");
		return EINVALID_CMD_LINE;
	}

	/* Units of 512-byte sectors */
	extent_size =
	    arg_uint_value(cmd, physicalextentsize_ARG, DEFAULT_EXTENT);

	if (!validate_vg_create_params(cmd, vg_name, extent_size,
				       &max_pv, &max_lv, alloc))
	    return EINVALID_CMD_LINE;

	/* Create the new VG */
	if (!(vg = vg_create(cmd, vg_name, extent_size, max_pv, max_lv, alloc,
			     argc - 1, argv + 1)))
		return ECMD_FAILED;

	if (max_lv != vg->max_lv)
		log_warn("WARNING: Setting maxlogicalvolumes to %d "
			 "(0 means unlimited)", vg->max_lv);

	if (max_pv != vg->max_pv)
		log_warn("WARNING: Setting maxphysicalvolumes to %d "
			 "(0 means unlimited)", vg->max_pv);

	if (arg_count(cmd, addtag_ARG)) {
		if (!(tag = arg_str_value(cmd, addtag_ARG, NULL))) {
			log_error("Failed to get tag");
			return ECMD_FAILED;
		}

		if (!(vg->fid->fmt->features & FMT_TAGS)) {
			log_error("Volume group format does not support tags");
			return ECMD_FAILED;
		}

		if (!str_list_add(cmd->mem, &vg->tags, tag)) {
			log_error("Failed to add tag %s to volume group %s",
				  tag, vg_name);
			return ECMD_FAILED;
		}
	}

	if (arg_count(cmd, clustered_ARG))
        	clustered = !strcmp(arg_str_value(cmd, clustered_ARG, "n"), "y");
	else
		/* Default depends on current locking type */
		clustered = locking_is_clustered();

	if (clustered)
		vg->status |= CLUSTERED;
	else
		vg->status &= ~CLUSTERED;

	if (!lock_vol(cmd, VG_ORPHANS, LCK_VG_WRITE)) {
		log_error("Can't get lock for orphan PVs");
		return ECMD_FAILED;
	}

	if (!lock_vol(cmd, vg_name, LCK_VG_WRITE | LCK_NONBLOCK)) {
		log_error("Can't get lock for %s", vg_name);
		unlock_vg(cmd, VG_ORPHANS);
		return ECMD_FAILED;
	}

	if (!archive(vg)) {
		unlock_vg(cmd, vg_name);
		unlock_vg(cmd, VG_ORPHANS);
		return ECMD_FAILED;
	}

	/* Store VG on disk(s) */
	if (!vg_write(vg) || !vg_commit(vg)) {
		unlock_vg(cmd, vg_name);
		unlock_vg(cmd, VG_ORPHANS);
		return ECMD_FAILED;
	}

	unlock_vg(cmd, vg_name);
	unlock_vg(cmd, VG_ORPHANS);

	backup(vg);

	log_print("Volume group \"%s\" successfully created", vg->name);

	return ECMD_PROCESSED;
}
