/*
  +----------------------------------------------------------------------+
  | See COPYING file for further copyright information                   |
  +----------------------------------------------------------------------+
  | Author: Oleg Grenrus <oleg.grenrus@dynamoid.com>                     |
  | See CREDITS for contributors                                         |
  +----------------------------------------------------------------------+
*/

/************************************************************************
 * V3 serialization and unserialization code                            *
 ***********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef PHP_WIN32
# include "ig_win32.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_alloc.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "ext/standard/info.h"
#include "ext/standard/php_var.h"

#if PHP_VERSION_ID >= 80100
#include "Zend/zend_enum.h"
#endif

#if HAVE_PHP_SESSION && !defined(COMPILE_DL_SESSION)
# include "ext/session/php_session.h"
#endif

#include "ext/standard/php_incomplete_class.h"

#if PHP_VERSION_ID < 70400
#define zend_get_properties_for(struc, purpose) Z_OBJPROP_P((struc))

#define zend_release_properties(ht) do {} while (0)
#endif

#if PHP_VERSION_ID < 70300
#define zend_string_efree(s) zend_string_release((s))
#define GC_ADDREF(p) (++GC_REFCOUNT((p)))
#endif

#if defined(HAVE_APCU_SUPPORT)
# include "ext/apcu/apc_serializer.h"
#endif /* HAVE_APCU_SUPPORT */

#include "php_igbinary.h"

#include "igbinary.h"
#include "igbinary_macros.h"

#include <assert.h>
#include <ctype.h>

#ifndef PHP_WIN32
# include <inttypes.h>
# include <stdbool.h>
# include <stdint.h>
#endif

#include <stddef.h>
#include "hash.h"
#include "hash_ptr.h"
#include "zend_alloc.h"
#include "zend_portability.h"
#include "igbinary_zend_hash.h"

#include "igbinary_impl.h"

/* {{{ enum igbinary_v3_type (V3 Types) */
enum igbinary_v3_type {
	/* NOTE: The values 00-03 were chosen to match the current values for IS_UNDEF..IS_TRUE. */

	/* 00 */ igbinary_v3_type_undef,			/**< Undefined. TODO handle this for a declared instance property that was omitted from serialization */
	/* 01 */ igbinary_v3_type_null,				/**< Null. */
	/* 02 */ igbinary_v3_type_bool_false,		/**< Boolean true. */
	/* 03 */ igbinary_v3_type_bool_true,		/**< Boolean false. */

	/* TODO reindex the other values */

	/* 04 */ igbinary_v3_type_long_literal0,	/**< 0 */
	/* 05 */ igbinary_v3_type_long_literal1,	/**< 1 */
	/* 06 */ igbinary_v3_type_long_literal2,	/**< 2 */
	/* 07 */ igbinary_v3_type_long_literal3,	/**< 3 */
	/* 08 */ igbinary_v3_type_long_literal4,	/**< 4 */
	/* 09 */ igbinary_v3_type_long_literal5,	/**< 5 */
	/* 0a */ igbinary_v3_type_long_literal6,	/**< 6 */
	/* 0b */ igbinary_v3_type_long_literal7,	/**< 7 */

	/* 0c */ igbinary_v3_type_long8p,			/**< Long 8bit positive. */
	/* 0d */ igbinary_v3_type_long8n,			/**< Long 8bit negative. */
	/* 0e */ igbinary_v3_type_long16p,			/**< Long 16bit positive. */
	/* 0f */ igbinary_v3_type_long16n,			/**< Long 16bit negative. */
	/* 10 */ igbinary_v3_type_long32p,			/**< Long 32bit positive. */
	/* 11 */ igbinary_v3_type_long32n,			/**< Long 32bit negative. */
	/* 12 */ igbinary_v3_type_long64signed,		/**< Long signed 64bit (int64_t range). */

	/* 13 */ igbinary_v3_type_double,			/**< C Double. */

	/* 14 */ igbinary_v3_type_string_empty,		/**< Empty string. */
	/* 15 */ igbinary_v3_type_string_char,		/**< Single character string. These are still interned for use in class names for object_id8. */

	/* 16 */ igbinary_v3_type_string8,			/**< String. */
	/* 17 */ igbinary_v3_type_string16,			/**< String. */
	/* 18 */ igbinary_v3_type_string32,			/**< String. */
	/* 19 */ igbinary_v3_type_string64,			/**< String larger than 4GB (originally, php strings had a limit of 32-bit lengths). */

	/* 20 */ igbinary_v3_type_string_private8,		/**< Private property,   8bit, "\x00$className\x00$name" */
	/* 20 */ igbinary_v3_type_string_protected8,	/**< Protected property, 8bit, "\x00*\x00$name" */

	/* 1a */ igbinary_v3_type_string_id8,		/**< String id. (String that was already unserialized) */
	/* 1b */ igbinary_v3_type_string_id16,		/**< String id. */
	/* 1c */ igbinary_v3_type_string_id32,		/**< String id. */

	/* 1d */ igbinary_v3_type_array0,			/**< Empty array. */
	/* 1e */ igbinary_v3_type_array8,			/**< Array. */
	/* 1f */ igbinary_v3_type_array16,			/**< Array. */
	/* 20 */ igbinary_v3_type_array32,			/**< Array. */

	/* 21 */ igbinary_v3_type_list8,			/**< Array with keys 0..count-1 in that order. */
	/* 22 */ igbinary_v3_type_list16,			/**< Array. */
	/* 1e */ igbinary_v3_type_list32,			/**< Array. */

	/* 1d */ igbinary_v3_type_object_id8,		/**< Object id. Indicates the start of an object being unserialized with a class name that has already been used as a string. */
	/* 1e */ igbinary_v3_type_object_id16,		/**< Object id. */
	/* 17 */ igbinary_v3_type_object_id32,		/**< Object id. */

	/* 1f */ igbinary_v3_type_object8,			/**< Object. */
	/* 20 */ igbinary_v3_type_object16,			/**< Object. */
	/* 21 */ igbinary_v3_type_object32,			/**< Object. */

	/* 22 */ igbinary_v3_type_object_ser8,		/**< Object serialized data. */
	/* 23 */ igbinary_v3_type_object_ser16,		/**< Object serialized data. */
	/* 24 */ igbinary_v3_type_object_ser32,		/**< Object serialized data. */

	/* 25 */ igbinary_v3_type_ref,				/**< Simple reference */
	/* 26 */ igbinary_v3_type_enum_case,		/**< PHP 8.1 Enum case. */

	/* 27 */ igbinary_v3_type_ref8,				/**< PHP reference. */
	/* 28 */ igbinary_v3_type_ref16,			/**< PHP reference. */
	/* 29 */ igbinary_v3_type_ref32,			/**< PHP reference. */

};

zend_always_inline static int igbinary_serialize_v3_null(struct igbinary_serialize_data *igsd);
zend_always_inline static int igbinary_serialize_v3_bool(struct igbinary_serialize_data *igsd, int b);
zend_always_inline static int igbinary_serialize_v3_long(struct igbinary_serialize_data *igsd, zend_long l);
zend_always_inline static int igbinary_serialize_v3_double(struct igbinary_serialize_data *igsd, double d);
zend_always_inline static int igbinary_serialize_v3_string(struct igbinary_serialize_data *igsd, zend_string *s);
zend_always_inline static int igbinary_serialize_v3_chararray(struct igbinary_serialize_data *igsd, const char *s, size_t len);

zend_always_inline static int igbinary_serialize_v3_array(struct igbinary_serialize_data *igsd, zval *z, bool object, bool incomplete_class, bool serialize_props);
zend_always_inline static int igbinary_serialize_v3_array_ref(struct igbinary_serialize_data *igsd, zval *z, bool object);
zend_always_inline static int igbinary_serialize_v3_array_sleep(struct igbinary_serialize_data *igsd, zval *z, HashTable *ht, zend_class_entry *ce);
zend_always_inline static int igbinary_serialize_v3_object_name(struct igbinary_serialize_data *igsd, zend_string *name);
zend_always_inline static int igbinary_serialize_v3_object(struct igbinary_serialize_data *igsd, zval *z);

zend_always_inline static int igbinary_unserialize_v3_long(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zend_long *ret);
zend_always_inline static int igbinary_unserialize_v3_double(struct igbinary_unserialize_data *igsd, double *ret);
zend_always_inline static zend_string *igbinary_unserialize_v3_string(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t);
zend_always_inline static zend_string *igbinary_unserialize_v3_chararray(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zend_bool check_interned);

zend_always_inline static int igbinary_unserialize_v3_array(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, int flags, zend_bool create_ref);
zend_always_inline static int igbinary_unserialize_v3_object(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, int flags);
static int igbinary_unserialize_v3_object_ser(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, zend_class_entry *ce);

/* {{{ igbinary_serialize_v3_null */
/** Serializes null. */
inline static int igbinary_serialize_v3_null(struct igbinary_serialize_data *igsd) {
	return igbinary_serialize8(igsd, igbinary_v3_type_null);
}
/* }}} */
/* {{{ igbinary_serialize_v3_bool */
/** Serializes bool. */
inline static int igbinary_serialize_v3_bool(struct igbinary_serialize_data *igsd, int b) {
	return igbinary_serialize8(igsd, (uint8_t)(b ? igbinary_v3_type_bool_true : igbinary_v3_type_bool_false));
}
/* }}} */
/* {{{ igbinary_serialize_v3_long */
/** Serializes zend_long. */
inline static int igbinary_serialize_v3_long(struct igbinary_serialize_data *igsd, zend_long l) {
	const bool p = l >= 0;
	// k is the absolute value of l
	const zend_ulong k = p ? (zend_ulong)l : -(zend_ulong)l;

	if (k <= 0xff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd,
			p ? igbinary_v3_type_long8p : igbinary_v3_type_long8n,
			(uint8_t)k
		));
	} else if (k <= 0xffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd,
			p ? igbinary_v3_type_long16p : igbinary_v3_type_long16n,
			(uint16_t)k
		));
#if SIZEOF_ZEND_LONG == 8
	} else if (k <= 0xffffffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd,
			p ? igbinary_v3_type_long32p : igbinary_v3_type_long32n,
			(uint32_t)k
		));
	} else {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_64(igsd,
			igbinary_v3_type_long64signed,
			(uint64_t)l  /* Use signed 'l'. coerce signed int64 range to unsigned int64 range */
		));
	}
#elif SIZEOF_ZEND_LONG == 4
	} else {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd,
			p ? igbinary_v3_type_long32p : igbinary_v3_type_long32n,
			(uint32_t)k
		));
	}
