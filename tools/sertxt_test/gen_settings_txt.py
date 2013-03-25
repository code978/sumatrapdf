#!/usr/bin/env python
import sys
sys.path.append("scripts") # assumes is being run as ./tools/sertxt_test/gen_settings_txt.py

import os, util, codecs
from gen_settings_types import Struct, settings, version, Field

"""
TODO:
 - default values
 - add comment at the top directing to documentation web page
 - allow changing values in struct instances, so that all fields with a given
   struct type don't have to have the same default value
 - remove flatten_struct code
 - escape values with '\n' in them
"""

g_script_dir = os.path.realpath(os.path.dirname(__file__))

g_field_names = util.SeqStrings()

def settings_src_dir():
    return util.verify_path_exists(g_script_dir)

def to_win_newlines(s):
    s = s.replace("\r\n", "\n")
    s = s.replace("\n", "\r\n")
    return s

def write_to_file(file_path, s): file(file_path, "w").write(to_win_newlines(s))
def write_to_file_utf8_bom(file_path, s):
    with codecs.open(file_path, "w", "utf-8-sig") as fo:
        fo.write(to_win_newlines(s))

h_bin_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#ifndef SettingsSumatra_h
#define SettingsSumatra_h

%(struct_defs)s
#endif
"""

cpp_bin_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by gen_settings_txt.py !!!!

#include "BaseUtil.h"
#include "SerializeBin.h"
#include "SettingsSumatra.h"

using namespace serbin;

%(structs_metadata)s
%(top_level_funcs)s
"""

# val is a top-level StructVal with primitive types and
# references to other struct types (forming a tree of values).
# we flatten the values into a list, in the reverse order of
# tree traversal
# TODO: this is really not necessary for txt serialization, only binary
# for txt we only use it to make sure we get type definition in the right
# order, which doesn't work for arrays with no elements
def flatten_struct(stru):
    assert isinstance(stru, Struct)
    vals = []
    left = [stru]
    while len(left) > 0:
        stru = left.pop(0)
        assert isinstance(stru, Struct)
        vals += [stru]
        for field in stru.values:
            if field.is_struct():
                if field.val != None:
                    assert isinstance(field.val, Struct)
                    left += [field.val]
            elif field.is_array():
                for v in field.val.values:
                    left += [v]

    vals.reverse()
    return vals

# TODO: could replace by a filed on Struct
g_struct_defs_generated = []

"""
struct $name {
   $type $field_name;
   ...
};
...
"""
def gen_struct_def(stru):
    global g_struct_defs_generated

    assert isinstance(stru, Struct)
    if stru in g_struct_defs_generated:
        return []
    #assert stru in GetAllStructs()
    name = stru.name()
    lines = ["struct %s {" % name]
    max_len = stru.get_max_field_type_len()
    fmt = "    %%-%ds %%s;" % max_len
    for val in stru.values:
        lines += [fmt % (val.c_type(), val.name)]
    lines += ["};\n"]
    return "\n".join(lines)

prototypes_tmpl = """%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen);
uint8_t *Serialize%(name)s(%(name)s *, int *dataLenOut);
void Free%(name)s(%(name)s *);
"""
def gen_struct_defs(vals, version_str):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.__class__.__name__
    lines = [gen_struct_def(stru) for stru in vals]
    lines += [prototypes_tmpl % locals()]
    return "\n".join(lines)

h_txt_tmpl   ="""// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#ifndef SettingsSumatra_h
#define SettingsSumatra_h

%(struct_defs)s
#endif
"""

cpp_txt_tmpl = """// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#include "BaseUtil.h"
#include "SerializeTxt.h"
#include "SettingsSumatra.h"

using namespace sertxt;

%(filed_names_seq_strings)s
%(structs_metadata)s
%(top_level_funcs)s
"""

"""
FieldMetadata g${name}FieldMetadata[] = {
    { $name_off, $offset, $type, &g${name}StructMetadata },
};
"""
def gen_struct_fields_txt(stru):
    global g_field_names
    assert isinstance(stru, Struct)
    struct_name = stru.name()
    lines = ["FieldMetadata g%(struct_name)sFieldMetadata[] = {" % locals()]
    max_type_len = 0
    max_name_len = 0
    for field in stru.values:
        max_type_len = max(max_type_len, len(field.get_typ_enum()))
    max_type_len += 1

    typ_fmt = "%%-%ds" % max_type_len
    for field in stru.values:
        assert isinstance(field, Field)
        typ_enum = field.get_typ_enum() + ","
        typ_enum = typ_fmt % typ_enum
        field_name = field.name
        settings_name = name2name(field.name)
        name_off = g_field_names.get_offset(settings_name)
        offset = "offsetof(%(struct_name)s, %(field_name)s)" % locals()
        if field.is_struct() or field.is_array():
            field_type = field.typ.name()
            lines += ["    { %(name_off)3d, %(offset)s, %(typ_enum)s &g%(field_type)sMetadata }," % locals()]
        else:
            lines += ["    { %(name_off)3d, %(offset)s, %(typ_enum)s NULL }," % locals()]
    lines += ["};\n"]
    return lines

