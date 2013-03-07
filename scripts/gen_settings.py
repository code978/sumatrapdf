#!/usr/bin/env python

import os
#import struct
import util, gen_settings_2_3

"""
Script that generates C code for compact storage of settings.

In C settigns are represented as a top-level struct (which can then refer to
other structs). The first field of top-level struct must be a u32 version
field. A version support up to 4 components, each component in range <0,255>.
For example version "2.1.3" is encoded as: u32 version = (2 << 16) | (1 << 8) | 3.
This way version numbers can be compared with ">".

We support generating multiple top-level such structs.

In order to support forward and backward compatibility, struct for a given
version must be a strict superset of struct for a previous version. We can
only add, we can't remove fields or change their types (we can change the names
of fields).

By convention, settings for each version of Sumatra are in gen_settings_$ver.py
file. For example, settings for version 2.3 are in gen_settings_2_3.py.

That way we can inherit settings for version N from settings for version N-1.

We rely on an exact layout of data in the struct, so:
 - we don't use C types whose size is not fixed (e.g. int)
 - we use a fixed struct packing
 - our pointers are always 8 bytes, to support both 64-bit and 32-bit archs
   with the same definition

TODO:
 - add a notion of Struct inheritance to make it easy to support forward/backward
   compatibility
 - generate default data as serialized block
 - generate asserts that our assumptions about offsets of fields in the structs
   are correct
"""

h_tmpl = """
// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#ifndef Settings_h
#define Settings_h

template <typename T>
union Ptr {
    T *       ptr;
    char      b[8];
};

STATIC_ASSERT(8 == sizeof(Ptr<int>), ptr_is_8_bytes);

%(h_body)s
#endif
"""

cpp_tmpl = """
// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#include "BaseUtil.h"
#include "Settings.h"

struct StructPointerInfo;

typedef struct {
    int                  pointersCount;
    StructPointerInfo *  pointersInfo;
} StructDef;

// information about a single field
struct StructPointerInfo {
    // from the beginning of the struct
    int offset;
    // what kind of structure it points to, needed
    // for recursive application of the algorithm
    // NULL if that structure doesn't need fixup
    // (has no pointers)
    StructDef *def;
};

%(cpp_body)s

static void unserialize_struct_r(char *data, StructDef *def, char *base)
{
    for (int i=0; i < def->pointersCount; i++) {
        int off = def->pointersInfo[i].offset;
        StructDef *memberDef = def->pointersInfo[i].def;
        char *pval = base + off;
        Ptr<char> *ptrToPtr = (Ptr<char>*)(data + off);
        ptrToPtr->ptr = pval;
        if (memberDef)
            unserialize_struct_r(pval, memberDef, base);
    }
}

// a serialized format is a linear chunk of memory with pointers
// replaced with offsets from the beginning of the memory.
// to unserialize, we need to fix them up i.e. convert offsets
// to pointers
void unserialize_struct(char *data, StructDef *def)
{
    unserialize_struct_r(data, def, data);
}
"""

def gen_cpp_for_struct(stru):
    return ""

def build_struct_def(stru):
    field_off = 0
    for field in stru.fields:
        field.offset = field_off
        field_off += field.c_size()
        stru.append_pack_format(field.pack_format())
        typ = field.c_type()
        if len(typ)  > stru.max_type_len:
            stru.max_type_len = len(typ)
    stru.size = field_off

def c_struct_def(stru):
    lines = ["struct %s {" % stru.name]
    format_str = "    %%-%ds %%s;" % stru.max_type_len
    for field in stru.fields:
        s = format_str % (field.c_type(), field.name)
        lines.append(s)
    lines.append("};\n")
    return "\n".join(lines)

def gen_h_for_struct(stru):
    # first field of the top-level struct must be a version
    assert "version" == stru.fields[0].name and "u32" == stru.fields[0].typ
    structs_remaining = [stru]
    structs_done = []
    while len(structs_remaining) > 0:
        stru = structs_remaining.pop(0)
        structs_done.append(stru)
        build_struct_def(stru)
        for field in stru.fields:
            if field.is_struct():
                structs_remaining.append(field.typ)

    offset = 0
    for stru in reversed(structs_done):
        stru.base_offset = offset
        offset += stru.size

    return "\n".join([c_struct_def(stru) for stru in reversed(structs_done)])

def src_dir():
    d = os.path.dirname(__file__)
    p = os.path.join(d, "..", "src")
    return util.verify_path_exists(p)

def main():
    dst_dir = src_dir()
    advancedSettings = gen_settings_2_3.advancedSettings

    h_body = gen_h_for_struct(advancedSettings)
    h_path = os.path.join(dst_dir, "Settings.h")
    h_txt = h_tmpl % locals()
    file(h_path, "w").write(h_txt)

    cpp_body = gen_cpp_for_struct(advancedSettings)
    cpp_txt = cpp_tmpl % locals()
    cpp_path = os.path.join(dst_dir, "Settings.cpp")
    file(cpp_path, "w").write(cpp_txt)  

if __name__ == "__main__":
    main()
