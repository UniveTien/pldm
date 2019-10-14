#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bios.h"
#include "bios_table.h"

#define POINTER_CHECK(pointer)                                                 \
	do {                                                                   \
		if (pointer == NULL)                                           \
			return PLDM_ERROR_INVALID_DATA;                        \
	} while (0)

#define ATTR_TYPE_EXPECT(type, expected)                                       \
	do {                                                                   \
		if (type != expected && type != (expected | 0x80))             \
			return PLDM_ERROR_INVALID_DATA;                        \
	} while (0)

#define BUFFER_SIZE_EXPECT(current_size, expected_size)                        \
	do {                                                                   \
		if (current_size < expected_size)                              \
			return PLDM_ERROR_INVALID_LENGTH;                      \
	} while (0)

#define MEMBER_SIZE(type, member) sizeof(((struct type *)0)->member)

static uint16_t get_bios_string_handle()
{
	static uint16_t handle = 0;
	assert(handle != UINT16_MAX);

	return handle++;
}

size_t pldm_bios_table_string_entry_encode_length(uint16_t string_length)
{
	return sizeof(struct pldm_bios_string_table_entry) -
	       MEMBER_SIZE(pldm_bios_string_table_entry, name) + string_length;
}

void pldm_bios_table_string_entry_encode(void *entry, size_t entry_length,
					 const char *str, uint16_t str_length)
{
	size_t length = pldm_bios_table_string_entry_encode_length(str_length);
	assert(length <= entry_length);
	struct pldm_bios_string_table_entry *string_entry = entry;
	string_entry->string_handle = htole16(get_bios_string_handle());
	string_entry->string_length = htole16(str_length);
	memcpy(string_entry->name, str, str_length);
}

int pldm_bios_table_string_entry_encode_check(void *entry, size_t entry_length,
					      const char *str,
					      uint16_t str_length)
{
	if (str_length == 0)
		return PLDM_ERROR_INVALID_DATA;
	POINTER_CHECK(entry);
	POINTER_CHECK(str);
	size_t length = pldm_bios_table_string_entry_encode_length(str_length);
	BUFFER_SIZE_EXPECT(entry_length, length);
	pldm_bios_table_string_entry_encode(entry, entry_length, str,
					    str_length);
	return PLDM_SUCCESS;
}

uint16_t pldm_bios_table_string_entry_decode_handle(
    const struct pldm_bios_string_table_entry *entry)
{
	return le16toh(entry->string_handle);
}

uint16_t pldm_bios_table_string_entry_decode_string_length(
    const struct pldm_bios_string_table_entry *entry)
{
	return le16toh(entry->string_length);
}

uint16_t pldm_bios_table_string_entry_decode_string(
    const struct pldm_bios_string_table_entry *entry, char *buffer, size_t size)
{
	uint16_t length =
	    pldm_bios_table_string_entry_decode_string_length(entry);
	length = length < size ? length : size;
	memcpy(buffer, entry->name, length);
	buffer[length] = 0;
	return length;
}

int pldm_bios_table_string_entry_decode_string_check(
    const struct pldm_bios_string_table_entry *entry, char *buffer, size_t size)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(buffer);
	size_t length =
	    pldm_bios_table_string_entry_decode_string_length(entry);
	BUFFER_SIZE_EXPECT(size, length + 1);
	pldm_bios_table_string_entry_decode_string(entry, buffer, size);
	return PLDM_SUCCESS;
}

static size_t string_table_entry_length(const void *table_entry)
{
	const struct pldm_bios_string_table_entry *entry = table_entry;
	return sizeof(*entry) - sizeof(entry->name) +
	       pldm_bios_table_string_entry_decode_string_length(entry);
}

static uint16_t get_bios_attr_handle()
{
	static uint16_t handle = 0;
	assert(handle != UINT16_MAX);

	return handle++;
}

static void attr_table_entry_encode_header(void *entry, size_t length,
					   uint8_t attr_type,
					   uint16_t string_handle)
{
	struct pldm_bios_attr_table_entry *attr_entry = entry;
	assert(sizeof(*attr_entry) <= length);
	attr_entry->attr_handle = htole16(get_bios_attr_handle());
	attr_entry->attr_type = attr_type;
	attr_entry->string_handle = htole16(string_handle);
}

