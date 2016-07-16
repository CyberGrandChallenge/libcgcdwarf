/*-
 * Copyright (c) 2007 John Birrell (jb@freebsd.org)
 * Copyright (c) 2009 Kai Wang
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "_libdwarf.h"

CGCEFTC_VCSID("$Id: dwarf_init.c 2073 2011-10-27 03:30:47Z jkoshy $");

int
dwarf_cgcef_init(CGCEf *cgcef, int mode, Dwarf_Handler errhand, Dwarf_Ptr errarg,
    Dwarf_Debug *ret_dbg, Dwarf_Error *error)
{
	Dwarf_Debug dbg;
	int ret;

	if (cgcef == NULL || ret_dbg == NULL) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (mode != DW_DLC_READ) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (_dwarf_alloc(&dbg, mode, error) != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	if (_dwarf_cgcef_init(dbg, cgcef, error) != DW_DLE_NONE) {
		free(dbg);
		return (DW_DLV_ERROR);
	}

	if ((ret = _dwarf_init(dbg, 0, errhand, errarg, error)) !=
	    DW_DLE_NONE) {
		_dwarf_cgcef_deinit(dbg);
		free(dbg);
		if (ret == DW_DLE_DEBUG_INFO_NULL)
			return (DW_DLV_NO_ENTRY);
		else
			return (DW_DLV_ERROR);
	}

	*ret_dbg = dbg;

	return (DW_DLV_OK);
}

int
dwarf_get_cgcef(Dwarf_Debug dbg, CGCEf **cgcef, Dwarf_Error *error)
{
	Dwarf_CGCEf_Object *e;

	if (dbg == NULL || cgcef == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	e = dbg->dbg_iface->object;
	*cgcef = e->eo_cgcef;

	return (DW_DLV_OK);
}

int
dwarf_init(int fd, int mode, Dwarf_Handler errhand, Dwarf_Ptr errarg,
    Dwarf_Debug *ret_dbg, Dwarf_Error *error)
{
	Dwarf_Debug dbg;
	CGCEf *cgcef;
	int ret;

	if (fd < 0 || ret_dbg == NULL) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (mode != DW_DLC_READ) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (cgcef_version(EV_CURRENT) == EV_NONE) {
		DWARF_SET_CGCEF_ERROR(NULL, error);
		return (DW_DLV_ERROR);
	}

	if ((cgcef = cgcef_begin(fd, CGCEF_C_READ, NULL)) == NULL) {
		DWARF_SET_CGCEF_ERROR(NULL, error);
		return (DW_DLV_ERROR);
	}

	if (_dwarf_alloc(&dbg, mode, error) != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	if (_dwarf_cgcef_init(dbg, cgcef, error) != DW_DLE_NONE) {
		free(dbg);
		return (DW_DLV_ERROR);
	}

	if ((ret = _dwarf_init(dbg, 0, errhand, errarg, error)) !=
	    DW_DLE_NONE) {
		_dwarf_cgcef_deinit(dbg);
		free(dbg);
		if (ret == DW_DLE_DEBUG_INFO_NULL)
			return (DW_DLV_NO_ENTRY);
		else
			return (DW_DLV_ERROR);
	}

	*ret_dbg = dbg;

	return (DW_DLV_OK);
}

int
dwarf_object_init(Dwarf_Obj_Access_Interface *iface, Dwarf_Handler errhand,
    Dwarf_Ptr errarg, Dwarf_Debug *ret_dbg, Dwarf_Error *error)
{
	Dwarf_Debug dbg;

	if (iface == NULL || ret_dbg == NULL) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_ARGUMENT);
		return (DW_DLV_ERROR);
	}

	if (_dwarf_alloc(&dbg, DW_DLC_READ, error) != DW_DLE_NONE)
		return (DW_DLV_ERROR);

	dbg->dbg_iface = iface;

	if (_dwarf_init(dbg, 0, errhand, errarg, error) != DW_DLE_NONE) {
		free(dbg);
		return (DW_DLV_ERROR);
	}

	*ret_dbg = dbg;

	return (DW_DLV_OK);
}