#else
#error "Strange sizeof(zend_long)."
#endif

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_v3_double */
/** Serializes double. */
inline static int igbinary_serialize_v3_double(struct igbinary_serialize_data *igsd, double d) {
	union {
		double d;
		uint64_t u;
	} u;

	u.d = d;

	return igbinary_serialize8_and_64(igsd, igbinary_v3_type_double, u.u);
}
/* }}} */
/* {{{ igbinary_serialize_v3_string */
/**
 * Serializes string.
 *
 * When compact_strings is true,
 * this will serialize the string as igbinary_v3_type_string* (A length followed by a character array) the first time,
 * and serialize subsequent references to the same string as igbinary_v3_type_string_id*.
 *
 * When compact_strings is false, this will always serialize the string as a character array.
 * compact_strings speeds up serialization, but slows down serialization and uses more space to represent the serialization.
 *
 * Serializes each string once.
 * After first time uses pointers (igbinary_v3_type_string_id*) instead of igbinary_v3_type_string*.
 */
inline static int igbinary_serialize_v3_string(struct igbinary_serialize_data *igsd, zend_string *s) {
	const size_t len = ZSTR_LEN(s);
	if (len <= 1) {
		if (len == 0) {
			/* The empty string is always serialized as igbinary_serialize_v3_type_string_empty (1 byte instead of 2) */
			return igbinary_serialize8(igsd, igbinary_v3_type_string_empty);
		}
		/* The single character string is always serialized as igbinary_serialize_v3_string (2 bytes instead of 3) */
		return igbinary_serialize8_and_8(igsd, (uint8_t)igbinary_v3_type_string_char, ZSTR_VAL(s)[0]);
	}

	if (!igsd->scalar && igsd->compact_strings) {
		struct hash_si_result result = hash_si_find_or_insert(&igsd->strings, s, igsd->string_count);
		if (result.code == hash_si_code_exists) {
			uint32_t value = result.value;
			if (value <= 0xff) {
				RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, (uint8_t)igbinary_v3_type_string_id8, (uint8_t)value));
			} else if (value <= 0xffff) {
				RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, (uint8_t)igbinary_v3_type_string_id16, (uint16_t)value));
			} else {
				RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, (uint8_t)igbinary_v3_type_string_id32, value));
			}
			return 0;
		} else if (EXPECTED(result.code == hash_si_code_inserted)) {
			/* Fall through to igbinary_serialize_v3_chararray */
		} else {
			return 1;  /* Failed to allocate copy of string */
		}
	}

	igsd->string_count++; /* A new string is being serialized - update count so that duplicate class names can be used. */
	if (UNEXPECTED(igsd->string_count == 0)) {
		zend_error(E_WARNING, "igbinary_serialize: Saw too many strings");
		return 1;
	}
	return igbinary_serialize_v3_chararray(igsd, ZSTR_VAL(s), len);
}
/* }}} */
/* {{{ igbinary_serialize_v3_chararray */
/** Serializes string data as the type followed by the length followed by the raw character array. */
inline static int igbinary_serialize_v3_chararray(struct igbinary_serialize_data *igsd, const char *s, size_t len) {
	ZEND_ASSERT(len >= 2);
	uint8_t *append_buffer;
	int offset;
	if (len <= 0xff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, len + 2));
		append_buffer = &igsd->buffer[igsd->buffer_size];
		append_buffer[0] = igbinary_v3_type_string8;
		append_buffer[1] = len;
		offset = 2;
	} else if (len <= 0xffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, len + 3));
		append_buffer = &igsd->buffer[igsd->buffer_size];
		append_buffer[0] = igbinary_v3_type_string16;
		append_buffer[1] = (uint8_t)(len >> 8 & 0xff);
		append_buffer[2] = (uint8_t)(len & 0xff);
		offset = 3;
	} else {
#if SIZEOF_SIZE_T > 4
		if (UNEXPECTED(len > 0xffffffff)) {
			return igbinary_serialize_extremely_long_chararray(igsd, igbinary_v3_type_string64, s, len);
		}
#endif
		RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, len + 5));
		append_buffer = &igsd->buffer[igsd->buffer_size];
		append_buffer[0] = igbinary_v3_type_string32;
		append_buffer[1] = (uint8_t)(len >> 24 & 0xff);
		append_buffer[2] = (uint8_t)(len >> 16 & 0xff);
		append_buffer[3] = (uint8_t)(len >> 8 & 0xff);
		append_buffer[4] = (uint8_t)(len & 0xff);
		offset = 5;
	}

	memcpy(append_buffer + offset, s, len);
	igsd->buffer_size += offset + len;

	return 0;
}
/* }}} */

/* {{{ igbinary_serialize_v3_array */
/**
 * Serializes an array's or object's inner properties.
 * If properties or keys are unexpectedly added (e.g. by __sleep() or serialize() elsewhere), this will skip serializing them.
 * If properties or keys are unexpectedly removed, this will add igbinary_v3_type_null as padding for the corresponding entries.
 */
inline static int igbinary_serialize_v3_array(struct igbinary_serialize_data *igsd, zval *z, bool object, bool incomplete_class, bool serialize_props) {
	/* If object=true: z is IS_OBJECT */
	/* If object=false: z is either IS_ARRAY, or IS_REFERENCE pointing to an IS_ARRAY. */
	HashTable *h;
	/* At the time of writing, struct _zend_array had uint32_t nNumOfElements */
	uint32_t n;
	zval *d;
	zval *z_original;

	zend_string *key;
	zend_long key_index;

	z_original = z;
	ZVAL_DEREF(z);

	ZEND_ASSERT((!object || !serialize_props) && (object || !incomplete_class));

	/* hash */
	if (object) {
		h = zend_get_properties_for(z, ZEND_PROP_PURPOSE_SERIALIZE);
		n = h ? zend_hash_num_elements(h) : 0;
		/* incomplete class magic member */

		if (incomplete_class && n > 0) {
			--n;
		}
	} else {
		h = Z_ARRVAL_P(z);
		n = zend_hash_num_elements(h);
		/* if it is an array or a reference to an array, then add a reference unique to that **reference** to that array */
		if (serialize_props) {
			int ref_ser = igbinary_serialize_v3_array_ref(igsd, z_original, false);
			if (ref_ser != 2) {
				/* 1 means out of memory or error, 0 means this already exists, 2 means this is new. */
				return ref_ser;
			}
		}
	}

	if (n <= 0xff) {
		if (n == 0) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8(igsd, igbinary_v3_type_array0));
			if (object) {
				zend_release_properties(h);
			}
			return 0;
		}
		// FIXME: zend_release_properties if this or other steps fail
		// With the normal emalloc allocator, it should be fatal and not return 0
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, igbinary_v3_type_array8, (uint8_t)n));
	} else if (n <= 0xffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, igbinary_v3_type_array16, (uint16_t)n));
	} else {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, igbinary_v3_type_array32, n));
	}

	/* serialize properties. */
	ZEND_HASH_FOREACH_KEY_VAL(h, key_index, key, d) {
		/* skip magic member in incomplete classes */
		/* Match it exactly and permit null bytes in the middle so that the counts will match. */
		if (incomplete_class && zend_string_equals_literal(key, MAGIC_MEMBER)) {
			continue;
		}

		/* https://wiki.php.net/phpng-int - This is a declared property of an object, or an element of $GLOBALS */
		if (Z_TYPE_P(d) == IS_INDIRECT) {
			d = Z_INDIRECT_P(d);
		}
		if (Z_TYPE_P(d) == IS_UNDEF) {
			/* This is an undefined declared typed property of an object. */
			/* This can't be a value or a reference in an array - except maybe for $GLOBALS, which has other issues. */
			ZEND_ASSERT(!serialize_props);
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_null(igsd));
			continue;
		}

		if (key == NULL) {
			/* Key is numeric */
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_long(igsd, key_index));
		} else {
			/* Key is string */
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_string(igsd, key));
		}

		/* we should still add element even if it's not OK,
		 * since we already wrote the length of the array before */
		RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_zval(igsd, d));
	} ZEND_HASH_FOREACH_END();
	if (object) {
		zend_release_properties(h);
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_v3_array_wrapper */
int igbinary_serialize_v3_array_wrapper(struct igbinary_serialize_data *igsd, zval *z, bool object, bool incomplete_class, bool serialize_props) {
	return igbinary_serialize_v3_array(igsd, z, object, incomplete_class, serialize_props);
}
/* }}} */
/* {{{ igbinary_serialize_v3_array_ref */
/** Serializes array reference (or reference in an object). Returns 0 on success. */
/* TODO: Use different result codes for missing keys and errors */
zend_always_inline static int igbinary_serialize_v3_array_ref(struct igbinary_serialize_data *igsd, zval * const z, bool object) {
	size_t t;
	zend_uintptr_t key;  /* The address of the pointer to the zend_refcounted struct or other struct */
	static int INVALID_KEY;  /* Not used, but we take the pointer of this knowing other zvals won't share it*/

	/* Similar to php_var_serialize_intern's first part, as well as php_add_var_hash, for printing R: (reference) or r:(object) */
	/* However, it differs from the built in serialize() in that references to objects are preserved when serializing and unserializing? (TODO check, test for backwards compatibility) */

	ZEND_ASSERT(Z_ISREF_P(z) || (object && Z_TYPE_P(z) == IS_OBJECT) || Z_TYPE_P(z) == IS_ARRAY);
	// zend_bool is_ref = Z_ISREF_P(z);
	/* Do I have to dereference object references so that reference ids will be the same as in php5? */
	/* If I do, then more tests fail. */
	/* is_ref || IS_OBJECT implies it has a unique refcounted struct */
	// NOTE: The original code would always use the same memory address - Z_COUNTED_P is the start of an object/array/reference
// 	if (object && Z_TYPE_P(z) == IS_OBJECT) {
// 		key = (zend_uintptr_t)Z_OBJ_P(z); /* expand object handle(uint32_t), cast to 32-bit/64-bit pointer */
// 	} else if (is_ref) {
// 		/* NOTE: PHP switched from `zval*` to `zval` for the values stored in HashTables.
// 		 * If an array has two references to the same ZVAL, then those references will have different zvals.
// 		 * We use Z_COUNTED_P(ref), which will be the same if (and only if) the references are the same. */
// 		/* is_ref implies there is a unique reference counting pointer for the reference */
// 		key = (zend_uintptr_t)Z_COUNTED_P(z);
// 	} else if (EXPECTED(Z_TYPE_P(z) == IS_ARRAY)) {
// 		key = (zend_uintptr_t)Z_ARR_P(z);
// 	} else {
// 		/* Nothing else is going to reference this when this is serialized, this isn't ref counted or an object, shouldn't be reached. */
// 		/* Increment the reference id for the deserializer, give up. */
// 		++igsd->references_id;
// 		php_error_docref(NULL, E_NOTICE, "igbinary_serialize_v3_array_ref expected either object or reference (param object=%s), got neither (zend_type=%d)", object ? "true" : "false", (int)Z_TYPE_P(z));
// 		return 1;
// 	}

	/* FIXME hack? If the top-level element was an array, we assume that it can't be a reference when we serialize it, */
	/* because that's the way it was serialized in php5. */
	/* Does this work with different forms of recursive arrays? */
	if (igsd->references_id == 0 && !object) {
		key = (zend_uintptr_t)&INVALID_KEY;
	} else {
		key = (zend_uintptr_t)z->value.ptr;
	}

	t = hash_si_ptr_find_or_insert(&igsd->references, key, igsd->references_id);
	if (t == SIZE_MAX) {
		/* This is a brand new object/array/reference entry with a key equal to igsd->references_id */
		igsd->references_id++;
		if (UNEXPECTED(igsd->references_id == 0)) {
			zend_error(E_WARNING, "igbinary_serialize: Saw too many references");
			return 1;
		}
		/* We only need to call this if the array/object is new, in case __serialize or other methods return temporary arrays or modify arrays that were serialized earlier */
		igsd_addref_and_defer_dtor(&igsd->deferred_dtor_tracker, z);
		return 2;
	}

	/* TODO: Properly handle running out of memory in this helper function. */
	/* It returns 1 both for running out of memory and inserting a new entry. */
	if (t <= 0xff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, (uint8_t)igbinary_v3_type_ref8, (uint8_t)t));
	} else if (t <= 0xffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, (uint8_t)igbinary_v3_type_ref16, (uint16_t)t))
	} else {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, (uint8_t)igbinary_v3_type_ref32, (uint32_t)t))
	}
	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_v3_array_sleep_single_prop_value */
