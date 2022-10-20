/*
  +----------------------------------------------------------------------+
  | See COPYING file for further copyright information                   |
  +----------------------------------------------------------------------+
  | Author: Oleg Grenrus <oleg.grenrus@dynamoid.com>                     |
  | See CREDITS for contributors                                         |
  +----------------------------------------------------------------------+
*/

/*
 * Internal helper functions/macros. Do not include outside of igbinary.
 */

#ifndef IGBINARY_IMPL_H
#define IGBINARY_IMPL_H

/* {{{ Common macro/helper definitions */
#ifndef ZEND_UNREACHABLE
#if ZEND_DEBUG
# define ZEND_UNREACHABLE() do {ZEND_ASSERT(0); ZEND_ASSUME(0);} while (0)
#else
# define ZEND_UNREACHABLE() ZEND_ASSUME(0)
#endif
#endif

static zend_always_inline HashTable *HASH_OF_OBJECT(zval *p) {
	ZEND_ASSERT(Z_TYPE_P(p) == IS_OBJECT);
	return Z_OBJ_HT_P(p)->get_properties(
#if PHP_VERSION_ID >= 80000
			Z_OBJ_P(p)
#else
			p
#endif
	);
}

#if PHP_VERSION_ID < 70300
#define zend_string_release_ex(s, persistent) zend_string_release((s))

static zend_always_inline void zval_ptr_dtor_str(zval *zval_ptr)
{
	if (Z_REFCOUNTED_P(zval_ptr) && !Z_DELREF_P(zval_ptr)) {
		ZEND_ASSERT(Z_TYPE_P(zval_ptr) == IS_STRING);
		ZEND_ASSERT(!ZSTR_IS_INTERNED(Z_STR_P(zval_ptr)));
		ZEND_ASSERT(!(GC_FLAGS(Z_STR_P(zval_ptr)) & IS_STR_PERSISTENT));
		efree(Z_STR_P(zval_ptr));
	}
}
#endif

#define RETURN_1_IF_NON_ZERO(cmd) \
  if (UNEXPECTED((cmd) != 0)) {   \
    return 1;                     \
  }

#ifdef ZEND_ACC_NOT_SERIALIZABLE
# define IGBINARY_IS_NOT_SERIALIZABLE(ce) UNEXPECTED((ce)->ce_flags & (ZEND_ACC_NOT_SERIALIZABLE | ZEND_ACC_ANON_CLASS))
# define IGBINARY_IS_NOT_UNSERIALIZABLE(ce) IGBINARY_IS_NOT_SERIALIZABLE(ce)
#elif PHP_VERSION_ID >= 70400
# define IGBINARY_IS_NOT_SERIALIZABLE(ce) UNEXPECTED((ce)->serialize == zend_class_serialize_deny)
# define IGBINARY_IS_NOT_UNSERIALIZABLE(ce) UNEXPECTED((ce)->unserialize == zend_class_unserialize_deny)
#else
// Because '__serialize' is not available prior to 7.4, this check is redundant.
# define IGBINARY_IS_NOT_SERIALIZABLE(ce) (0)
# define IGBINARY_IS_NOT_UNSERIALIZABLE(ce) (0)
#endif
/* }}} */

/* Defers calls to zval_ptr_dtor for values that are refcounted. */
struct deferred_dtor_tracker {
	zval *zvals;           /**< refcounted objects and arrays to call dtor on after unserializing. See i_zval_ptr_dtor */
	size_t count;    /**< count of refcounted in array for calls to dtor */
	size_t capacity; /**< capacity of refcounted in array for calls to dtor */
};

/** Serializer data.
 * @author Oleg Grenrus <oleg.grenrus@dynamoid.com>
 */
struct igbinary_serialize_data {
	uint8_t *buffer;               /**< Buffer. */
	size_t buffer_size;            /**< Buffer size. */
	size_t buffer_capacity;        /**< Buffer capacity. */
	bool scalar;                   /**< Serializing scalar. */
	bool compact_strings;          /**< Check for duplicate strings. */
	struct hash_si strings;        /**< Hash of already serialized strings. */
	struct hash_si_ptr references; /**< Hash of already serialized potential references. (non-NULL uintptr_t => int32_t) */
	uint32_t references_id;        /**< Number of things that the unserializer might think are references. >= length of references */
	uint32_t string_count;         /**< Serialized string count, used for back referencing */