size_t pldm_bios_table_attr_entry_enum_encode_length(uint8_t pv_num,
						     uint8_t def_num)
{
	return sizeof(struct pldm_bios_attr_table_entry) -
	       MEMBER_SIZE(pldm_bios_attr_table_entry, metadata) +
	       sizeof(pv_num) + pv_num * sizeof(uint16_t) + sizeof(def_num) +
	       def_num;
}

void pldm_bios_table_attr_entry_enum_encode(
    void *entry, size_t entry_length,
    const struct pldm_bios_table_attr_entry_enum_info *info)
{
	size_t length = pldm_bios_table_attr_entry_enum_encode_length(
	    info->pv_num, info->def_num);
	assert(length <= entry_length);
	uint8_t attr_type = info->read_only ? PLDM_BIOS_ENUMERATION_READ_ONLY
					    : PLDM_BIOS_ENUMERATION;
	attr_table_entry_encode_header(entry, entry_length, attr_type,
				       info->name_handle);
	struct pldm_bios_attr_table_entry *attr_entry = entry;
	attr_entry->metadata[0] = info->pv_num;
	uint16_t *pv_hdls =
	    (uint16_t *)(attr_entry->metadata + 1 /* sizeof(pv num) */);
	size_t i;
	for (i = 0; i < info->pv_num; i++)
		pv_hdls[i] = htole16(info->pv_handle[i]);
	attr_entry->metadata[1 + info->pv_num * sizeof(uint16_t)] =
	    info->def_num;
	memcpy(attr_entry->metadata + 1 /* sizeof(pv num) */ +
		   info->pv_num * sizeof(uint16_t) + 1 /* sizeof(def num)*/,
	       info->def_index, info->def_num);
}

int pldm_bios_table_attr_entry_enum_encode_check(
    void *entry, size_t entry_length,
    const struct pldm_bios_table_attr_entry_enum_info *info)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(info);
	size_t length = pldm_bios_table_attr_entry_enum_encode_length(
	    info->pv_num, info->def_num);
	BUFFER_SIZE_EXPECT(entry_length, length);
	pldm_bios_table_attr_entry_enum_encode(entry, entry_length, info);
	return PLDM_SUCCESS;
}

#define ATTR_TYPE_EXPECT(type, expected)                                       \
	do {                                                                   \
		if (type != expected && type != (expected | 0x80))             \
			return PLDM_ERROR_INVALID_DATA;                        \
	} while (0)

uint8_t pldm_bios_table_attr_entry_enum_decode_pv_num(
    const struct pldm_bios_attr_table_entry *entry)
{
	return entry->metadata[0];
}

int pldm_bios_table_attr_entry_enum_decode_pv_num_check(
    const struct pldm_bios_attr_table_entry *entry, uint8_t *pv_num)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(pv_num);
	ATTR_TYPE_EXPECT(entry->attr_type, PLDM_BIOS_ENUMERATION);
	*pv_num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	return PLDM_SUCCESS;
}

uint8_t pldm_bios_table_attr_entry_enum_decode_def_num(
    const struct pldm_bios_attr_table_entry *entry)
{
	uint8_t pv_num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	return entry->metadata[sizeof(uint8_t) /* pv_num */ +
			       sizeof(uint16_t) * pv_num];
}

int pldm_bios_table_attr_entry_enum_decode_def_num_check(
    const struct pldm_bios_attr_table_entry *entry, uint8_t *def_num)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(def_num);
	ATTR_TYPE_EXPECT(entry->attr_type, PLDM_BIOS_ENUMERATION);
	*def_num = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
	return PLDM_SUCCESS;
}

uint8_t pldm_bios_table_attr_entry_enum_decode_pv_hdls(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num)
{
	uint8_t num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	num = num < pv_num ? num : pv_num;
	size_t i;
	for (i = 0; i < num; i++) {
		uint16_t *hdl = (uint16_t *)(entry->metadata + sizeof(uint8_t) +
					     i * sizeof(uint16_t));
		pv_hdls[i] = le16toh(*hdl);
	}
	return num;
}

