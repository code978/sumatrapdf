// DON'T EDIT MANUALLY !!!!
// auto-generated by scripts/gen_settings.py !!!!

#include "BaseUtil.h"
#include "Settings.h"

#define MAGIC_ID 0x53756d53  // 'SumS' as 'Sumatra Settings'

#define SERIALIZED_HEADER_LEN 12

typedef struct {
    uint32_t   magicId;
    uint32_t   version;
    uint32_t   topLevelStructOffset;
} SerializedHeader;

STATIC_ASSERT(sizeof(SerializedHeader) == SERIALIZED_HEADER_LEN, SerializedHeader_is_12_bytes);

static const uint16_t TYPE_BOOL         = 0;
static const uint16_t TYPE_I16          = 1;
static const uint16_t TYPE_U16          = 2;
static const uint16_t TYPE_I32          = 3;
static const uint16_t TYPE_U32          = 4;
static const uint16_t TYPE_STR          = 5;
static const uint16_t TYPE_STRUCT_PTR   = 6;

struct FieldMetadata;

typedef struct {
    uint16_t        size;
    uint16_t        nFields;
    FieldMetadata * fields;
} StructMetadata;

// information about a single field
struct FieldMetadata {
    uint16_t type; // TYPE_*
    // from the beginning of the struct
    uint16_t offset;
    // type is TYPE_STRUCT_PTR, otherwise NULL
    StructMetadata *def;
};

FieldMetadata gForwardSearchSettingsFieldMetadata[] = {
    { TYPE_I32, offsetof(ForwardSearchSettings, highlightOffset), NULL },
    { TYPE_I32, offsetof(ForwardSearchSettings, highlightWidth), NULL },
    { TYPE_I32, offsetof(ForwardSearchSettings, highlightPermanent), NULL },
    { TYPE_U32, offsetof(ForwardSearchSettings, highlightColor), NULL },
};

StructMetadata gForwardSearchSettingsMetadata = { sizeof(ForwardSearchSettings), 4, &gForwardSearchSettingsFieldMetadata[0] };

FieldMetadata gPaddingSettingsFieldMetadata[] = {
    { TYPE_U16, offsetof(PaddingSettings, top), NULL },
    { TYPE_U16, offsetof(PaddingSettings, bottom), NULL },
    { TYPE_U16, offsetof(PaddingSettings, left), NULL },
    { TYPE_U16, offsetof(PaddingSettings, right), NULL },
    { TYPE_U16, offsetof(PaddingSettings, spaceX), NULL },
    { TYPE_U16, offsetof(PaddingSettings, spaceY), NULL },
};

StructMetadata gPaddingSettingsMetadata = { sizeof(PaddingSettings), 6, &gPaddingSettingsFieldMetadata[0] };

FieldMetadata gAdvancedSettingsFieldMetadata[] = {
    { TYPE_BOOL, offsetof(AdvancedSettings, traditionalEbookUI), NULL },
    { TYPE_BOOL, offsetof(AdvancedSettings, escToExit), NULL },
    { TYPE_STR, offsetof(AdvancedSettings, emptyString), NULL },
    { TYPE_U32, offsetof(AdvancedSettings, logoColor), NULL },
    { TYPE_STRUCT_PTR, offsetof(AdvancedSettings, pagePadding), &gPaddingSettingsMetadata },
    { TYPE_STRUCT_PTR, offsetof(AdvancedSettings, foo2Padding), &gPaddingSettingsMetadata },
    { TYPE_STR, offsetof(AdvancedSettings, notEmptyString), NULL },
    { TYPE_STRUCT_PTR, offsetof(AdvancedSettings, forwardSearch), &gForwardSearchSettingsMetadata },
};

StructMetadata gAdvancedSettingsMetadata = { sizeof(AdvancedSettings), 8, &gAdvancedSettingsFieldMetadata[0] };