	struct deferred_dtor_tracker deferred_dtor_tracker;  /**< refcounted objects and arrays to call dtor on after serializing. See i_zval_ptr_dtor */
};

/*
Object {
   reference {scalar, object, array, null} (convert to reference, share reference in zval_ref)
   object {} (convert to zend_object, share zend_object* in reference)
   array {} (convert to zend_array, share zend_array* in reference)
   empty array {} (use ZVAL_EMPTY_ARRAY to create zvals)
}
*/
enum zval_ref_type {
	IG_REF_IS_REFERENCE,   // Points to zend_reference
	IG_REF_IS_OBJECT,      // Points to zend_object
	IG_REF_IS_ARRAY,       // Points to zend_array
#if PHP_VERSION_ID >= 70300
	IG_REF_IS_EMPTY_ARRAY, // Use the macro ZVAL_EMPTY_ARRAY to create a pointer to the empty array with the correct type info flags.
#endif
};

struct igbinary_value_ref {
	// We reuse temporary values for object properties that are references or arrays.
	union {
		zend_reference *reference;
		zend_object *object;
		zend_array *array;
	} reference;
	enum zval_ref_type type;
};

struct deferred_unserialize_call {
	zval param;          /* The array parameter passed to the __unserialize call */
	zend_object *object; /* The object which has a deferred call to __unserialize that is going to get called. */
};

struct deferred_call {
	union {
		zend_object *wakeup;
#if PHP_VERSION_ID >= 70400
		/* Currently, zvals are safe to relocate */
		struct deferred_unserialize_call unserialize;
#endif
	} data;

#if PHP_VERSION_ID >= 70400
	zend_bool is_unserialize;
#endif
};
/** Unserializer data.
 * @author Oleg Grenrus <oleg.grenrus@dynamoid.com>
 */
struct igbinary_unserialize_data {
	const uint8_t *buffer;          /**< Buffer with bytes to unserialize. */
	const uint8_t *buffer_end;      /**< Buffer size. */
	const uint8_t *buffer_ptr;      /**< Current read offset. */

	zend_string **strings;          /**< Unserialized strings. */
	size_t strings_count;           /**< Unserialized string count. */
	size_t strings_capacity;        /**< Unserialized string array capacity. */

	struct igbinary_value_ref *references; /**< Unserialized Arrays/Objects/References */
	size_t references_count;        /**< Unserialized array/objects count. */
	size_t references_capacity;     /**< Unserialized array/object array capacity. */

	struct deferred_call *deferred; /**< objects&data for calls to __unserialize/__wakeup */
	size_t deferred_capacity;     /**< capacity of objects in array for calls to __unserialize/__wakeup */
	uint32_t deferred_count;        /**< count of objects in array for calls to __unserialize/__wakeup. NOTE: Current php releases including 8.1 limit the total number of objects to a 32-bit integer. */
	zend_bool deferred_finished;    /**< whether the deferred calls were performed */
	struct deferred_dtor_tracker deferred_dtor_tracker;  /**< refcounted objects and arrays to call dtor on after unserializing. See i_zval_ptr_dtor */
};

#define IGB_REF_VAL_2(igsd, n)	((igsd)->references[(n)])
#define IGB_NEEDS_MORE_DATA(igsd, n)	UNEXPECTED((size_t)((igsd)->buffer_end - (igsd)->buffer_ptr) < (n))
#define IGB_REMAINING_BYTES(igsd)	((unsigned int)((igsd)->buffer_end - (igsd)->buffer_ptr))
#define IGB_BUFFER_OFFSET(igsd)	((unsigned int)((igsd)->buffer_ptr - (igsd)->buffer))

#define WANT_CLEAR (0)
#define WANT_REF   (1 << 1)

/* }}} */

/* {{{ Unserializing functions prototypes */
zend_always_inline static int igbinary_unserialize_data_init(struct igbinary_unserialize_data *igsd);
zend_always_inline static void igbinary_unserialize_data_deinit(struct igbinary_unserialize_data *igsd);