int pldm_bios_table_attr_entry_enum_decode_pv_hdls_check(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *pv_hdls,
    uint8_t pv_num)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(pv_hdls);
	ATTR_TYPE_EXPECT(entry->attr_type, PLDM_BIOS_ENUMERATION);
	uint8_t num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	if (num != pv_num)
		return PLDM_ERROR_INVALID_DATA;
	pldm_bios_table_attr_entry_enum_decode_pv_hdls(entry, pv_hdls, pv_num);
	return PLDM_SUCCESS;
}

/** @brief Get length of an enum attribute entry
 */
static size_t
attr_table_entry_length_enum(const struct pldm_bios_attr_table_entry *entry)
{
	uint8_t pv_num = pldm_bios_table_attr_entry_enum_decode_pv_num(entry);
	uint8_t def_num = pldm_bios_table_attr_entry_enum_decode_def_num(entry);
	return pldm_bios_table_attr_entry_enum_encode_length(pv_num, def_num);
}

struct attr_table_string_entry_fields {
	uint8_t string_type;
	uint16_t min_length;
	uint16_t max_length;
	uint16_t def_length;
	uint8_t def_string[1];
} __attribute__((packed));

size_t pldm_bios_table_attr_entry_string_encode_length(uint16_t def_str_len)
{
	return sizeof(struct pldm_bios_attr_table_entry) -
	       MEMBER_SIZE(pldm_bios_attr_table_entry, metadata) +
	       sizeof(struct attr_table_string_entry_fields) -
	       MEMBER_SIZE(attr_table_string_entry_fields, def_string) +
	       def_str_len;
}

void pldm_bios_table_attr_entry_string_encode(
    void *entry, size_t entry_length,
    const struct pldm_bios_table_attr_entry_string_info *info)
{
	size_t length =
	    pldm_bios_table_attr_entry_string_encode_length(info->def_length);
	assert(length <= entry_length);
	uint8_t attr_type =
	    info->read_only ? PLDM_BIOS_STRING_READ_ONLY : PLDM_BIOS_STRING;
	attr_table_entry_encode_header(entry, entry_length, attr_type,
				       info->name_handle);
	struct pldm_bios_attr_table_entry *attr_entry = entry;
	struct attr_table_string_entry_fields *attr_fields =
	    (struct attr_table_string_entry_fields *)attr_entry->metadata;
	attr_fields->string_type = info->string_type;
	attr_fields->min_length = htole16(info->min_length);
	attr_fields->max_length = htole16(info->max_length);
	attr_fields->def_length = htole16(info->def_length);
	if (info->def_length != 0 && info->def_string != NULL)
		memcpy(attr_fields->def_string, info->def_string,
		       info->def_length);
}

int pldm_bios_table_attr_entry_string_encode_check(
    void *entry, size_t entry_length,
    const struct pldm_bios_table_attr_entry_string_info *info)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(info);
	size_t length =
	    pldm_bios_table_attr_entry_string_encode_length(info->def_length);
	BUFFER_SIZE_EXPECT(entry_length, length);
	if (info->def_length > info->max_length ||
	    info->def_length < info->min_length ||
	    info->min_length > info->max_length)
		return PLDM_ERROR_INVALID_DATA;
	if (info->string_type > 5 && info->string_type != 0xFF)
		return PLDM_ERROR_INVALID_DATA;
	pldm_bios_table_attr_entry_string_encode(entry, entry_length, info);
	return PLDM_SUCCESS;
}

uint16_t pldm_bios_table_attr_entry_string_decode_def_string_length(
    const struct pldm_bios_attr_table_entry *entry)
{
	struct attr_table_string_entry_fields *fields =
	    (struct attr_table_string_entry_fields *)entry->metadata;
	return le16toh(fields->def_length);
}

int pldm_bios_table_attr_entry_string_decode_def_string_length_check(
    const struct pldm_bios_attr_table_entry *entry, uint16_t *def_string_length)
{
	POINTER_CHECK(entry);
	POINTER_CHECK(def_string_length);
	ATTR_TYPE_EXPECT(entry->attr_type, PLDM_BIOS_STRING);
	*def_string_length =
	    pldm_bios_table_attr_entry_string_decode_def_string_length(entry);
	return PLDM_SUCCESS;
}

/** @brief Get length of a string attribute entry
 */