/** Serializes one value of an object's properties array, for use with the __sleep function. */
inline static int igbinary_serialize_v3_array_sleep_single_prop_value(struct igbinary_serialize_data *igsd, zval *z, zval *v, zend_class_entry *ce, zend_string *prop_name) {
	/* Precondition: All args are non-null */
	if (Z_TYPE_P(v) == IS_INDIRECT) {
		v = Z_INDIRECT_P(v);
		if (UNEXPECTED(Z_TYPE_P(v) == IS_UNDEF)) {
#if PHP_VERSION_ID >= 70400
			if (UNEXPECTED(zend_get_typed_property_info_for_slot(Z_OBJ_P(z), v) != NULL)) {
				zend_throw_error(NULL, "Typed property %s::$%s must not be accessed before initialization (in __sleep)", ZSTR_VAL(ce->name), ZSTR_VAL(prop_name));
				return 1;
			}
#endif
			goto serialize_untyped_uninitialized_prop;
		}
	} else {
		if (UNEXPECTED(Z_TYPE_P(v) == IS_UNDEF)) {
serialize_untyped_uninitialized_prop:
			php_error_docref(NULL, E_NOTICE, "\"%s\" returned as member variable from __sleep() but does not exist", ZSTR_VAL(prop_name));
			return igbinary_serialize_v3_null(igsd);
		}
	}
	return igbinary_serialize_v3_zval(igsd, v);
}
/* }}} */
/* {{{ igbinary_serialize_v3_array_sleep_inner */
/** Serializes object's properties array with __sleep -function. */
inline static int igbinary_serialize_v3_array_sleep_inner(struct igbinary_serialize_data *igsd, zval *z, HashTable *h, HashTable *object_properties, zend_class_entry *ce) {
	zval *d;
	zval *v;

	ZEND_HASH_FOREACH_VAL(h, d) {
		if (UNEXPECTED(d == NULL || Z_TYPE_P(d) != IS_STRING)) {
			php_error_docref(NULL, E_NOTICE, "__sleep should return an array only "
					"containing the names of instance-variables to "
					"serialize");

			/* we should still add element even if it's not OK,
			 * since we already wrote the length of the array before
			 * serialize null as key-value pair */
			/* TODO: Allow creating a tmp string, like php's serialize() */
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_null(igsd));
			continue;
		}
		zend_string *prop_name = Z_STR_P(d);

		if ((v = zend_hash_find(object_properties, prop_name)) != NULL) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_string(igsd, prop_name));

			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_array_sleep_single_prop_value(igsd, z, v, ce, prop_name));
		} else {
			zend_string *mangled_prop_name;

			v = NULL;

			int res;
			/* try private */
			mangled_prop_name = zend_mangle_property_name(ZSTR_VAL(ce->name), ZSTR_LEN(ce->name),
				ZSTR_VAL(prop_name), ZSTR_LEN(prop_name), 0);
			v = zend_hash_find(object_properties, mangled_prop_name);

			/* try protected */
			if (v == NULL) {
				zend_string_efree(mangled_prop_name);
				mangled_prop_name = zend_mangle_property_name("*", 1,
					ZSTR_VAL(prop_name), ZSTR_LEN(prop_name), 0);

				v = zend_hash_find(object_properties, mangled_prop_name);
				/* Neither protected nor private property exists */
				if (v == NULL) {
					zend_string_efree(mangled_prop_name);

					php_error_docref(NULL, E_NOTICE, "\"%s\" returned as member variable from __sleep() but does not exist", ZSTR_VAL(prop_name));
					RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_string(igsd, prop_name));

					RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_null(igsd));

					continue;
				}
			}

			res = igbinary_serialize_v3_string(igsd, mangled_prop_name);
			/* igbinary_serialize_v3_string will increase the reference count. */
			zend_string_release_ex(mangled_prop_name, 0);

			RETURN_1_IF_NON_ZERO(res);
			RETURN_1_IF_NON_ZERO(igbinary_serialize_v3_array_sleep_single_prop_value(igsd, z, v, ce, prop_name));
		}
	} ZEND_HASH_FOREACH_END();

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_v3_array_sleep */
/** Serializes object's properties array with __sleep -function. */
inline static int igbinary_serialize_v3_array_sleep(struct igbinary_serialize_data *igsd, zval *z, HashTable *h, zend_class_entry *ce) {
	HashTable *object_properties;
	uint32_t n = zend_hash_num_elements(h);

	/* Serialize array id. */
	if (n <= 0xff) {
		if (n == 0) {
			return igbinary_serialize8_and_8(igsd, igbinary_v3_type_array0, (uint8_t)n);
		}
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, igbinary_v3_type_array8, (uint8_t)n))
	} else if (n <= 0xffff) {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, igbinary_v3_type_array16, (uint16_t)n))
	} else {
		RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, igbinary_v3_type_array32, n))
	}

	object_properties = zend_get_properties_for(z, ZEND_PROP_PURPOSE_SERIALIZE);

	int r = igbinary_serialize_v3_array_sleep_inner(igsd, z, h, object_properties, ce);
	zend_release_properties(object_properties);
	return r;
}
/* }}} */
/* {{{ igbinary_serialize_v3_object_name */
/**
 * Serialize a PHP object's class name.
 * Note that this deliberately ignores the compact_strings setting.
 */
inline static int igbinary_serialize_v3_object_name(struct igbinary_serialize_data *igsd, zend_string *class_name) {
	struct hash_si_result result = hash_si_find_or_insert(&igsd->strings, class_name, igsd->string_count);
	if (result.code == hash_si_code_inserted) {
		const size_t name_len = ZSTR_LEN(class_name);
		igsd->string_count += 1;
		if (UNEXPECTED(igsd->string_count == 0)) {
			zend_error(E_WARNING, "igbinary_serialize: Saw too many strings");
			return 1;
		}

		if (name_len <= 0xff) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, igbinary_v3_type_object8, (uint8_t)name_len))
		} else if (EXPECTED(name_len <= 0xffff)) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, igbinary_v3_type_object16, (uint16_t)name_len))
		} else {
#if SIZEOF_SIZE_T > 4
			if (UNEXPECTED(name_len > 0xffffffff)) {
				zend_error(E_WARNING, "igbinary_serialize_v3_object_name: class name does not fit in 32 bits");
				return 1;
			}
#endif

			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, igbinary_v3_type_object32, (uint32_t)name_len))
		}

		RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, name_len));

		memcpy(igsd->buffer + igsd->buffer_size, ZSTR_VAL(class_name), name_len);
		igsd->buffer_size += name_len;
	} else if (EXPECTED(result.code == hash_si_code_exists)) {
		/* already serialized string */
		uint32_t value = result.value;
		if (value <= 0xff) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_8(igsd, (uint8_t)igbinary_v3_type_object_id8, (uint8_t)value))
		} else if (value <= 0xffff) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_16(igsd, (uint8_t)igbinary_v3_type_object_id16, (uint16_t)value))
		} else {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8_and_32(igsd, (uint8_t)igbinary_v3_type_object_id32, (uint32_t)value))
		}
	} else {
		return 1; /* Failed to allocate copy of string */
	}

	return 0;
}
/* }}} */
/* igbinary_serialize_v3_object_old_serializer_class {{{ */
static ZEND_COLD int igbinary_serialize_v3_object_old_serializer_class(struct igbinary_serialize_data* igsd, zval *z, zend_class_entry *ce) {
	unsigned char *serialized_data = NULL;
	size_t serialized_len;
	int r = 0;

	if (ce->serialize(z, &serialized_data, &serialized_len, (zend_serialize_data *)NULL) == SUCCESS && !EG(exception)) {
		if (UNEXPECTED(igbinary_serialize_v3_object_name(igsd, ce->name) != 0)) {
			goto failure;
		}

		if (serialized_len <= 0xff) {
			if (UNEXPECTED(igbinary_serialize8_and_8(igsd, igbinary_v3_type_object_ser8, (uint8_t)serialized_len) != 0)) {
				goto failure;
			}
		} else if (serialized_len <= 0xffff) {
			if (UNEXPECTED(igbinary_serialize8_and_16(igsd, (uint8_t)igbinary_v3_type_object_ser16, (uint16_t)serialized_len) != 0)) {
				goto failure;
			}
		} else {
#if SIZEOF_ZEND_LONG > 4
			if (UNEXPECTED(serialized_len > 0xffffffff)) {
				zend_error(E_WARNING, "igbinary_serialize_v3_object_old_serializer_class: serialize() data 4GB or larger is not supported");
				goto failure;
			}
#endif
			if (UNEXPECTED(igbinary_serialize8_and_32(igsd, (uint8_t)igbinary_v3_type_object_ser32, (uint32_t)serialized_len) != 0)) {
				goto failure;
			}
		}

		if (UNEXPECTED(igbinary_serialize_resize(igsd, serialized_len))) {
			goto failure;
		}

		memcpy(igsd->buffer + igsd->buffer_size, serialized_data, serialized_len);
		igsd->buffer_size += serialized_len;
	} else if (EG(exception)) {
		/* exception, return failure */
failure:
		r = 1;
	} else {
		/* Serialization callback failed, assume null output */
		r = igbinary_serialize_v3_null(igsd);
	}

	if (serialized_data) {
		efree(serialized_data);
	}

	return r;
}
/* }}} */
/* igbinary_serialize_v3_object_new_serializer {{{ */
#if PHP_VERSION_ID >= 70400
inline static int igbinary_serialize_v3_object_new_serializer(struct igbinary_serialize_data* igsd, zval *z, zend_class_entry *ce) {
	zval retval;
	int r = 0;

	// Call retval = __serialize(z), which returns an array or fails.
	if (igbinary_var_serialize_call_magic_serialize(&retval, z)) {
		// retval is already freed
		if (!EG(exception)) {
			// When does this happen?
			igbinary_serialize_v3_null(igsd);
			return 0;
		}
		return 1;
	}
	// fprintf(stderr, "Serialize returned a value of type %d\n", Z_TYPE(retval));
	if (UNEXPECTED(igbinary_serialize_v3_object_name(igsd, ce->name) != 0)) {
		zval_ptr_dtor(&retval);
		return 1;
	}
	// This serializes an object name followed by an array, in the same format used when no serializers exist.
	// object is false because this is the array returned by __serialize() for the object.
	// TODO: Add tests that this works properly when the same array is reused.
	r = igbinary_serialize_v3_array(igsd, &retval, false, false, false);
	zval_ptr_dtor(&retval);
	if (UNEXPECTED(r)) {
		return 1;
	}
	return EG(exception) != NULL;
}
#endif
/* }}} */
/* {{{ igbinary_serialize_v3_object_enum_case */
#if PHP_VERSION_ID >= 80100
inline static int igbinary_serialize_v3_object_enum_case(struct igbinary_serialize_data *igsd, zend_object *obj, zend_class_entry *ce) {
	if (UNEXPECTED(igbinary_serialize_v3_object_name(igsd, ce->name) != 0)) {
		return 1;
	}
	if (UNEXPECTED(igbinary_serialize8(igsd, (uint8_t)igbinary_v3_type_enum_case))) {
		return 1;
	}
	zval *case_name = zend_enum_fetch_case_name(obj);
	ZEND_ASSERT(Z_TYPE_P(case_name) == IS_STRING);
	return igbinary_serialize_v3_string(igsd, Z_STR_P(case_name));
}
#endif
/* }}} */
/* {{{ igbinary_serialize_v3_object */
/** Serialize object.
 * @see ext/standard/var.c
 * */