zend_always_inline static uint32_t igbinary_unserialize_header(struct igbinary_unserialize_data *igsd);

zend_always_inline static uint8_t igbinary_unserialize8(struct igbinary_unserialize_data *igsd);
zend_always_inline static uint16_t igbinary_unserialize16(struct igbinary_unserialize_data *igsd);
zend_always_inline static uint32_t igbinary_unserialize32(struct igbinary_unserialize_data *igsd);
zend_always_inline static uint64_t igbinary_unserialize64(struct igbinary_unserialize_data *igsd);

zend_never_inline zend_string* igbinary_unserialize_extremely_long_chararray(struct igbinary_unserialize_data *igsd);
static int igbinary_unserialize_v3_zval(struct igbinary_unserialize_data *igsd, zval *const z, int flags);
int igbinary_unserialize_v3_zval_wrapper(struct igbinary_unserialize_data *igsd, zval *const z, int flags);
/* }}} */

/* {{{ Serializing functions prototypes */
zend_always_inline static void igbinary_serialize_header(struct igbinary_serialize_data *igsd, const uint8_t version);

zend_always_inline static int igbinary_serialize8(struct igbinary_serialize_data *igsd, uint8_t i);
zend_always_inline static int igbinary_serialize16(struct igbinary_serialize_data *igsd, uint16_t i);
zend_always_inline static int igbinary_serialize32(struct igbinary_serialize_data *igsd, uint32_t i);
zend_always_inline static int igbinary_serialize64(struct igbinary_serialize_data *igsd, uint64_t i);
ZEND_COLD int igbinary_warn_serialize_resource(zval *z);

int igbinary_serialize_v3_array_wrapper(struct igbinary_serialize_data *igsd, zval *z, bool object, bool incomplete_class, bool serialize_props);
int igbinary_serialize_v3_zval_wrapper(struct igbinary_serialize_data *igsd, zval *z);
/* }}} */

/* {{{ igbinary_unserialize_data_init */
/** Inits igbinary_unserialize_data. */
inline static int igbinary_unserialize_data_init(struct igbinary_unserialize_data *igsd) {
	struct igbinary_value_ref *references = emalloc(sizeof(igsd->references[0]) * 4);
	zend_string **strings;
	if (UNEXPECTED(references == NULL)) {
		return 1;
	}
	strings = (zend_string **)emalloc(sizeof(zend_string *) * 4);
	if (UNEXPECTED(strings == NULL)) {
		/* We failed to allocate memory for strings. Fail and free everything we allocated */
		efree(references);
		return 1;
	}
	igsd->buffer = NULL;
	igsd->buffer_end = NULL;
	igsd->buffer_ptr = NULL;

	igsd->strings = NULL;
	igsd->strings_count = 0;
	igsd->strings_capacity = 4;

	igsd->references = references;
	igsd->references_count = 0;
	igsd->references_capacity = 4;

	igsd->strings = strings;

	/** Don't bother allocating zvals which __wakeup or __unserialize, probably not common */
	igsd->deferred = NULL;
	igsd->deferred_count = 0;
	igsd->deferred_capacity = 0;
	igsd->deferred_finished = 0;

	igsd->deferred_dtor_tracker.zvals = NULL;
	igsd->deferred_dtor_tracker.count = 0;
	igsd->deferred_dtor_tracker.capacity = 0;

	return 0;
}
/* }}} */
/* {{{ igsd_ensure_deferred_dtor_capacity(struct igbinary_serialize_data *igsd) */
static inline int igsd_ensure_deferred_dtor_capacity(struct deferred_dtor_tracker *tracker) {
	if (tracker->count >= tracker->capacity) {
		if (tracker->capacity == 0) {
			tracker->capacity = 2;
			tracker->zvals = emalloc(sizeof(tracker->zvals[0]) * tracker->capacity);
		} else {
			tracker->capacity *= 2;
			zval *old_deferred_dtor = tracker->zvals;
			tracker->zvals = erealloc(old_deferred_dtor, sizeof(tracker->zvals[0]) * tracker->capacity);
			if (UNEXPECTED(tracker->zvals == NULL)) {
				tracker->zvals = old_deferred_dtor;
				return 1;
			}
		}
	}
	return 0;
}
/* }}} */