static size_t
attr_table_entry_length_string(const struct pldm_bios_attr_table_entry *entry)
{
	uint16_t def_str_len =
	    pldm_bios_table_attr_entry_string_decode_def_string_length(entry);
	return pldm_bios_table_attr_entry_string_encode_length(def_str_len);
}

struct attr_table_entry {
	uint8_t attr_type;
	size_t (*entry_length_handler)(
	    const struct pldm_bios_attr_table_entry *);
};

static struct attr_table_entry attr_table_entrys[] = {
    {.attr_type = PLDM_BIOS_ENUMERATION,
     .entry_length_handler = attr_table_entry_length_enum},
    {.attr_type = PLDM_BIOS_ENUMERATION_READ_ONLY,
     .entry_length_handler = attr_table_entry_length_enum},
    {.attr_type = PLDM_BIOS_STRING,
     .entry_length_handler = attr_table_entry_length_string},
    {.attr_type = PLDM_BIOS_STRING_READ_ONLY,
     .entry_length_handler = attr_table_entry_length_string},
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static struct attr_table_entry *find_attr_table_entry_by_type(uint8_t attr_type)
{
	size_t i;
	for (i = 0; i < ARRAY_SIZE(attr_table_entrys); i++) {
		if (attr_type == attr_table_entrys[i].attr_type)
			return &attr_table_entrys[i];
	}
	return NULL;
}

static size_t attr_table_entry_length(const void *table_entry)
{
	const struct pldm_bios_attr_table_entry *entry = table_entry;
	struct attr_table_entry *attr_table_entry =
	    find_attr_table_entry_by_type(entry->attr_type);
	assert(attr_table_entry != NULL);
	assert(attr_table_entry->entry_length_handler != NULL);

	return attr_table_entry->entry_length_handler(entry);
}

size_t pldm_bios_table_attr_value_entry_encode_enum_length(uint8_t count)
{
	return sizeof(struct pldm_bios_attr_val_table_entry) - 1 +
	       sizeof(count) + count;
}

void pldm_bios_table_attr_value_entry_encode_enum(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handles)
{
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_enum_length(count);
	assert(length <= entry_length);

	struct pldm_bios_attr_val_table_entry *table_entry = entry;
	table_entry->attr_handle = htole16(attr_handle);
	table_entry->attr_type = attr_type;
	table_entry->value[0] = count;
	if (count != 0)
		memcpy(&table_entry->value[1], handles, count);
}

int pldm_bios_table_attr_value_entry_encode_enum_check(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint8_t count, uint8_t *handles)
{
	POINTER_CHECK(entry);
	if (count != 0 && handles == NULL)
		return PLDM_ERROR_INVALID_DATA;
	ATTR_TYPE_EXPECT(attr_type, PLDM_BIOS_ENUMERATION);
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_enum_length(count);
	BUFFER_SIZE_EXPECT(entry_length, length);
	pldm_bios_table_attr_value_entry_encode_enum(
	    entry, entry_length, attr_handle, attr_type, count, handles);
	return PLDM_SUCCESS;
}

size_t
pldm_bios_table_attr_value_entry_encode_string_length(uint16_t string_length)
{
	return sizeof(struct pldm_bios_attr_val_table_entry) - 1 +
	       sizeof(string_length) + string_length;
}

void pldm_bios_table_attr_value_entry_encode_string(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t str_length, const char *str)
{
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_string_length(str_length);
	assert(length <= entry_length);

	struct pldm_bios_attr_val_table_entry *table_entry = entry;
	table_entry->attr_handle = htole16(attr_handle);
	table_entry->attr_type = attr_type;
	if (str_length != 0)
		memcpy(table_entry->value + sizeof(str_length), str,
		       str_length);
	str_length = htole16(str_length);
	memcpy(table_entry->value, &str_length, sizeof(str_length));
}

int pldm_bios_table_attr_value_entry_encode_string_check(
    void *entry, size_t entry_length, uint16_t attr_handle, uint8_t attr_type,
    uint16_t str_length, const char *str)
{
	POINTER_CHECK(entry);
	if (str_length != 0 && str == NULL)
		return PLDM_ERROR_INVALID_DATA;
	ATTR_TYPE_EXPECT(attr_type, PLDM_BIOS_STRING);
	size_t length =
	    pldm_bios_table_attr_value_entry_encode_string_length(str_length);
	BUFFER_SIZE_EXPECT(entry_length, length);
	pldm_bios_table_attr_value_entry_encode_string(
	    entry, entry_length, attr_handle, attr_type, str_length, str);
	return PLDM_SUCCESS;
}

struct pldm_bios_table_iter {
	const uint8_t *table_data;
	size_t table_len;
	size_t current_pos;
	size_t (*entry_length_handler)(const void *table_entry);
};

struct pldm_bios_table_iter *
pldm_bios_table_iter_create(const void *table, size_t length,
			    enum pldm_bios_table_types type)
{
	struct pldm_bios_table_iter *iter = malloc(sizeof(*iter));
	assert(iter != NULL);
	iter->table_data = table;
	iter->table_len = length;
	iter->current_pos = 0;
	iter->entry_length_handler = NULL;
	switch (type) {
	case PLDM_BIOS_STRING_TABLE:
		iter->entry_length_handler = string_table_entry_length;
		break;
	case PLDM_BIOS_ATTR_TABLE:
		iter->entry_length_handler = attr_table_entry_length;
		break;
	case PLDM_BIOS_ATTR_VAL_TABLE:
		break;
	}

