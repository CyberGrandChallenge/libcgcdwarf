/*-
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

CGCEFTC_VCSID("$Id: libdwarf_cgcef_init.c 2070 2011-10-27 03:05:32Z jkoshy $");

static const char *debug_name[] = {
	".debug_abbrev",
	".debug_aranges",
	".debug_frame",
	".debug_info",
	".debug_line",
	".debug_pubnames",
	".eh_frame",
	".debug_macinfo",
	".debug_str",
	".debug_loc",
	".debug_pubtypes",
	".debug_ranges",
	".debug_static_func",
	".debug_static_vars",
	".debug_types",
	".debug_weaknames",
	NULL
};

static void
_dwarf_cgcef_apply_reloc(Dwarf_Debug dbg, void *buf, CGCEf_Data *rel_data,
    CGCEf_Data *symtab_data, int endian)
{
	Dwarf_Unsigned type;
	GCGCEf_Rela rela;
	GCGCEf_Sym sym;
	size_t symndx;
	uint64_t offset;
	int size, j;

	j = 0;
	while (gcgcef_getrela(rel_data, j++, &rela) != NULL) {
		symndx = GCGCEF_R_SYM(rela.r_info);
		type = GCGCEF_R_TYPE(rela.r_info);

		if (gcgcef_getsym(symtab_data, symndx, &sym) == NULL)
			continue;

		offset = rela.r_offset;
		size = _dwarf_get_reloc_size(dbg, type);

		if (endian == CGCEFDATA2MSB)
			_dwarf_write_msb(buf, &offset, rela.r_addend, size);
		else
			_dwarf_write_lsb(buf, &offset, rela.r_addend, size);
	}
}

static int
_dwarf_cgcef_relocate(Dwarf_Debug dbg, CGCEf *cgcef, Dwarf_CGCEf_Data *ed, size_t shndx,
    size_t symtab, CGCEf_Data *symtab_data, Dwarf_Error *error)
{
	GCGCEf_Ehdr eh;
	GCGCEf_Shdr sh;
	CGCEf_Scn *scn;
	CGCEf_Data *rel;
	int cgceferr;

	if (symtab == 0 || symtab_data == NULL)
		return (DW_DLE_NONE);

	if (gcgcef_getehdr(cgcef, &eh) == NULL) {
		DWARF_SET_CGCEF_ERROR(dbg, error);
		return (DW_DLE_CGCEF);
	}

	scn = NULL;
	(void) cgcef_errno();
	while ((scn = cgcef_nextscn(cgcef, scn)) != NULL) {
		if (gcgcef_getshdr(scn, &sh) == NULL) {
			DWARF_SET_CGCEF_ERROR(dbg, error);
			return (DW_DLE_CGCEF);
		}

		if (sh.sh_type != SHT_RELA || sh.sh_size == 0)
			continue;

		if (sh.sh_info == shndx && sh.sh_link == symtab) {
			if ((rel = cgcef_getdata(scn, NULL)) == NULL) {
				cgceferr = cgcef_errno();
				if (cgceferr != 0) {
					_DWARF_SET_ERROR(NULL, error,
					    DW_DLE_CGCEF, cgceferr);
					return (DW_DLE_CGCEF);
				} else
					return (DW_DLE_NONE);
			}

			ed->ed_alloc = malloc(ed->ed_data->d_size);
			if (ed->ed_alloc == NULL) {
				DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
				return (DW_DLE_MEMORY);
			}
			memcpy(ed->ed_alloc, ed->ed_data->d_buf,
			    ed->ed_data->d_size);
			_dwarf_cgcef_apply_reloc(dbg, ed->ed_alloc, rel,
			    symtab_data, eh.e_ident[EI_DATA]);

			return (DW_DLE_NONE);
		}
	}
	cgceferr = cgcef_errno();
	if (cgceferr != 0) {
		DWARF_SET_CGCEF_ERROR(dbg, error);
		return (DW_DLE_CGCEF);
	}

	return (DW_DLE_NONE);
}

int
_dwarf_cgcef_init(Dwarf_Debug dbg, CGCEf *cgcef, Dwarf_Error *error)
{
	Dwarf_Obj_Access_Interface *iface;
	Dwarf_CGCEf_Object *e;
	const char *name;
	GCGCEf_Shdr sh;
	CGCEf_Scn *scn;
	CGCEf_Data *symtab_data;
	size_t symtab_ndx;
	int cgceferr, i, j, n, ret;

	ret = DW_DLE_NONE;

	if ((iface = calloc(1, sizeof(*iface))) == NULL) {
		DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
		return (DW_DLE_MEMORY);
	}

	if ((e = calloc(1, sizeof(*e))) == NULL) {
		free(iface);
		DWARF_SET_ERROR(dbg, error, DW_DLE_MEMORY);
		return (DW_DLE_MEMORY);
	}

	e->eo_cgcef = cgcef;
	e->eo_methods.get_section_info = _dwarf_cgcef_get_section_info;
	e->eo_methods.get_byte_order = _dwarf_cgcef_get_byte_order;
	e->eo_methods.get_length_size = _dwarf_cgcef_get_length_size;
	e->eo_methods.get_pointer_size = _dwarf_cgcef_get_pointer_size;
	e->eo_methods.get_section_count = _dwarf_cgcef_get_section_count;
	e->eo_methods.load_section = _dwarf_cgcef_load_section;

	iface->object = e;
	iface->methods = &e->eo_methods;

	dbg->dbg_iface = iface;

	if (gcgcef_getehdr(cgcef, &e->eo_ehdr) == NULL) {
		DWARF_SET_CGCEF_ERROR(dbg, error);
		ret = DW_DLE_CGCEF;
		goto fail_cleanup;
	}

	dbg->dbg_machine = e->eo_ehdr.e_machine;

	if (!(cgcef_getshdrstrndx(cgcef, &e->eo_strndx) >= 0)) {
		DWARF_SET_CGCEF_ERROR(dbg, error);
		ret = DW_DLE_CGCEF;
		goto fail_cleanup;
	}

	n = 0;
	symtab_ndx = 0;
	symtab_data = NULL;
	scn = NULL;
	(void) cgcef_errno();
	while ((scn = cgcef_nextscn(cgcef, scn)) != NULL) {
		if (gcgcef_getshdr(scn, &sh) == NULL) {
			DWARF_SET_CGCEF_ERROR(dbg, error);
			ret = DW_DLE_CGCEF;
			goto fail_cleanup;
		}

		if ((name = cgcef_strptr(cgcef, e->eo_strndx, sh.sh_name)) ==
		    NULL) {
			DWARF_SET_CGCEF_ERROR(dbg, error);
			ret = DW_DLE_CGCEF;
			goto fail_cleanup;
		}

		if (!strcmp(name, ".symtab")) {
			symtab_ndx = cgcef_ndxscn(scn);
			if ((symtab_data = cgcef_getdata(scn, NULL)) == NULL) {
				cgceferr = cgcef_errno();
				if (cgceferr != 0) {
					_DWARF_SET_ERROR(NULL, error,
					    DW_DLE_CGCEF, cgceferr);
					ret = DW_DLE_CGCEF;
					goto fail_cleanup;
				}
			}
			continue;
		}

		for (i = 0; debug_name[i] != NULL; i++) {
			if (!strcmp(name, debug_name[i]))
				n++;
		}
	}
	cgceferr = cgcef_errno();
	if (cgceferr != 0) {
		DWARF_SET_CGCEF_ERROR(dbg, error);
		return (DW_DLE_CGCEF);
	}

	e->eo_seccnt = n;

	if ((e->eo_data = calloc(n, sizeof(Dwarf_CGCEf_Data))) == NULL ||
	    (e->eo_shdr = calloc(n, sizeof(GCGCEf_Shdr))) == NULL) {
		DWARF_SET_ERROR(NULL, error, DW_DLE_MEMORY);
		ret = DW_DLE_MEMORY;
		goto fail_cleanup;
	}

	scn = NULL;
	j = 0;
	while ((scn = cgcef_nextscn(cgcef, scn)) != NULL && j < n) {
		if (gcgcef_getshdr(scn, &sh) == NULL) {
			DWARF_SET_CGCEF_ERROR(dbg, error);
			ret = DW_DLE_CGCEF;
			goto fail_cleanup;
		}

		memcpy(&e->eo_shdr[j], &sh, sizeof(sh));

		if ((name = cgcef_strptr(cgcef, e->eo_strndx, sh.sh_name)) ==
		    NULL) {
			DWARF_SET_CGCEF_ERROR(dbg, error);
			ret = DW_DLE_CGCEF;
			goto fail_cleanup;
		}

		for (i = 0; debug_name[i] != NULL; i++) {
			if (strcmp(name, debug_name[i]))
				continue;

			(void) cgcef_errno();
			if ((e->eo_data[j].ed_data = cgcef_getdata(scn, NULL)) ==
			    NULL) {
				cgceferr = cgcef_errno();
				if (cgceferr != 0) {
					_DWARF_SET_ERROR(dbg, error,
					    DW_DLE_CGCEF, cgceferr);
					ret = DW_DLE_CGCEF;
					goto fail_cleanup;
				}
			}

			if (_libdwarf.applyrela) {
				if (_dwarf_cgcef_relocate(dbg, cgcef,
				    &e->eo_data[j], cgcef_ndxscn(scn), symtab_ndx,
				    symtab_data, error) != DW_DLE_NONE)
					goto fail_cleanup;
			}

			j++;
		}
	}

	assert(j == n);

	return (DW_DLE_NONE);

fail_cleanup:

	_dwarf_cgcef_deinit(dbg);

	return (ret);
}

void
_dwarf_cgcef_deinit(Dwarf_Debug dbg)
{
	Dwarf_Obj_Access_Interface *iface;
	Dwarf_CGCEf_Object *e;
	int i;

	iface = dbg->dbg_iface;
	assert(iface != NULL);

	e = iface->object;
	assert(e != NULL);

	if (e->eo_data) {
		for (i = 0; (Dwarf_Unsigned) i < e->eo_seccnt; i++) {
			if (e->eo_data[i].ed_alloc)
				free(e->eo_data[i].ed_alloc);
		}
		free(e->eo_data);
	}
	if (e->eo_shdr)
		free(e->eo_shdr);

	free(e);
	free(iface);

	dbg->dbg_iface = NULL;
}