/* {{{ free_deferred_dtors(struct deferred_dtor_tracker *tracker) */
static zend_always_inline void free_deferred_dtors(struct deferred_dtor_tracker *tracker) {
	zval *const zvals = tracker->zvals;
	if (zvals) {
		const size_t n = tracker->count;
		size_t i;
		for (i = 0; i < n; i++) {
			/* fprintf(stderr, "freeing i=%d id=%d refcount=%d\n", (int)i, (int)Z_OBJ_HANDLE(zvals[i]), (int)Z_REFCOUNT(zvals[i])); */
			zval_ptr_dtor(&zvals[i]);
		}
		efree(zvals);
	}
}
/* }}} */

/* {{{ igsd_addref_and_defer_dtor(struct igbinary_serialize_data *igsd, zval *z) */
static zend_always_inline int igsd_addref_and_defer_dtor(struct deferred_dtor_tracker *tracker, zval *z) {
	if (!Z_REFCOUNTED_P(z)) {
		return 0;
	}
	if (UNEXPECTED(igsd_ensure_deferred_dtor_capacity(tracker))) {
		return 1;
	}

	ZEND_ASSERT(Z_REFCOUNT_P(z) >= 1);  /* Expect that there were references at the time this was serialized */
	ZVAL_COPY(&tracker->zvals[tracker->count++], z);  /* Copy and increase reference count */
	return 0;
}
/* }}} */
/* {{{ igsd_defer_dtor(struct igbinary_serialize_data *igsd, zval *z) */
static inline int igsd_defer_dtor(struct deferred_dtor_tracker *tracker, zval *z) {
	if (!Z_REFCOUNTED_P(z)) {
		return 0;
	}
	if (igsd_ensure_deferred_dtor_capacity(tracker)) {
		return 1;
	}

	ZEND_ASSERT(Z_REFCOUNT_P(z) >= 1);  /* Expect that there were references at the time this was serialized */
	ZVAL_COPY_VALUE(&tracker->zvals[tracker->count++], z);  /* Copy without increasing reference count */
	return 0;
}
/* }}} */

/* {{{ igbinary_unserialize_data_deinit */
/** Deinits igbinary_unserialize_data. */
inline static void igbinary_unserialize_data_deinit(struct igbinary_unserialize_data *igsd) {
	if (igsd->strings) {
		size_t i;
		size_t strings_count = igsd->strings_count;
		zend_string **strings = igsd->strings;
		for (i = 0; i < strings_count; i++) {
			zend_string *s = strings[i];
#if ZEND_DEBUG
			ZEND_ASSERT(GC_REFCOUNT(s) >= 1);
#endif
			zend_string_release_ex(s, 0); /* Should only create interned or non-persistent strings when unserializing */
		}

		efree(strings);
	}

	if (igsd->references) {
		efree(igsd->references);
	}
	if (igsd->deferred) {
		struct deferred_call *calls = igsd->deferred;
#if PHP_VERSION_ID >= 70400
		uint32_t i;
		uint32_t n = igsd->deferred_count;
		for (i = 0; i < n; i++) {
			struct deferred_call *call = &calls[i];
			if (call->is_unserialize) {
				if (!igsd->deferred_finished) {
					struct deferred_unserialize_call *unserialize_call = &call->data.unserialize;
					GC_ADD_FLAGS(unserialize_call->object, IS_OBJ_DESTRUCTOR_CALLED);
					zval_ptr_dtor(&unserialize_call->param);
				}
			}
		}
#endif
		efree(calls);
	}
	free_deferred_dtors(&igsd->deferred_dtor_tracker);

	return;
}
/* }}} */
/* {{{ igbinary_unserialize_header_emit_warning */
/**
 * Warns about invalid byte headers
 * Precondition: igsd->buffer_size >= 4 */