static const uint8_t gAdvancedSettingsDefault[] = {
    0x53, 0x6d, 0x75, 0x53, // magic id 'SumS'
    0x00, 0x00, 0x03, 0x02, // version 2.3
    0x19, 0x00, 0x00, 0x00, // top-level struct offset 0x19

    // offset: 0xc StructVal@0x7fde0dcc ForwardSearchSettings
    0x00, // int32_t highlightOffset = 0x0
    0x0f, // int32_t highlightWidth = 0xf
    0x00, // int32_t highlightPermanent = 0x0
    0xff, 0x83, 0x96, 0x03, // uint32_t highlightColor = 0x6581ff

    // offset: 0x13 StructVal@0x7fde0c8c PaddingSettings
    0x02, // uint16_t top = 0x2
    0x02, // uint16_t bottom = 0x2
    0x04, // uint16_t left = 0x4
    0x04, // uint16_t right = 0x4
    0x04, // uint16_t spaceX = 0x4
    0x04, // uint16_t spaceY = 0x4

    // offset: 0x19 StructVal@0x7fde0e6c AdvancedSettings
    0x00, // bool traditionalEbookUI = 0x0
    0x00, // bool escToExit = 0x0
    0x00, // const char * emptyString = ""
    0x80, 0xe4, 0xff, 0x07, // uint32_t logoColor = 0xfff200
    0x13, // PaddingSettings * pagePadding = StructVal@0x7fde0c8c
    0x00, // PaddingSettings * foo2Padding = NULL
    0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f, // const char * notEmptyString = "Hello"
    0x0c, // ForwardSearchSettings * forwardSearch = StructVal@0x7fde0dcc
};

static uint32_t VersionFromStr(const char *v)
{
    // TODO: write me
    return 0;
}

static uint8_t* DeserializeRec(const uint8_t *data, int dataSize, int dataOff, StructMetadata *def)
{
#if 0
    uint8_t *res = AllocArray<uint8_t>(def->size);
    FieldMetadata *fieldDef = NULL;
    uint64_t decodedVal;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        uint16_t offset = fieldDef->offset;
        uint16_t type = fieldDef->type;
    }
#endif
    return NULL;
}

// a serialized format is a linear chunk of memory with pointers
// replaced with offsets from the beginning of the memory (base)
// to deserialize, we malloc() each struct and replace offsets
// with pointers to those newly allocated structs
// TODO: when version of the data doesn't match our version,
// especially in the case of our version being higher (and hence
// having more data), we should decode the default values and
// then over-write them with whatever values we decoded.
// alternatively, we could keep a default value in struct metadata
static uint8_t* Deserialize(const uint8_t *data, int dataSize, const char *version, StructMetadata *def)
{
    if (!data)
        return NULL;
    if (dataSize < sizeof(SerializedHeader))
        return NULL;
    SerializedHeader *hdr = (SerializedHeader*)data;
    if (hdr->magicId != MAGIC_ID)
        return NULL;
    //uint32_t ver = VersionFromStr(version);
    return DeserializeRec(data, dataSize, SERIALIZED_HEADER_LEN, def);
}

// the assumption here is that the data was either build by deserialize_struct
// or was set by application code in a way that observes our rule: each
// object was separately allocated with malloc()
void FreeStruct(uint8_t *data, StructMetadata *def)
{
    if (!data)
        return;
    FieldMetadata *fieldDef = NULL;
    for (int i = 0; i < def->nFields; i++) {
        fieldDef = def->fields + i;
        if (TYPE_STRUCT_PTR ==  fieldDef->type) {
            uint8_t **p = (uint8_t**)(data + fieldDef->offset);
            FreeStruct(*p, fieldDef->def);
        }
    }
    free(data);
}

// TODO: write me
uint8_t *Serialize(const uint8_t *data, const char *version, StructMetadata *def, int *sizeOut)
{
    *sizeOut = 0;
    return NULL;
}

AdvancedSettings *DeserializeAdvancedSettings(const uint8_t *data, int dataLen, bool *usedDefaultOut)
{
    void *res = NULL;
    res = Deserialize(data, dataLen, AdvancedSettingsVersion, &gAdvancedSettingsMetadata);
    if (res) {
        if (usedDefaultOut)
            *usedDefaultOut = false;
        return (AdvancedSettings*)res;
    }
    res = Deserialize(&gAdvancedSettingsDefault[0], sizeof(gAdvancedSettingsDefault), AdvancedSettingsVersion, &gAdvancedSettingsMetadata);
    CrashAlwaysIf(!res);
    if (usedDefaultOut)
        *usedDefaultOut = true;
    return (AdvancedSettings*)res;
}

uint8_t *SerializeAdvancedSettings(AdvancedSettings *val, int *dataLenOut)
{
    return Serialize((const uint8_t*)val, AdvancedSettingsVersion, &gAdvancedSettingsMetadata, dataLenOut);
}