inline static int igbinary_serialize_v3_object(struct igbinary_serialize_data *igsd, zval *z) {
	PHP_CLASS_ATTRIBUTES;

	zend_class_entry *ce;

	int r = 0;

	int ref_ser = igbinary_serialize_v3_array_ref(igsd, z, true);
	if (ref_ser != 2) {
		return ref_ser;
	}

	ce = Z_OBJCE_P(z);

	if (IGBINARY_IS_NOT_SERIALIZABLE(ce)) {
		zend_throw_exception_ex(NULL, 0, "Serialization of '%s' is not allowed", ZSTR_VAL(ce->name));
		return 1;
	}

#if PHP_VERSION_ID >= 80100
	if (ce->ce_flags & ZEND_ACC_ENUM) {
		return igbinary_serialize_v3_object_enum_case(igsd, Z_OBJ_P(z), ce);
	}
#endif

#if PHP_VERSION_ID >= 70400
#if PHP_VERSION_ID >= 80000
	if (ce->__serialize)
#else
	if (zend_hash_str_exists(&ce->function_table, "__serialize", sizeof("__serialize")-1))
#endif
	{
		// fprintf(stderr, "Going to serialize %s\n", ZSTR_VAL(ce->name));
		return igbinary_serialize_v3_object_new_serializer(igsd, z, ce);
	}
#endif
	if (ce->serialize != NULL) {
		return igbinary_serialize_v3_object_old_serializer_class(igsd, z, ce);
	}

	/* serialize class name */
	PHP_SET_CLASS_ATTRIBUTES(z);
	if (igbinary_serialize_v3_object_name(igsd, class_name) != 0) {
		PHP_CLEANUP_CLASS_ATTRIBUTES();
		return 1;
	}
	PHP_CLEANUP_CLASS_ATTRIBUTES();

	if (ce && ce != PHP_IC_ENTRY && zend_hash_str_exists(&ce->function_table, "__sleep", sizeof("__sleep") - 1)) {
		zval h;
		zval f;
		/* function name string */
		/* TODO use ZSTR_KNOWN(ZEND_STR_SLEEP) instead when available*/
		/* zval *zv = zend_hash_find_known_hash(&ce->function_table, ZSTR_KNOWN(ZEND_STR_SLEEP)); */
		ZVAL_STRINGL(&f, "__sleep", sizeof("__sleep") - 1);

		/* calling z->__sleep */
		r = call_user_function(CG(function_table), z, &f, &h, 0, 0);

		zval_ptr_dtor_str(&f);

		if (r == SUCCESS && !EG(exception)) {
			HashTable *ht;
			r = 0;

			if (Z_TYPE(h) == IS_UNDEF) {
				/* FIXME: is this ok? */
				/* Valid, but skip */
			} else if ((ht = HASH_OF(&h)) != NULL) {
				/* NOTE: PHP permits returning an object in __sleep */
				r = igbinary_serialize_v3_array_sleep(igsd, z, ht, ce);
			} else {
				php_error_docref(NULL, E_NOTICE, "__sleep should return an array only "
						"containing the names of instance-variables to "
						"serialize");

				/* empty array */
				r = igbinary_serialize8(igsd, igbinary_v3_type_array0);
			}
		} else {
			r = 1;
		}

		/* cleanup */
		zval_ptr_dtor(&h);

		return r;
	}
	return igbinary_serialize_v3_array(igsd, z, true, incomplete_class, false);
}
/* }}} */
/* {{{ igbinary_warn_serialize_resource */
ZEND_COLD int igbinary_warn_serialize_resource(zval *z) {
	const char *resource_type;
	resource_type = zend_rsrc_list_get_rsrc_type(Z_RES_P(z));
	if (!resource_type) {
		resource_type = "Unknown";
	}
	php_error_docref(NULL, E_DEPRECATED, "Cannot serialize resource(%s) and resources may be converted to objects that cannot be serialized in future php releases. Serializing the value as null instead", resource_type);
	return EG(exception) != NULL;
}
/* }}} */
/* {{{ igbinary_serialize_v3_zval */
/** Serialize zval. */
static int igbinary_serialize_v3_zval(struct igbinary_serialize_data *igsd, zval *z) {
	if (Z_ISREF_P(z)) {
		if (Z_REFCOUNT_P(z) >= 2) {
			RETURN_1_IF_NON_ZERO(igbinary_serialize8(igsd, (uint8_t)igbinary_v3_type_ref))

			switch (Z_TYPE_P(Z_REFVAL_P(z))) {
			case IS_ARRAY:
				return igbinary_serialize_v3_array(igsd, z, false, false, true);
			case IS_OBJECT:
				break; /* Fall through */
			default:
				{
					/* Serialize a reference if zval already added */
					int ref_ser = igbinary_serialize_v3_array_ref(igsd, z, false);
					if (ref_ser != 2) {
						return ref_ser;
					}
					/* Fall through */
				}
			}
		}

		z = Z_REFVAL_P(z);
	}
	switch (Z_TYPE_P(z)) {
		case IS_RESOURCE:
			if (igbinary_warn_serialize_resource(z)) {
				return 1;
			}
			return igbinary_serialize_v3_null(igsd);
		case IS_OBJECT:
			return igbinary_serialize_v3_object(igsd, z);
		case IS_ARRAY:
			/* if is_ref, then php5 would have called igbinary_serialize_v3_array_ref */
			return igbinary_serialize_v3_array(igsd, z, false, false, true);
		case IS_STRING:
			return igbinary_serialize_v3_string(igsd, Z_STR_P(z));
		case IS_LONG:
			return igbinary_serialize_v3_long(igsd, Z_LVAL_P(z));
		case IS_UNDEF:
			// https://github.com/igbinary/igbinary/issues/134
			// TODO: In a new major version, could have a separate type for IS_UNDEF, which would unset the property in an object context?
			// fallthrough
		case IS_NULL:
			return igbinary_serialize_v3_null(igsd);
		case IS_TRUE:
			return igbinary_serialize_v3_bool(igsd, 1);
		case IS_FALSE:
			return igbinary_serialize_v3_bool(igsd, 0);
		case IS_DOUBLE:
			return igbinary_serialize_v3_double(igsd, Z_DVAL_P(z));
		default:
			zend_error(E_ERROR, "igbinary_serialize_v3_zval: zval has unknown type %d", (int)Z_TYPE_P(z));
			return 1;
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_v3_zval_wrapper */
int igbinary_serialize_v3_zval_wrapper(struct igbinary_serialize_data *igsd, zval *z) {
	return igbinary_serialize_v3_zval(igsd, z);
}
/* }}} */
/* End of functions for serialize v2, start of functions for unserialize v3 */
/* {{{ igbinary_unserialize_v3_long */
/** Unserializes zend_long */
inline static int igbinary_unserialize_v3_long(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zend_long *ret) {
	if (t == igbinary_v3_type_long8p || t == igbinary_v3_type_long8n) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_long: end-of-data");
			return 1;
		}

		*ret = (zend_long)(t == igbinary_v3_type_long8n ? -1 : 1) * igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_long16p || t == igbinary_v3_type_long16n) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_long: end-of-data");
			return 1;
		}

		*ret = (zend_long)(t == igbinary_v3_type_long16n ? -1 : 1) * igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_long32p || t == igbinary_v3_type_long32n) {
		uint32_t tmp32;
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_long: end-of-data");
			return 1;
		}

		/* check for boundaries (perform only one comparison in common case) */
		tmp32 = igbinary_unserialize32(igsd);
#if SIZEOF_ZEND_LONG == 4
		if (UNEXPECTED(tmp32 >= 0x80000000 && (tmp32 > 0x80000000 || t == igbinary_v3_type_long32p))) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_long: 64bit long on 32bit platform?");
			tmp32 = 0; /* t == igbinary_v3_type_long32p ? LONG_MAX : LONG_MIN; */
		}
#endif
		*ret = (zend_long)(t == igbinary_v3_type_long32n ? -1 : 1) * tmp32;
	} else {
#if SIZEOF_ZEND_LONG == 8
		uint64_t tmp64;
#endif

		ZEND_ASSERT(t == igbinary_v3_type_long64signed);
#if SIZEOF_ZEND_LONG == 8
		if (IGB_NEEDS_MORE_DATA(igsd, 8)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_long: end-of-data");
			return 1;
		}

		/* check for boundaries (perform only one comparison in common case) */
		tmp64 = igbinary_unserialize64(igsd);

		*ret = (zend_long)tmp64;
#elif SIZEOF_ZEND_LONG == 4
		/* can't put 64bit long into 32bit one, placeholder zero */
		*ret = 0;
		igbinary_unserialize64(igsd);
		zend_error(E_WARNING, "igbinary_unserialize_v3_long: 64bit long on 32bit platform");
#else
#error "Strange sizeof(zend_long)."
#endif
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_double */
/** Unserializes double. */
inline static int igbinary_unserialize_v3_double(struct igbinary_unserialize_data *igsd, double *ret) {
	union {
		double d;
		uint64_t u;
	} u;

	if (IGB_NEEDS_MORE_DATA(igsd, 8)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_double: end-of-data");
		return 1;
	}

	u.u = igbinary_unserialize64(igsd);

	*ret = u.d;

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_string */
/** Unserializes string by string id. Returns NULL on error. */
inline static zend_string *igbinary_unserialize_v3_string(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t) {
	size_t i;
	zend_string *zstr;
	if (t == igbinary_v3_type_string_id8 || t == igbinary_v3_type_object_id8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_string: end-of-data");
			return NULL;
		}
		i = igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_string_id16 || t == igbinary_v3_type_object_id16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_string: end-of-data");
			return NULL;
		}
		i = igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_string_id32 || t == igbinary_v3_type_object_id32) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_string: end-of-data");
			return NULL;
		}
		i = igbinary_unserialize32(igsd);
	} else {
		/* Callers are all cases of a switch statement */
#ifdef ZEND_DEBUG
		zend_error(E_WARNING, "igbinary_unserialize_v3_string: unexpected type 0x%02x, should not happen", (int)t);
#endif
		ZEND_UNREACHABLE();
		return NULL;
	}

	if (UNEXPECTED(i >= igsd->strings_count)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_string: string index is out-of-bounds");
		return NULL;
	}

	zstr = igsd->strings[i];
	// Add one more ref (currently not using any interned strings) - Callers of this will decrease refs as needed