static ZEND_COLD void igbinary_unserialize_header_emit_warning(struct igbinary_unserialize_data *igsd, int version) {
	int i;
	char buf[9], *it;
	for (i = 0; i < 4; i++) {
		if (!isprint((int)igsd->buffer[i])) {
			if (version != 0 && (((unsigned int)version) & 0xff000000) == (unsigned int)version) {
				// Check if high order byte was set instead of low order byte
				zend_error(E_WARNING, "igbinary_unserialize_header: unsupported version: %u, should be %u or %u or %u (wrong endianness?)", (unsigned int)version, 0x00000001, 0x00000002, (unsigned int)IGBINARY_FORMAT_VERSION);
				return;
			}
			// Binary data, or a version number from a future release.
			zend_error(E_WARNING, "igbinary_unserialize_header: unsupported version: %u, should be %u or %u or %u", (unsigned int)version, 0x00000001, 0x00000002, (unsigned int)IGBINARY_FORMAT_VERSION);
			return;
		}
	}

	/* To avoid confusion, if the first 4 bytes are all printable, print those instead of the integer representation to make debugging easier. */
	/* E.g. strings such as "a:2:" are emitted when an Array is serialized with serialize() instead of igbinary_serialize(), */
	/* and subsequently passed to igbinary_unserialize instead of unserialize(). */
	for (it = buf, i = 0; i < 4; i++) {
		char c = igsd->buffer[i];
		if (c == '"' || c == '\\') {
			*it++ = '\\';
		}
		*it++ = c;
	}
	*it = '\0';
	zend_error(E_WARNING, "igbinary_unserialize_header: unsupported version: \"%s\"..., should begin with a binary version header of \"\\x00\\x00\\x00\\x01\" or \"\\x00\\x00\\x00\\x02\" or \"\\x00\\x00\\x00\\x%02x\"", buf, (int)IGBINARY_FORMAT_VERSION);
}
/* }}} */
/* {{{ igbinary_unserialize_header */
/** Unserialize header. Check for version. */
inline static uint32_t igbinary_unserialize_header(struct igbinary_unserialize_data *igsd) {
	uint32_t version;

	if (IGB_NEEDS_MORE_DATA(igsd, 5)) {
		zend_error(E_WARNING, "igbinary_unserialize_header: expected at least 5 bytes of data, got %u byte(s)", IGB_REMAINING_BYTES(igsd));
		return 0;
	}

	version = igbinary_unserialize32(igsd);

	/* Support older version 1 and the current format 2 and the new format 3*/
	if (version >= 0x00000001 && version <= IGBINARY_FORMAT_VERSION) {
		return version;
	} else {
		igbinary_unserialize_header_emit_warning(igsd, version);
		return 0;
	}
}
/* }}} */
/* {{{ igbinary_unserialize8 */
/** Unserialize 8bit value. */
inline static uint8_t igbinary_unserialize8(struct igbinary_unserialize_data *igsd) {
	return *(igsd->buffer_ptr++);
}
/* }}} */
/* {{{ igbinary_unserialize16 */
/** Unserialize 16bit value. */
inline static uint16_t igbinary_unserialize16(struct igbinary_unserialize_data *igsd) {
	uint16_t ret =
	    ((uint16_t)(igsd->buffer_ptr[0]) << 8) |
	    ((uint16_t)(igsd->buffer_ptr[1]));
	igsd->buffer_ptr += 2;
	return ret;
}
/* }}} */
/* {{{ igbinary_unserialize32 */
/** Unserialize 32bit value. */
inline static uint32_t igbinary_unserialize32(struct igbinary_unserialize_data *igsd) {
	uint32_t ret =
	    ((uint32_t)(igsd->buffer_ptr[0]) << 24) |
	    ((uint32_t)(igsd->buffer_ptr[1]) << 16) |
	    ((uint32_t)(igsd->buffer_ptr[2]) << 8) |
	    ((uint32_t)(igsd->buffer_ptr[3]));
	igsd->buffer_ptr += 4;
	return ret;
}
/* }}} */
/* {{{ igbinary_unserialize64 */
/** Unserialize 64bit value. */
inline static uint64_t igbinary_unserialize64(struct igbinary_unserialize_data *igsd) {
	uint64_t ret =
	    ((uint64_t)((igsd->buffer_ptr[0])) << 56) |
	    ((uint64_t)((igsd->buffer_ptr[1])) << 48) |
	    ((uint64_t)((igsd->buffer_ptr[2])) << 40) |
	    ((uint64_t)((igsd->buffer_ptr[3])) << 32) |
	    ((uint64_t)((igsd->buffer_ptr[4])) << 24) |
	    ((uint64_t)((igsd->buffer_ptr[5])) << 16) |
	    ((uint64_t)((igsd->buffer_ptr[6])) << 8) |
	    ((uint64_t)((igsd->buffer_ptr[7])) << 0);
	igsd->buffer_ptr += 8;
	return ret;
}
/* }}} */
/* {{{ igbinary_raise_capacity */
static int igbinary_raise_capacity(struct igbinary_serialize_data *igsd, size_t size) {
	do {
		igsd->buffer_capacity *= 2;
	} while (igsd->buffer_size + size >= igsd->buffer_capacity);

	uint8_t *const old_buffer = igsd->buffer;
	igsd->buffer = erealloc(old_buffer, igsd->buffer_capacity);
	if (UNEXPECTED(igsd->buffer == NULL)) {
		/* We failed to allocate a larger buffer for the result. Free the memory used for the original buffer. */
		efree(old_buffer);
		return 1;
	}

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize_resize */
/** Expands igbinary_serialize_data if necessary. */
zend_always_inline static int igbinary_serialize_resize(struct igbinary_serialize_data *igsd, size_t size) {
	if (igsd->buffer_size + size < igsd->buffer_capacity) {
		return 0;
	}

	return igbinary_raise_capacity(igsd, size);
}
/* }}} */
/* {{{ igbinary_serialize8 */
/** Serialize 8bit value. */
zend_always_inline static int igbinary_serialize8(struct igbinary_serialize_data *igsd, uint8_t i) {
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 1));

	igsd->buffer[igsd->buffer_size++] = i;
	return 0;
}
/* }}} */
/* {{{ igbinary_serialize16 */
/** Serialize 16bit value. */
zend_always_inline static int igbinary_serialize16(struct igbinary_serialize_data *igsd, uint16_t i) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 2));

	append_buffer = &igsd->buffer[igsd->buffer_size];
	append_buffer[0] = (uint8_t)(i >> 8 & 0xff);
	append_buffer[1] = (uint8_t)(i & 0xff);
	igsd->buffer_size += 2;

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize32 */
/** Serialize 32bit value. */
zend_always_inline static int igbinary_serialize32(struct igbinary_serialize_data *igsd, uint32_t i) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 4));

	append_buffer = &igsd->buffer[igsd->buffer_size];
	append_buffer[0] = (uint8_t)(i >> 24 & 0xff);
	append_buffer[1] = (uint8_t)(i >> 16 & 0xff);
	append_buffer[2] = (uint8_t)(i >> 8 & 0xff);
	append_buffer[3] = (uint8_t)(i & 0xff);
	igsd->buffer_size += 4;

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize64 */
/** Serialize 64bit value. */
zend_always_inline static int igbinary_serialize64(struct igbinary_serialize_data *igsd, uint64_t i) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 8));

	append_buffer = &igsd->buffer[igsd->buffer_size];
	append_buffer[0] = (uint8_t)(i >> 56 & 0xff);
	append_buffer[1] = (uint8_t)(i >> 48 & 0xff);
	append_buffer[2] = (uint8_t)(i >> 40 & 0xff);
	append_buffer[3] = (uint8_t)(i >> 32 & 0xff);
	append_buffer[4] = (uint8_t)(i >> 24 & 0xff);
	append_buffer[5] = (uint8_t)(i >> 16 & 0xff);
	append_buffer[6] = (uint8_t)(i >> 8 & 0xff);
	append_buffer[7] = (uint8_t)(i & 0xff);
	igsd->buffer_size += 8;

	return 0;
}
/* }}} */
/* {{{ igbinary_serialize8_and_8 */
/** Serialize 8bit value + 8bit value. */
zend_always_inline static int igbinary_serialize8_and_8(struct igbinary_serialize_data *igsd, uint8_t i, uint8_t v) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 2));
	append_buffer = &igsd->buffer[igsd->buffer_size];

	append_buffer[0] = i;
	append_buffer[1] = v;
	igsd->buffer_size += 2;
	return 0;
}
/* }}} */
/* {{{ igbinary_serialize8_and_16 */
/** Serialize 8bit value + 16bit value. */
zend_always_inline static int igbinary_serialize8_and_16(struct igbinary_serialize_data *igsd, uint8_t i, uint16_t v) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 3));
	append_buffer = &igsd->buffer[igsd->buffer_size];

	append_buffer[0] = i;
	append_buffer[1] = (uint8_t)(v >> 8 & 0xff);
	append_buffer[2] = (uint8_t)(v & 0xff);
