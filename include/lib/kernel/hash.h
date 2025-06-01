#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H

/* 해시 테이블:
 *
 * 이 자료구조는 Pintos Project 3의 Tour of Pintos에서 자세히 설명되어 있습니다.
 *
 * 이 구조는 체이닝(chaining)을 사용하는 표준 해시 테이블입니다. 테이블에서 요소를 찾으려면,
 * 요소의 데이터에 대해 해시 함수를 계산하고, 그 값을 이중 연결 리스트 배열의 인덱스로 사용한 뒤,
 * 해당 리스트를 선형 탐색합니다.
 *
 * 체인 리스트는 동적 할당을 사용하지 않습니다. 대신, 해시에 들어갈 수 있는 각 구조체는
 * 반드시 struct hash_elem 멤버를 포함해야 합니다. 모든 해시 함수는 이 struct hash_elem을
 * 대상으로 동작합니다. hash_entry 매크로를 사용하면 struct hash_elem에서
 * 이를 포함하는 구조체 객체로 다시 변환할 수 있습니다. 이 기법은 연결 리스트 구현에서도
 * 사용됩니다. 자세한 설명은 lib/kernel/list.h를 참고하세요. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "list.h"

/* Hash element. */
struct hash_elem {
	struct list_elem list_elem;
};

/* Converts pointer to hash element HASH_ELEM into a pointer to
 * the structure that HASH_ELEM is embedded inside.  Supply the
 * name of the outer structure STRUCT and the member name MEMBER
 * of the hash element.  See the big comment at the top of the
 * file for an example. */
#define hash_entry(HASH_ELEM, STRUCT, MEMBER)                   \
	((STRUCT *) ((uint8_t *) &(HASH_ELEM)->list_elem        \
		- offsetof (STRUCT, MEMBER.list_elem)))

/* 보조 데이터 AUX가 주어졌을 때, 해시 요소 E의 해시 값을 계산하여 반환합니다. */
typedef uint64_t hash_hash_func (const struct hash_elem *e, void *aux);

/* 보조 데이터 AUX가 주어지면 두 해시 요소 A와 B의 값을 비교합니다.
 * A가 B보다 작으면 true를 반환하고,
 * A가 B보다 크거나 같으면 false를 반환합니다. */
typedef bool hash_less_func (const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux);

/* 보조 데이터 AUX가 주어지면 해시 요소 E에 대해 일부 연산을 수행합니다. */
typedef void hash_action_func (struct hash_elem *e, void *aux);

/* Hash table. */
struct hash {
	size_t elem_cnt;            /* Number of elements in table. */
	size_t bucket_cnt;          /* Number of buckets, a power of 2. */
	struct list *buckets;       /* Array of `bucket_cnt' lists. */
	hash_hash_func *hash;       /* Hash function. */
	hash_less_func *less;       /* Comparison function. */
	void *aux;                  /* Auxiliary data for `hash' and `less'. */
};

/* A hash table iterator. */
struct hash_iterator {
	struct hash *hash;          /* The hash table. */
	struct list *bucket;        /* Current bucket. */
	struct hash_elem *elem;     /* Current hash element in current bucket. */
};

/* Basic life cycle. */
bool hash_init (struct hash *, hash_hash_func *, hash_less_func *, void *aux);
void hash_clear (struct hash *, hash_action_func *);
void hash_destroy (struct hash *, hash_action_func *);

/* Search, insertion, deletion. */
struct hash_elem *hash_insert (struct hash *, struct hash_elem *);
struct hash_elem *hash_replace (struct hash *, struct hash_elem *);
struct hash_elem *hash_find (struct hash *, struct hash_elem *);
struct hash_elem *hash_delete (struct hash *, struct hash_elem *);

/* Iteration. */
void hash_apply (struct hash *, hash_action_func *);
void hash_first (struct hash_iterator *, struct hash *);
struct hash_elem *hash_next (struct hash_iterator *);
struct hash_elem *hash_cur (struct hash_iterator *);

/* Information. */
size_t hash_size (struct hash *);
bool hash_empty (struct hash *);

/* Sample hash functions. */
uint64_t hash_bytes (const void *, size_t);
uint64_t hash_string (const char *);
uint64_t hash_int (int);



#endif /* lib/kernel/hash.h */