#if PHP_VERSION_ID >= 80100
	zend_string_addref(zstr);
#else
	ZEND_ASSERT(!ZSTR_IS_INTERNED(zstr));
	GC_ADDREF(zstr);
#endif
	return zstr;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_char*/
/** Unserializes single byte string. Returns NULL on error. */
inline static zend_string *igbinary_unserialize_v3_char(struct igbinary_unserialize_data *igsd) {
	if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: end-of-data");
		return NULL;
	}
	uint8_t c = igbinary_unserialize8(igsd);
	return ZSTR_CHAR(c);
}
/* }}} */
/* {{{ igbinary_unserialize_v3_chararray */
/** Unserializes chararray of string. Returns NULL on error. */
inline static zend_string *igbinary_unserialize_v3_chararray(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zend_bool check_interned) {
	size_t l;
	zend_string *zstr;

	if (t == igbinary_v3_type_string8 || t == igbinary_v3_type_object8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: end-of-data");
			return NULL;
		}
		l = igbinary_unserialize8(igsd);
		/* Requires converting these into interned strings. Maybe add one-char in v3 of igbinary format?
		if (l == 1) {
			zstr = ZSTR_CHAR((zend_uchar)igsd->buffer[igsd->buffer_offset]);
			igsd->strings[igsd->strings_count] = zstr;
			igsd->strings_count += 1;
			igsd->buffer_offset++;
			return zstr;
		}
		*/
	} else if (t == igbinary_v3_type_string16 || t == igbinary_v3_type_object16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: end-of-data");
			return NULL;
		}
		l = igbinary_unserialize16(igsd);
	} else if (EXPECTED(t == igbinary_v3_type_string32 || t == igbinary_v3_type_object32)) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: end-of-data");
			return NULL;
		}
		l = igbinary_unserialize32(igsd);
	} else if (t == igbinary_v3_type_string64) {
		return igbinary_unserialize_extremely_long_chararray(igsd);
	} else {
		zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
		return NULL;
	}
	if (IGB_NEEDS_MORE_DATA(igsd, l)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_chararray: end-of-data");
		return NULL;
	}

	if (igsd->strings_count + 1 > igsd->strings_capacity) {
		zend_string **new_strings;
		igsd->strings_capacity *= 2;

		new_strings = (zend_string **)erealloc(igsd->strings, sizeof(zend_string *) * igsd->strings_capacity);
		if (new_strings == NULL) {
			// The cleanup function will take care of destroying the allocated zend_strings.
			return NULL;
		}
		igsd->strings = new_strings;
	}

#if PHP_VERSION_ID >= 80100
	if (check_interned && l < 100) {
		/*
		 * Reuse interned strings if possible for the following reasons:
		 * 1. Save memory (e.g. for strings in property default values of classes, for arrays repeating string field names, etc.)
		 * 2. Speed up checking if strings are identical.
		 * 3. Speed up the code that ends up using the return value of igbinary_unserialize().
		 *
		 * Note that this change has mixed results. unserialize-object-array and unserialize-stdclass have performance improve because all strings are already interned,
		 * but unserialize-stringarray on unstructured text has worse performance.
		 */
		zstr = zend_string_init_existing_interned((const char*)igsd->buffer_ptr, l, 0);
		zend_string_addref(zstr);
	} else
#endif
	{
		zstr = zend_string_init((const char*)igsd->buffer_ptr, l, 0);
		GC_ADDREF(zstr); /* definitely not interned. Add a reference in case the first reference gets deleted before reusing the temporary string */
	}

	igsd->buffer_ptr += l;

	igsd->strings[igsd->strings_count] = zstr;
	igsd->strings_count += 1;
	return zstr;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_array */
/** Unserializes a PHP array. */
zend_always_inline static int igbinary_unserialize_v3_array(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, int flags, zend_bool create_ref) {
	/* WANT_REF means that z will be wrapped by an IS_REFERENCE */
	uint32_t n;
	uint32_t i;

	enum igbinary_v3_type key_type;

	HashTable *h;

	if (t == igbinary_v3_type_array0) {
		if (create_ref) {
			/* NOTE: igbinary uses ZVAL_ARR() when reading the created reference, which assumes that the original array is refcounted. */
			/* This would have to be fixed to switch to ZVAL_EMPTY_ARRAY for the empty array (there's probably not benefit to making that switch). */
			/* Otherwise, using ZVAL_EMPTY_ARRAY would segfault. */
#if PHP_VERSION_ID >= 70300
			ZVAL_EMPTY_ARRAY(z);
#else
			array_init_size(z, 0);
#endif
			struct igbinary_value_ref ref;
			if (flags & WANT_REF) {
				ZVAL_NEW_REF(z, z);
				ref.reference.reference = Z_REF_P(z);
				ref.type = IG_REF_IS_REFERENCE;
			} else {
#if PHP_VERSION_ID >= 70300
				ref.type = IG_REF_IS_EMPTY_ARRAY;
#else
				ref.reference.array = Z_ARR_P(z);
				ref.type = IG_REF_IS_ARRAY;
#endif
			}
			/* add the new array to the list of unserialized references */
			RETURN_1_IF_NON_ZERO(igsd_append_ref(igsd, ref) == SIZE_MAX);
		} else {
			/* This only matters if __unserialize() would get called with an empty array */
#if PHP_VERSION_ID >= 70300
			ZVAL_EMPTY_ARRAY(z);
#else
			array_init_size(z, 0);
#endif
		}
		return 0;
	}
	if (t == igbinary_v3_type_array8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_array: end-of-data");
			return 1;
		}
		n = igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_array16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_array: end-of-data");
			return 1;
		}
		n = igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_array32) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_array: end-of-data");
			return 1;
		}
		n = igbinary_unserialize32(igsd);
	} else {
		zend_error(E_WARNING, "igbinary_unserialize_v3_array: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
		return 1;
	}

	/* n cannot be larger than the number of minimum "objects" in the array */
	if (IGB_NEEDS_MORE_DATA(igsd, n)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_array: data size %zu smaller that requested array length %zu.", (size_t)IGB_REMAINING_BYTES(igsd), (size_t)n);
		return 1;
	}

	zval *z_deref = z;
	if (flags & WANT_REF) {
		if (!Z_ISREF_P(z)) {
			ZVAL_NEW_REF(z, z);
			z_deref = Z_REFVAL_P(z);
		}
	}
	array_init_size(z_deref, n);
	h = Z_ARRVAL_P(z_deref);
#if PHP_VERSION_ID >= 70200
	/* The array may contain references to itself, in which case we'll be modifying an
	 * rc>1 array. This is okay, since the array is, ostensibly, only visible to
	 * unserialize (in practice unserialization handlers also see it). */
	HT_ALLOW_COW_VIOLATION(h);
#endif
	if (create_ref) {
		/* Only create a reference if this is not from __unserialize(), because the existence of __unserialize can change */
		struct igbinary_value_ref ref;
		if (flags & WANT_REF) {
			// We converted to reference earlier.
			ref.reference.reference = Z_REF_P(z);
			ref.type = IG_REF_IS_REFERENCE;
		} else {
			ref.reference.array = Z_ARR_P(z_deref);
			ref.type = IG_REF_IS_ARRAY;
		}
		/* add the new array to the list of unserialized references */
		RETURN_1_IF_NON_ZERO(igsd_append_ref(igsd, ref) == SIZE_MAX);
	}

	for (i = 0; i < n; i++) {
		zval *vp;
		zend_long key_index = 0;
		zend_string *key_str = NULL; /* NULL means use key_index */

		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_array: end-of-data");
cleanup:
			zval_dtor(z);
			ZVAL_NULL(z);
			return 1;
		}

		key_type = (enum igbinary_v3_type)igbinary_unserialize8(igsd);

		switch (key_type) {
			case igbinary_v3_type_long8p:
			case igbinary_v3_type_long16p:
			case igbinary_v3_type_long8n:
			case igbinary_v3_type_long16n:
			case igbinary_v3_type_long32p:
			case igbinary_v3_type_long32n:
			case igbinary_v3_type_long64signed:
				if (UNEXPECTED(igbinary_unserialize_v3_long(igsd, key_type, &key_index))) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_string_id8:
			case igbinary_v3_type_string_id16:
			case igbinary_v3_type_string_id32:
				key_str = igbinary_unserialize_v3_string(igsd, key_type);
				if (UNEXPECTED(key_str == NULL)) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_string_empty:
				key_str = ZSTR_EMPTY_ALLOC();
				break;
			case igbinary_v3_type_string_char:
				key_str = igbinary_unserialize_v3_char(igsd);
				if (UNEXPECTED(key_str == NULL)) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_string8:
			case igbinary_v3_type_string16:
			case igbinary_v3_type_string32:
			case igbinary_v3_type_string64:
				key_str = igbinary_unserialize_v3_chararray(igsd, key_type, 1);
				if (UNEXPECTED(key_str == NULL)) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_null:
				continue;
			default:
				zend_error(E_WARNING, "igbinary_unserialize_v3_array: unknown key type '%02x', position %zu", key_type, (size_t)IGB_BUFFER_OFFSET(igsd));
				goto cleanup;
		}

		/* first add key into array so references can properly and not stack allocated zvals */
		/* Use NULL because inserting UNDEF into array does not add a new element */
		if (key_str != NULL) {
			vp = igbinary_zend_hash_add_or_find(h, key_str);
			/* If there was an old value instead of an inserted IS_NULL zval (unlikely) and that old value was refcounted, then add a reference and defer freeing that reference */
			if (UNEXPECTED(igsd_addref_and_defer_dtor(&igsd->deferred_dtor_tracker, vp))) {
				return 1;
			}

			zend_string_release(key_str);
		} else {
			/* If there was an old value instead of an inserted IS_NULL zval (unlikely) and that old value was refcounted, then add a reference and defer freeing that reference */
			vp = igbinary_zend_hash_index_add_or_find(h, key_index);
			if (UNEXPECTED(igsd_addref_and_defer_dtor(&igsd->deferred_dtor_tracker, vp))) {
				return 1;
			}
		}

		ZEND_ASSERT(vp != NULL);
		if (Z_TYPE_P(vp) == IS_INDIRECT) {
			/* TODO: In php 8.1+, IS_INDIRECT checks and macros may become unnecessary
			 * due to $GLOBALS being converted to a regular array in https://wiki.php.net/rfc/restrict_globals_usage */
			vp = Z_INDIRECT_P(vp);
		}

		ZEND_ASSERT(vp != NULL);
		if (UNEXPECTED(igbinary_unserialize_v3_zval(igsd, vp, WANT_CLEAR))) {
			return 1;
		}
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_object_properties */
/** Unserializes the array of object properties and adds those to the object z. */
inline static int igbinary_unserialize_v3_object_properties(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, const zend_class_entry *ce) {
	/* WANT_REF means that z will be wrapped by an IS_REFERENCE */
	uint32_t n;

	zval v;
	zval *z_deref;

	HashTable *h;
	zend_bool did_extend;

	if (t == igbinary_v3_type_array0) {
		return 0;
	}
	if (t == igbinary_v3_type_array8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: end-of-data");
			return 1;
		}
		n = igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_array16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: end-of-data");
			return 1;
		}
		n = igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_array32) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: end-of-data");
			return 1;
		}
		n = igbinary_unserialize32(igsd);
	} else {
#ifdef ZEND_DEBUG
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
#endif
		ZEND_UNREACHABLE();
		return 1;
	}

	/* n cannot be larger than the number of minimum "objects" in the array */
	if (IGB_NEEDS_MORE_DATA(igsd, n)) {
		zend_error(E_WARNING, "%s: data size %zu smaller that requested array length %zu.", "igbinary_unserialize_v3_object_properties", (size_t)IGB_REMAINING_BYTES(igsd), (size_t)n);
		return 1;
	}

	z_deref = z;
	ZVAL_DEREF(z_deref);

	/* empty array */
	if (n == 0) {
		return 0;
	}

	h = HASH_OF_OBJECT(z_deref);

	did_extend = 0;

	do {
		n--;
		zval *vp;
		enum igbinary_v3_type key_type;
		zend_string *key_str = NULL; /* NULL means use key_index */

		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: end-of-data");
			zval_dtor(z);
			ZVAL_NULL(z);
			return 1;
		}

		key_type = (enum igbinary_v3_type)igbinary_unserialize8(igsd);

		switch (key_type) {
			case igbinary_v3_type_long8p:
			case igbinary_v3_type_long8n:
			case igbinary_v3_type_long16p:
			case igbinary_v3_type_long16n:
			case igbinary_v3_type_long32p:
			case igbinary_v3_type_long32n:
			case igbinary_v3_type_long64signed:
			{
				zend_long key_index = 0;
				if (UNEXPECTED(igbinary_unserialize_v3_long(igsd, key_type, &key_index))) {
long_cleanup:
					zval_dtor(z);
					ZVAL_UNDEF(z);
					return 1;
				}
				key_str = zend_long_to_str(key_index);
				if (UNEXPECTED(key_str == NULL)) {
					goto long_cleanup;
				}
				break;
			}
			case igbinary_v3_type_string_id8:
			case igbinary_v3_type_string_id16:
			case igbinary_v3_type_string_id32:
				key_str = igbinary_unserialize_v3_string(igsd, key_type);
				if (UNEXPECTED(key_str == NULL)) {
cleanup:
					zval_dtor(z);
					ZVAL_UNDEF(z);
					return 1;
				}
				break;
			case igbinary_v3_type_string_empty:
				key_str = ZSTR_EMPTY_ALLOC();
				break;
			case igbinary_v3_type_string_char:
				key_str = igbinary_unserialize_v3_char(igsd);
				if (UNEXPECTED(key_str == NULL)) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_string8:
			case igbinary_v3_type_string16:
			case igbinary_v3_type_string32:
			case igbinary_v3_type_string64:
				key_str = igbinary_unserialize_v3_chararray(igsd, key_type, 1);
				if (UNEXPECTED(key_str == NULL)) {
					goto cleanup;
				}
				break;
			case igbinary_v3_type_null:
				continue;  /* Skip unserializing this element, serialized with no value. In C, this applies to loop, not switch. */
			default:
				zend_error(E_WARNING, "igbinary_unserialize_v3_object_properties: unknown key type '%02x', position %zu", key_type, (size_t)IGB_BUFFER_OFFSET(igsd));
				goto cleanup;
		}

		/* first add key into array so references can properly and not stack allocated zvals */
		/* Use NULL because inserting UNDEF into array does not add a new element */
		ZVAL_NULL(&v);
		zval *prototype_value = zend_hash_find(h, key_str);