;
	igsd->buffer_size += 3;
	return 0;
}
/* }}} */
/* {{{ igbinary_serialize8_and_32 */
/** Serialize 8bit value + 32bit value. */
zend_always_inline static int igbinary_serialize8_and_32(struct igbinary_serialize_data *igsd, uint8_t i, uint32_t v) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 5));
	append_buffer = &igsd->buffer[igsd->buffer_size];

	append_buffer[0] = i;
	append_buffer[1] = (uint8_t)(v >> 24 & 0xff);
	append_buffer[2] = (uint8_t)(v >> 16 & 0xff);
	append_buffer[3] = (uint8_t)(v >> 8 & 0xff);
	append_buffer[4] = (uint8_t)(v & 0xff);
;
	igsd->buffer_size += 5;
	return 0;
}
/* }}} */
/* {{{ igbinary_serialize8_and_64 */
/** Serialize 8bit value + 64bit value. */
inline static int igbinary_serialize8_and_64(struct igbinary_serialize_data *igsd, uint8_t i, uint64_t v) {
	uint8_t *append_buffer;
	RETURN_1_IF_NON_ZERO(igbinary_serialize_resize(igsd, 9));
	append_buffer = &igsd->buffer[igsd->buffer_size];

	append_buffer[0] = i;
	append_buffer[1] = (uint8_t)(v >> 56 & 0xff);
	append_buffer[2] = (uint8_t)(v >> 48 & 0xff);
	append_buffer[3] = (uint8_t)(v >> 40 & 0xff);
	append_buffer[4] = (uint8_t)(v >> 32 & 0xff);
	append_buffer[5] = (uint8_t)(v >> 24 & 0xff);
	append_buffer[6] = (uint8_t)(v >> 16 & 0xff);
	append_buffer[7] = (uint8_t)(v >> 8 & 0xff);
	append_buffer[8] = (uint8_t)(v & 0xff);
;
	igsd->buffer_size += 9;
	return 0;
}
/* }}} */