	return iter;
}

void pldm_bios_table_iter_free(struct pldm_bios_table_iter *iter)
{
	free(iter);
}

#define pad_and_check_max 7
bool pldm_bios_table_iter_is_end(const struct pldm_bios_table_iter *iter)
{
	if (iter->table_len - iter->current_pos <= pad_and_check_max)
		return true;
	return false;
}

void pldm_bios_table_iter_next(struct pldm_bios_table_iter *iter)
{
	if (pldm_bios_table_iter_is_end(iter))
		return;
	const void *entry = iter->table_data + iter->current_pos;
	iter->current_pos += iter->entry_length_handler(entry);
}

const void *pldm_bios_table_iter_value(struct pldm_bios_table_iter *iter)
{
	return iter->table_data + iter->current_pos;
}

static const void *
pldm_bios_table_entry_find(struct pldm_bios_table_iter *iter, const void *key,
			   int (*equal)(const void *entry, const void *key))
{
	const void *entry;
	while (!pldm_bios_table_iter_is_end(iter)) {
		entry = pldm_bios_table_iter_value(iter);
		if (equal(entry, key))
			return entry;
		pldm_bios_table_iter_next(iter);
	}
	return NULL;
}

static int string_table_handle_equal(const void *entry, const void *key)
{
	const struct pldm_bios_string_table_entry *string_entry = entry;
	uint16_t handle = *(uint16_t *)key;
	if (pldm_bios_table_string_entry_decode_handle(string_entry) == handle)
		return true;
	return false;
}

struct string_equal_arg {
	uint16_t str_length;
	const char *str;
};

static int string_table_string_equal(const void *entry, const void *key)
{
	const struct pldm_bios_string_table_entry *string_entry = entry;
	const struct string_equal_arg *arg = key;
	if (arg->str_length !=
	    pldm_bios_table_string_entry_decode_string_length(string_entry))
		return false;
	if (memcmp(string_entry->name, arg->str, arg->str_length) != 0)
		return false;
	return true;
}

const struct pldm_bios_string_table_entry *
pldm_bios_table_string_find_by_string(const void *table, size_t length,
				      const char *str)
{
	uint16_t str_length = strlen(str);
	struct string_equal_arg arg = {str_length, str};
	struct pldm_bios_table_iter *iter =
	    pldm_bios_table_iter_create(table, length, PLDM_BIOS_STRING_TABLE);
	const void *entry =
	    pldm_bios_table_entry_find(iter, &arg, string_table_string_equal);
	pldm_bios_table_iter_free(iter);
	return entry;
}

const struct pldm_bios_string_table_entry *
pldm_bios_table_string_find_by_handle(const void *table, size_t length,
				      uint16_t handle)
{
	struct pldm_bios_table_iter *iter =
	    pldm_bios_table_iter_create(table, length, PLDM_BIOS_STRING_TABLE);
	const void *entry = pldm_bios_table_entry_find(
	    iter, &handle, string_table_handle_equal);
	pldm_bios_table_iter_free(iter);
	return entry;
}