#if PHP_VERSION_ID >= 70400
		zend_property_info *info = NULL;
#endif
		if (prototype_value != NULL) {
			if (Z_TYPE_P(prototype_value) == IS_INDIRECT) {
				/* This is a declared object property */
				prototype_value = Z_INDIRECT_P(prototype_value);
#if PHP_VERSION_ID >= 70400
				info = zend_get_typed_property_info_for_slot(Z_OBJ_P(z_deref), prototype_value);
#endif
			}
			/* This is written to avoid the overhead of a second zend_hash_update call. See https://github.com/php/php-src/pull/5095 */
			/* Use igsd_addref_and_defer_dtor instead (like php-src), in case of gc causing issues. */
			/* Something already added a reference, so just defer the destructor */
			if (UNEXPECTED(igsd_defer_dtor(&igsd->deferred_dtor_tracker, prototype_value))) {
				zend_string_release(key_str);
				return 1;
			}
			/* Just override the original value directly */
			ZVAL_COPY_VALUE(prototype_value, &v);
			vp = prototype_value;
		} else {
#if PHP_VERSION_ID >= 80000
			/* ZEND_ACC_NO_DYNAMIC_PROPERTIES was introduced in php 8.0 but was largely used with zend_class_unserialize_deny, though external pecls might set this flag. */
			if (UNEXPECTED(ce->ce_flags & ZEND_ACC_NO_DYNAMIC_PROPERTIES)) {
				zend_throw_error(NULL, "Cannot create dynamic property %s::$%s in igbinary_unserialize",
					ZSTR_VAL(ce->name), zend_get_unmangled_property_name(key_str));
				zend_string_release(key_str);
				return 1;
			}
#endif
#if PHP_VERSION_ID >= 80200
			/* PHP 8.2 deprecated the creation of dynamic properties by default without `#[AllowDynamicProperties]`. */
			if (UNEXPECTED(!(ce->ce_flags & ZEND_ACC_ALLOW_DYNAMIC_PROPERTIES))) {
				php_error_docref(NULL, E_DEPRECATED, "Creation of dynamic property %s::$%s is deprecated",
					ZSTR_VAL(ce->name), zend_get_unmangled_property_name(key_str));
				if (UNEXPECTED(EG(exception))) {
					zend_string_release(key_str);
					return 1;
				}
			}
#endif
			if (!did_extend) {
				/* remaining_elements is at least one, because we're looping from n-1..0 */
				uint32_t remaining_elements = n + 1;
				/* Copied from var_unserializer.re. Need to ensure that IGB_REF_VAL doesn't point to invalid data. */
				/* Worst case: All remaining_elements of the added properties are dynamic. */
				zend_hash_extend(h, zend_hash_num_elements(h) + remaining_elements, (h->u.flags & HASH_FLAG_PACKED));
				did_extend = 1;
			}
			vp = zend_hash_add_new(h, key_str, &v);
		}

		zend_string_release(key_str);

		/* Should only be indirect for typed properties? */
		ZEND_ASSERT(Z_TYPE_P(vp) != IS_INDIRECT);

		if (UNEXPECTED(igbinary_unserialize_v3_zval(igsd, vp, WANT_CLEAR))) {
			/* Unserializing a property into this zval has failed. */
			/* zval_ptr_dtor(z); */
			/* zval_ptr_dtor(vp); */
			return 1;
		}
#if PHP_VERSION_ID >= 70400
		if (UNEXPECTED(info)) {
			if (!zend_verify_prop_assignable_by_ref(info, vp, /* strict */ 1)) {
				zval_ptr_dtor(vp);
				ZVAL_UNDEF(vp);
				return 1;
			}
			if (Z_ISREF_P(vp)) {
				ZEND_REF_ADD_TYPE_SOURCE(Z_REF_P(vp), info);
			}
		}
#endif
	} while (n > 0);

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_object_ser */
/** Unserializes object's property array. This is used to serialize objects implementing Serializable -interface. */
static ZEND_COLD int igbinary_unserialize_v3_object_ser(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, zend_class_entry *ce) {
	size_t n;
	int ret;
	php_unserialize_data_t var_hash;

	if (ce->unserialize == NULL) {
		/* Should be impossible. */
		zend_error(E_WARNING, "Class %s has no unserializer", ZSTR_VAL(ce->name));
		return 1;
	}

	if (IGBINARY_IS_NOT_UNSERIALIZABLE(ce)) {
		zend_throw_exception_ex(NULL, 0, "Unserialization of '%s' is not allowed", ZSTR_VAL(ce->name));
		return 1;
	}

	if (t == igbinary_v3_type_object_ser8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_ser: end-of-data");
			return 1;
		}
		n = igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_object_ser16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_ser: end-of-data");
			return 1;
		}
		n = igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_object_ser32) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_ser: end-of-data");
			return 1;
		}
		n = igbinary_unserialize32(igsd);
	} else {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_ser: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
		return 1;
	}

	if (IGB_NEEDS_MORE_DATA(igsd, n)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_ser: end-of-data");
		return 1;
	}

	PHP_VAR_UNSERIALIZE_INIT(var_hash);
	ret = ce->unserialize(
		z,
		ce,
		(const unsigned char *)igsd->buffer_ptr,
		n,
		(zend_unserialize_data *)&var_hash
	);
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

	if (ret != SUCCESS || EG(exception)) {
		return 1;
	}

	igsd->buffer_ptr += n;

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_object_enum_case */
#if PHP_VERSION_ID >= 80100
/** Unserializes object's property array. This is used to serialize objects implementing Serializable -interface. */
static int igbinary_unserialize_v3_object_enum_case(struct igbinary_unserialize_data *igsd, zval *const z, zend_class_entry *ce) {
	if (UNEXPECTED(!(ce->ce_flags & ZEND_ACC_ENUM))) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: Class '%s' is not an enum", ZSTR_VAL(ce->name));
		return 1;
	}
	if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: end-of-data");
		return 1;
	}

	enum igbinary_v3_type t = (enum igbinary_v3_type)igbinary_unserialize8(igsd);
	zend_string *case_name;
	switch (t) {
		case igbinary_v3_type_string8:
		case igbinary_v3_type_string16:
		case igbinary_v3_type_string32:
		case igbinary_v3_type_string64:
			case_name = igbinary_unserialize_v3_chararray(igsd, t, 1);
			break;
		case igbinary_v3_type_string_id8:
		case igbinary_v3_type_string_id16:
		case igbinary_v3_type_string_id32:
			case_name = igbinary_unserialize_v3_string(igsd, t);
			break;
		default:
			zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: Unexpected case name type %02x, expected char array or string id", (int)t);
			return 1;
	}
	if (UNEXPECTED(!case_name)) {
		return 1;
	}

	zval *zv = zend_hash_find(&ce->constants_table, case_name);
	if (UNEXPECTED(!zv)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: Undefined constant %s::%s", ZSTR_VAL(ce->name), ZSTR_VAL(case_name));
		zend_string_release(case_name);
		return 1;
	}

	zend_class_constant *c = Z_PTR_P(zv);
	if (UNEXPECTED(!(ZEND_CLASS_CONST_FLAGS(c) & ZEND_CLASS_CONST_IS_CASE))) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: %s::%s is not an enum case", ZSTR_VAL(ce->name), ZSTR_VAL(case_name));
		zend_string_release(case_name);
		return 1;
	}
	zend_string_release(case_name);
	zval *value = &c->value;
	if (Z_TYPE_P(value) == IS_CONSTANT_AST) {
		zval_update_constant_ex(value, c->ce);
		if (UNEXPECTED(EG(exception) != NULL)) {
			return 1;
		}
	}
	ZEND_ASSERT(Z_TYPE_P(value) == IS_OBJECT);
	/* increment the reference count and copy the enum case object into the constructed value. */
	ZVAL_COPY(z, value);

	return 0;
}
#endif
/* }}} */
/* {{{ igbinary_unserialize_v3_object */
/** Unserialize an object.
 * @see ext/standard/var_unserializer.c in the php-src repo. Parts of this code are based on that.
 */