/* igbinary_var_serialize_call_magic_serialize {{{ */
// Source: ext/standard/var.c from php-src
#if PHP_VERSION_ID >= 70400
inline static int igbinary_var_serialize_call_magic_serialize(zval *retval, zval *obj)
{
#if PHP_VERSION_ID >= 80000
	BG(serialize_lock)++;
	zend_call_known_instance_method_with_0_params(
		Z_OBJCE_P(obj)->__serialize, Z_OBJ_P(obj), retval);
	BG(serialize_lock)--;

	if (EG(exception)) {
		zval_ptr_dtor(retval);
		return 1;
	}
#else
	zval fname;
	int res;

	ZVAL_STRINGL(&fname, "__serialize", sizeof("__serialize") - 1);
	// XXX does this work with the standard serializer?
	BG(serialize_lock)++;
	res = call_user_function(CG(function_table), obj, &fname, retval, 0, 0);
	BG(serialize_lock)--;
	zval_ptr_dtor_str(&fname);

	if (res == FAILURE || Z_ISUNDEF_P(retval)) {
		zval_ptr_dtor(retval);
		return 1;
	}
#endif

	if (Z_TYPE_P(retval) != IS_ARRAY) {
		zval_ptr_dtor(retval);
		zend_type_error("%s::__serialize() must return an array", ZSTR_VAL(Z_OBJCE_P(obj)->name));
		return 1;
	}

	return 0;
}
#endif
/* }}} */

#endif