"""
StructMetadata g${name}StructMetadata = { $size, $nFields, $fields };
"""
def gen_structs_metadata_txt(structs):
    lines = []
    for stru in structs:
        struct_name = stru.name()
        nFields = len(stru.values)
        fields = "&g%sFieldMetadata[0]" % struct_name
        lines += gen_struct_fields_txt(stru)
        lines += ["StructMetadata g%(struct_name)sMetadata = { sizeof(%(struct_name)s), %(nFields)d, %(fields)s };\n" % locals()]
    return "\n".join(lines)

top_level_funcs_txt_tmpl = """
%(name)s *Deserialize%(name)s(const uint8_t *data, int dataLen)
{
    void *res = Deserialize(data, dataLen, &g%(name)sMetadata, FIELD_NAMES_SEQ);
    return (%(name)s*)res;
}

uint8_t *Serialize%(name)s(%(name)s *val, int *dataLenOut)
{
    return Serialize((const uint8_t*)val, &g%(name)sMetadata, FIELD_NAMES_SEQ, dataLenOut);
}

void Free%(name)s(%(name)s *val)
{
    FreeStruct((uint8_t*)val, &g%(name)sMetadata);
}

"""

def gen_top_level_funcs_txt(vals):
    top_level = vals[-1]
    assert isinstance(top_level, Struct)
    name = top_level.name()
    return top_level_funcs_txt_tmpl % locals()

# fooBar => foo_bar
def name2name(s):
    if s == None:
        return None
    res = ""
    n_upper = 0
    for c in s:
        if c.isupper():
            if n_upper == 0:
                res += "_"
            n_upper += 1
            res += c.lower()
        else:
            if n_upper > 1:
                res += "_"
            res += c
            n_upper = 0
    return res


# TODO: support compact serialization of some structs e.g.
"""
  window_pos [
    x: 0
    y: 0
    dx: 0
    dy: 0
  ]
=>
  window_pos: 0 0 0 0
"""
# would need additional info in the data model
def gen_data_txt_rec(root_val, name, lines, indent):
    assert isinstance(root_val, Struct)
    prefix = ""
    if indent >= 0:
        prefix = " " * (indent*2)
    if name != None:
        name = name2name(name)
        lines += ["%s%s [" % (prefix, name)]

    prefix += "  "
    for field in root_val.values:
        var_name = field.name
        var_name = name2name(var_name)
        if field.is_no_store():
            continue
        if field.is_struct():
            if field.val.offset == 0:
                if False:
                    lines += ["%s%s: " % (prefix, var_name)] # TODO: just omit the values?
            else:
                gen_data_txt_rec(field.val, var_name, lines, indent + 1)
        elif field.is_array():
            try:
                n = len(field.val.values)
            except:
                print(field)
                print(field.val)
                raise
            if n > 0:
                lines += ["%s%s [" % (prefix, var_name)]
                prefix += "  "
                for val in field.val.values:
                    lines += [prefix + "["]
                    gen_data_txt_rec(val, None, lines, indent+2)
                    lines += [prefix + "]"]
                prefix = prefix[:-2]
                lines += ["%s]" % prefix]
        else:
            s = field.serialized_as_text()
            # omit serializing empty strings
            if s != None:
                lines += ["%s%s: %s" % (prefix, var_name, s)]
    if name != None:
        lines += ["%s]" % prefix[:-2]]

def gen_txt():
    dst_dir = settings_src_dir()
    val = settings
    vals = flatten_struct(val)

    struct_defs = gen_struct_defs(vals, version)

    structs_metadata = gen_structs_metadata_txt(vals)

    top_level_funcs = gen_top_level_funcs_txt(vals)
    filed_names_seq_strings = "#define FIELD_NAMES_SEQ %s\n" % g_field_names.get_all_c_escaped()

    write_to_file(os.path.join(dst_dir, "SettingsSumatra.h"),  h_txt_tmpl % locals())
    write_to_file(os.path.join(dst_dir, "SettingsSumatra.cpp"), cpp_txt_tmpl % locals())

    lines = []
    gen_data_txt_rec(vals[-1], None, lines, -1)
    s = "\n".join(lines)
    write_to_file_utf8_bom(os.path.join(dst_dir, "data.txt"), s)

def main():
    gen_txt()

if __name__ == "__main__":
    main()