zend_always_inline static int igbinary_unserialize_v3_object(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *z, int flags) {
	zend_class_entry *ce;

	size_t ref_n;

	zend_string *class_name;

	int r;

	bool incomplete_class = false;
	bool is_from_serialized_data = false;

	if (t == igbinary_v3_type_object8 || t == igbinary_v3_type_object16 || t == igbinary_v3_type_object32) {
		class_name = igbinary_unserialize_v3_chararray(igsd, t, 1);
	} else if (t == igbinary_v3_type_object_id8 || t == igbinary_v3_type_object_id16 || t == igbinary_v3_type_object_id32) {
		class_name = igbinary_unserialize_v3_string(igsd, t);
	} else {
		/* Should not happen, callers are all known cases of a switch statement */
#if ZEND_DEBUG
		zend_error(E_WARNING, "igbinary_unserialize_v3_object: unknown object type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
#endif
		ZEND_UNREACHABLE();
		return 1;
	}

	if (UNEXPECTED(class_name == NULL)) {
		return 1;
	}

	do {
		zval user_func;
		zval retval;
		zval args[1];
		const char* user_func_name;

		/* Try to find class directly */
		if (EXPECTED((ce = zend_lookup_class(class_name)) != NULL)) {
			/* FIXME: lookup class may cause exception in load callback */
			break;
		}

		user_func_name = PG(unserialize_callback_func);
		/* Check for unserialize callback */
		if ((user_func_name == NULL) || (user_func_name[0] == '\0')) {
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
			break;
		}

		/* Call unserialize callback */
		ZVAL_STRING(&user_func, user_func_name);
		ZVAL_STR(&args[0], class_name);
		if (call_user_function(CG(function_table), NULL, &user_func, &retval, 1, args) != SUCCESS) {
			php_error_docref(NULL, E_WARNING, "defined (%s) but not found", Z_STRVAL(user_func));
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
			zval_ptr_dtor_nogc(&user_func);
			break;
		}
		/* FIXME: always safe? */
		zval_ptr_dtor(&retval);
		zval_ptr_dtor_str(&user_func);

		/* User function call may have raised an exception */
		if (EG(exception)) {
			zend_string_release_ex(class_name, 0);
			return 1;
		}

		/* The callback function may have defined the class */
		ce = zend_lookup_class(class_name);
		if (!ce) {
			php_error_docref(NULL, E_WARNING, "Function %s() hasn't defined the class it was called for", PG(unserialize_callback_func));
			incomplete_class = true;
			ce = PHP_IC_ENTRY;
		}
	} while (0);

	if (IGBINARY_IS_NOT_UNSERIALIZABLE(ce)) {
		zend_throw_exception_ex(NULL, 0, "Unserialization of '%s' is not allowed", ZSTR_VAL(ce->name));
		zend_string_release(class_name);
		return 1;
	}

	/* add this to the list of unserialized references, get the index */
	if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_object: end-of-data");
		zend_string_release(class_name);
		return 1;
	}

	{
		/* The actual value of ref is unused. We use ref_n later in this function, after creating the object. */
		struct igbinary_value_ref ref = {{0}, 0};
		ref_n = igsd_append_ref(igsd, ref);
		if (UNEXPECTED(ref_n == SIZE_MAX)) {
			zend_string_release(class_name);
			return 1;
		}
	}

	t = (enum igbinary_v3_type)igbinary_unserialize8(igsd);
	switch (t) {
		case igbinary_v3_type_array0:
		case igbinary_v3_type_array8:
		case igbinary_v3_type_array16:
		case igbinary_v3_type_array32:
		case igbinary_v3_type_list8:
		case igbinary_v3_type_list16:
		case igbinary_v3_type_list32:
		{
			if (UNEXPECTED(object_init_ex(z, ce) != SUCCESS)) {
				php_error_docref(NULL, E_NOTICE, "igbinary unable to create object for class entry");
				r = 1;
				break;
			}
			if (incomplete_class) {
#if PHP_VERSION_ID >= 80000
				php_store_class_name(z, class_name);
#else
				php_store_class_name(z, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
#endif
#if PHP_VERSION_ID >= 70400
			} else {
#if PHP_VERSION_ID >= 80000
				if (ce->__unserialize)
#else
				if (zend_hash_str_exists(&ce->function_table, "__unserialize", sizeof("__unserialize") - 1))
#endif
				{
					ZEND_ASSERT(Z_TYPE_P(z) == IS_OBJECT);
					struct igbinary_value_ref *ref = &IGB_REF_VAL_2(igsd, ref_n);
					if ((flags & WANT_REF) != 0) {
						ZVAL_MAKE_REF(z);
						ref->reference.reference = Z_REF_P(z);
						ref->type = IG_REF_IS_REFERENCE;
					} else {
						ref->reference.object = Z_OBJ_P(z);
						ref->type = IG_REF_IS_OBJECT;
					}
					/* Unserialize the array as an array for a deferred call to __unserialize */
					zval param;
					int result;
					zend_string_release(class_name);
					result = igbinary_unserialize_v3_array(igsd, t, &param, 0, false);
					ZVAL_DEREF(z);
					ZEND_ASSERT(Z_TYPE_P(z) == IS_OBJECT);
					// FIXME should this check result
					igsd_defer_unserialize(igsd, Z_OBJ_P(z), param);
					return result;
				}
#endif
			}
			struct igbinary_value_ref *ref = &IGB_REF_VAL_2(igsd, ref_n);
			if ((flags & WANT_REF) != 0) {
				ZVAL_MAKE_REF(z);
				ref->reference.reference = Z_REF_P(z);
				ref->type = IG_REF_IS_REFERENCE;
			} else {
				ref->reference.object = Z_OBJ_P(z);
				ref->type = IG_REF_IS_OBJECT;
			}

			r = igbinary_unserialize_v3_object_properties(igsd, t, z, ce);
			break;
		}
		case igbinary_v3_type_object_ser8:
		case igbinary_v3_type_object_ser16:
		case igbinary_v3_type_object_ser32:
		{
			is_from_serialized_data = true;
			/* FIXME will this break if z isn't an object? */
			r = igbinary_unserialize_v3_object_ser(igsd, t, z, ce);
			if (r != 0) {
				break;
			}

			if (incomplete_class) {
#if PHP_VERSION_ID >= 80000
				php_store_class_name(z, class_name);
#else
				php_store_class_name(z, ZSTR_VAL(class_name), ZSTR_LEN(class_name));
#endif
			}
			struct igbinary_value_ref *ref = &IGB_REF_VAL_2(igsd, ref_n);
			if ((flags & WANT_REF) != 0) {
				ZVAL_MAKE_REF(z);
				ref->reference.reference = Z_REF_P(z);
				ref->type = IG_REF_IS_REFERENCE;
			} else {
				ref->reference.object = Z_OBJ_P(z);
				ref->type = IG_REF_IS_OBJECT;
			}
			break;
		}
		case igbinary_v3_type_enum_case:
#if PHP_VERSION_ID >= 80100
			if (UNEXPECTED(incomplete_class)) {
				zend_error(E_WARNING, "igbinary_unserialize_v3_object_enum_case: Class '%s' does not exist", ZSTR_VAL(class_name));
				zend_string_release(class_name);
				return 1;
			}
			zend_string_release(class_name);
			r = igbinary_unserialize_v3_object_enum_case(igsd, z, ce);
			if (r) {
				return r;
			}

			struct igbinary_value_ref *ref = &IGB_REF_VAL_2(igsd, ref_n);
			if ((flags & WANT_REF) != 0) {
				ZVAL_MAKE_REF(z);
				ref->reference.reference = Z_REF_P(z);
				ref->type = IG_REF_IS_REFERENCE;
			} else {
				ref->reference.object = Z_OBJ_P(z);
				ref->type = IG_REF_IS_OBJECT;
			}
			return 0;
#else
			zend_error(E_WARNING, "igbinary_unserialize_v3_object: Cannot unserialize enum cases prior to php 8.1 at position %zu", (size_t)IGB_BUFFER_OFFSET(igsd));
			r = 1;
#endif
			break;
		default:
			zend_error(E_WARNING, "igbinary_unserialize_v3_object: unknown object inner type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
			r = 1;
	}
	zend_string_release(class_name);
	class_name = NULL;

	/* If unserialize was successful, defer the call to __wakeup if __wakeup exists for this object. */
	/* (But don't call __wakeup() if Serializable::unserialize was called */
	if (r == 0 && !is_from_serialized_data) {
		struct igbinary_value_ref *const ref = &IGB_REF_VAL_2(igsd, ref_n);
		zend_object *object;
		if (ref->type == IG_REF_IS_OBJECT) {
			object = ref->reference.object;
		} else if (EXPECTED(ref->type == IG_REF_IS_REFERENCE)) {
			/* May have created a reference while deserializing an object, if it was recursive. */
			zval ztemp = ref->reference.reference->val;
			if (UNEXPECTED(Z_TYPE(ztemp) != IS_OBJECT)) {
				zend_error(E_WARNING, "igbinary_unserialize_v3_object preparing to __wakeup/__unserialize: got reference to non-object somehow (inner type '%02x', position %zu)", t, (size_t)IGB_BUFFER_OFFSET(igsd));
				return 1;
			}
			object = Z_OBJ(ztemp);
		} else {
			zend_error(E_WARNING, "igbinary_unserialize_v3_object preparing to __wakeup/__unserialize: created non-object somehow (inner type '%02x', position %zu)", t, (size_t)IGB_BUFFER_OFFSET(igsd));
			return 1;
		}
		if (object->ce != PHP_IC_ENTRY) {
			if (zend_hash_str_exists(&object->ce->function_table, "__wakeup", sizeof("__wakeup") - 1)) {
				if (UNEXPECTED(igsd_defer_wakeup(igsd, object))) {
					return 1;
				}
			}
		}
	}

	return r;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_ref */
/** Unserializes an array or object by reference. */
zend_always_inline static int igbinary_unserialize_v3_ref(struct igbinary_unserialize_data *igsd, enum igbinary_v3_type t, zval *const z, int flags) {
	size_t n;

	if (t == igbinary_v3_type_ref8) {
		if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_ref: end-of-data");
			return 1;
		}
		n = igbinary_unserialize8(igsd);
	} else if (t == igbinary_v3_type_ref16) {
		if (IGB_NEEDS_MORE_DATA(igsd, 2)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_ref: end-of-data");
			return 1;
		}
		n = igbinary_unserialize16(igsd);
	} else if (t == igbinary_v3_type_ref32) {
		if (IGB_NEEDS_MORE_DATA(igsd, 4)) {
			zend_error(E_WARNING, "igbinary_unserialize_v3_ref: end-of-data");
			return 1;
		}
		n = igbinary_unserialize32(igsd);
	} else {
		zend_error(E_WARNING, "igbinary_unserialize_v3_ref: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
		return 1;
	}

	if (n >= igsd->references_count) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_ref: invalid reference %zu >= %zu", (size_t)n, (size_t)igsd->references_count);
		return 1;
	}

	if (z != NULL) {
		/* FIXME: check with is refcountable or some such */
		zval_ptr_dtor(z);
		ZVAL_UNDEF(z);
	}

	struct igbinary_value_ref *ref_ptr = &IGB_REF_VAL_2(igsd, n);
	struct igbinary_value_ref ref = *ref_ptr;

	/**
	 * Permanently convert the zval in IGB_REF_VAL() into a IS_REFERENCE if it wasn't already one.
	 * TODO: Can there properly be multiple reference groups to an object?
	 * Similar to https://github.com/php/php-src/blob/master/ext/standard/var_unserializer.re , for "R:"
	 * Using `flags` because igbinary_unserialize_v3_ref might be used both for copy on writes ($a = $b = [2]) and by PHP references($a = &$b).
	 */
	if ((flags & WANT_REF) != 0) {
		/* Want to create an IS_REFERENCE, not just to share the same value until modified. */
		switch (ref.type) {
		case IG_REF_IS_OBJECT:
			ZVAL_OBJ(z, ref.reference.object);
			Z_ADDREF_P(z);
			ZVAL_MAKE_REF(z); /* Convert original zval data to a reference */
			/* replace the entry in IGB_REF_VAL with a reference. */
			ref_ptr->reference.reference = Z_REF_P(z);
			ref_ptr->type = IG_REF_IS_REFERENCE;
			break;
		case IG_REF_IS_ARRAY:
			ZVAL_ARR(z, ref.reference.array);
			/* All arrays built by igbinary when unserializing are refcounted, except IG_REF_IS_EMPTY_ARRAY. */
			/* If they were not refcounted, the ZVAL_ARR call would probably also need to be changed. */
			Z_ADDREF_P(z);
			ZVAL_MAKE_REF(z); /* Convert original zval data to a reference */
			/* replace the entry in IGB_REF_VAL with a reference. */
			ref_ptr->reference.reference = Z_REF_P(z);
			ref_ptr->type = IG_REF_IS_REFERENCE;
			break;
#if PHP_VERSION_ID >= 70300
		case IG_REF_IS_EMPTY_ARRAY:
			ZVAL_EMPTY_ARRAY(z);
			ZVAL_MAKE_REF(z); /* Convert original zval data to a reference */
			/* replace the entry in IGB_REF_VAL with a reference. */
			ref_ptr->reference.reference = Z_REF_P(z);
			ref_ptr->type = IG_REF_IS_REFERENCE;
			break;
#endif
		case IG_REF_IS_REFERENCE:
			// This is already a reference, convert into reference count.
			ZVAL_REF(z, ref.reference.reference);
			Z_ADDREF_P(z);
			break;
		}
	} else {
		switch (ref.type) {
		case IG_REF_IS_OBJECT:
			ZVAL_OBJ(z, ref.reference.object);
			Z_ADDREF_P(z);
			break;
		case IG_REF_IS_ARRAY:
			ZVAL_ARR(z, ref.reference.array);
			Z_ADDREF_P(z);
			break;
#if PHP_VERSION_ID >= 70300
		case IG_REF_IS_EMPTY_ARRAY:
			ZVAL_EMPTY_ARRAY(z);
			break;
#endif
		case IG_REF_IS_REFERENCE:
			ZVAL_COPY(z, &(ref.reference.reference->val));
			break;
		}
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_unserialize_v3_zval */
/** Unserialize a zval of any serializable type (zval is PHP's internal representation of a value). */
static int igbinary_unserialize_v3_zval(struct igbinary_unserialize_data *igsd, zval *const z, int flags) {
	enum igbinary_v3_type t;

	zend_long tmp_long;
	double tmp_double;
	zend_string *tmp_str;

	if (IGB_NEEDS_MORE_DATA(igsd, 1)) {
		zend_error(E_WARNING, "igbinary_unserialize_v3_zval: end-of-data");
		return 1;
	}

	t = (enum igbinary_v3_type)igbinary_unserialize8(igsd);

	switch (t) {
		case igbinary_v3_type_ref:
			if (UNEXPECTED(igbinary_unserialize_v3_zval(igsd, z, WANT_REF))) {
				return 1;
			}

			/* If it is already a ref, nothing to do */
			if (Z_ISREF_P(z)) {
				break;
			}

			const zend_uchar type = Z_TYPE_P(z);
			/* Permanently convert the zval in IGB_REF_VAL() into a IS_REFERENCE if it wasn't already one. */
			/* TODO: Support multiple reference groups to the same object */
			/* Similar to https://github.com/php/php-src/blob/master/ext/standard/var_unserializer.re , for "R:" */
			ZVAL_MAKE_REF(z);
			switch (type) {
				case IS_STRING:
				case IS_LONG:
				case IS_NULL:
				case IS_DOUBLE:
				case IS_FALSE:
				case IS_TRUE:
				{
					struct igbinary_value_ref ref;
					ref.reference.reference = Z_REF_P(z);
					ref.type = IG_REF_IS_REFERENCE;
					/* add the unserialized scalar to the list of unserialized references. Objects and arrays were already added in igbinary_unserialize_v3_zval. */
					RETURN_1_IF_NON_ZERO(igsd_append_ref(igsd, ref) == SIZE_MAX);
					break;
				}
				default:
					break;
			}
			break;
		case igbinary_v3_type_ref8:
		case igbinary_v3_type_ref16:
		case igbinary_v3_type_ref32:
			if (UNEXPECTED(igbinary_unserialize_v3_ref(igsd, t, z, flags))) {
				return 1;
			}
			break;
		case igbinary_v3_type_object8:
		case igbinary_v3_type_object16:
		case igbinary_v3_type_object32:
		case igbinary_v3_type_object_id8:
		case igbinary_v3_type_object_id16:
		case igbinary_v3_type_object_id32:
			if (UNEXPECTED(igbinary_unserialize_v3_object(igsd, t, z, flags))) {
				return 1;
			}
			break;
		case igbinary_v3_type_array0:
		case igbinary_v3_type_array8:
		case igbinary_v3_type_array16:
		case igbinary_v3_type_array32:
		case igbinary_v3_type_list8:
		case igbinary_v3_type_list16:
		case igbinary_v3_type_list32:
			if (UNEXPECTED(igbinary_unserialize_v3_array(igsd, t, z, flags, true))) {
				return 1;
			}
			break;
		case igbinary_v3_type_string_empty:
			ZVAL_EMPTY_STRING(z);
			break;
		case igbinary_v3_type_string_char:
			ZVAL_INTERNED_STR(z, igbinary_unserialize_v3_char(igsd));
			break;
		case igbinary_v3_type_string_id8:
		case igbinary_v3_type_string_id16:
		case igbinary_v3_type_string_id32:
			tmp_str = igbinary_unserialize_v3_string(igsd, t);
			if (UNEXPECTED(tmp_str == NULL)) {
				return 1;
			}
			ZVAL_STR(z, tmp_str);
			break;
		case igbinary_v3_type_string8:
		case igbinary_v3_type_string16:
		case igbinary_v3_type_string32:
		case igbinary_v3_type_string64:
			tmp_str = igbinary_unserialize_v3_chararray(igsd, t, 0);
			if (UNEXPECTED(tmp_str == NULL)) {
				return 1;
			}
			ZVAL_STR(z, tmp_str);
			break;
		case igbinary_v3_type_long8p:
		case igbinary_v3_type_long16p:
		case igbinary_v3_type_long8n:
		case igbinary_v3_type_long16n:
		case igbinary_v3_type_long32p:
		case igbinary_v3_type_long32n:
		case igbinary_v3_type_long64signed:
			if (UNEXPECTED(igbinary_unserialize_v3_long(igsd, t, &tmp_long))) {
				return 1;
			}
			ZVAL_LONG(z, tmp_long);
			break;
#if IS_NULL == 1 && IS_FALSE == 2 && IS_TRUE == 3
		case igbinary_v3_type_null:
		case igbinary_v3_type_bool_false:
		case igbinary_v3_type_bool_true:
			Z_TYPE_INFO_P(z) = t;
			break;
#else
#warn Possibly unsupported php version, unrecognized values for types
		case igbinary_v3_type_null:
			ZVAL_NULL(z);
			break;
		case igbinary_v3_type_bool_false:
			ZVAL_BOOL(z, 0);
			break;
		case igbinary_v3_type_bool_true:
			ZVAL_BOOL(z, 1);
			break;
#endif
		case igbinary_v3_type_double:
			if (UNEXPECTED(igbinary_unserialize_v3_double(igsd, &tmp_double))) {
				return 1;
			}
			ZVAL_DOUBLE(z, tmp_double);
			break;
		default:
			zend_error(E_WARNING, "igbinary_unserialize_v3_zval: unknown type '%02x', position %zu", t, (size_t)IGB_BUFFER_OFFSET(igsd));
			return 1;
	}

	return 0;
}
/* }}} */

