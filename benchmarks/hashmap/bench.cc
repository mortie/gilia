/*
 * Benchmark Results:
 *
 * Big sequential insert:
 *     L2 namespace:  0.177321
 *     Absl flat map: 1.16507
 *     Absl node map: 2.76126
 *     unordered map: 1.63138
 *     map:           2.16513
 * Big sequential lookup:
 *     L2 namespace:  0.15393
 *     Absl flat map: 2.2001
 *     Absl node map: 2.5494
 *     unordered map: 0.737267
 *     map:           8.69381
 * Small rand insert:
 *     L2 namespace:  0.103471
 *     Absl flat map: 0.401839
 *     Absl node map: 0.686433
 *     unordered map: 0.422422
 *     map:           0.31541
 * Small rand lookup:
 *     L2 namespace:  0.19638
 *     Absl flat map: 0.506841
 *     Absl node map: 0.502917
 *     unordered map: 0.738461
 *     map:           0.471924
 */

#include <unordered_map>
#include <map>
#include <absl/container/flat_hash_map.h>
#include <absl/container/node_hash_map.h>
#include <iostream>

extern "C" {
#include <lang2/vm/vm.h>
#include <time.h>
}

double getTime() {
	struct timespec tv;
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return tv.tv_sec + tv.tv_nsec / 1000000000.0;
}

struct L2Spec {
	static struct l2_vm_value alloc() {
		return {
			.flags = l2_value_type::L2_VAL_TYPE_NAMESPACE,
			.ns = nullptr,
		};
	}

	static void free(struct l2_vm_value &ns) {
		::free(ns.ns);
	}

	static void insert(struct l2_vm_value &ns, l2_word key, l2_word val) {
		l2_vm_namespace_set(&ns, key, val);
	}

	static l2_word lookup(struct l2_vm_value &ns, l2_word key) {
		return l2_vm_namespace_get(NULL, &ns, key);
	}
};

template<typename T>
struct CppSpec {
	static T alloc() {
		return {};
	}

	static void free(T &) {}

	static l2_word lookup(T &map, l2_word key) {
		auto it = map.find(key);
		if (it == map.end()) {
			return 0;
		} else {
			return it->second;
		}
	}

	static void insert(T &map, l2_word key, l2_word val) {
		map[key] = val;
	}
};
using UnorderedMapSpec = CppSpec<std::unordered_map<l2_word, l2_word>>;
using MapSpec = CppSpec<std::map<l2_word, l2_word>>;
using AbslFlatSpec = CppSpec<absl::flat_hash_map<l2_word, l2_word>>;
using AbslNodeSpec = CppSpec<absl::node_hash_map<l2_word, l2_word>>;

template<typename Spec>
double testBigSeqInsert() {
	auto map = Spec::alloc();

	double start = getTime();
	for (int i = 1; i < 10000000; ++i) {
		Spec::insert(map, i, i * 10);
	}
	double t = getTime() - start;

	Spec::free(map);
	return t;
}

template<typename Spec>
double testBigSeqLookup() {
	auto map = Spec::alloc();
	for (int i = 1; i < 1000000; ++i) {
		Spec::insert(map, i, i * 10);
	}

	volatile l2_word sum = 0;
	double start = getTime();
	for (int i = 0; i < 100; ++i) {
		for (int j = 1; j < 1000000; ++j) {
			sum += Spec::lookup(map, j);
		}
	}
	double t = getTime() - start;

	Spec::free(map);
	return t;
}

template<typename Spec>
double testSmallRandInsert() {
	srand(0);
	l2_word rands[100];
	for (int i = 0; i < 100; ++i) {
		rands[i] = rand();
	}

	double start = getTime();
	for (int i = 0; i < 100000; ++i) {
		auto map = Spec::alloc();
		double start = getTime();
		for (int j = 0; j < 100; ++j) {
			Spec::insert(map, rands[j], j + 1);
		}
		Spec::free(map);
	}

	return getTime() - start;;
}

template<typename Spec>
double testSmallRandLookup() {
	srand(0);
	l2_word rands[100];
	for (int i = 0; i < 100; ++i) {
		rands[i] = rand() % 100;
	}

	auto map = Spec::alloc();
	for (int j = 0; j < 100; ++j) {
		Spec::insert(map, rands[j], j + 1);
	}

	volatile l2_word sum = 0;
	double start = getTime();
	for (int j = 0; j < 100000000; ++j) {
		sum += Spec::lookup(map, rands[j % 100]);
	}
	double t = getTime() - start;

	Spec::free(map);
	return t;
}

int main() {
	std::cout << "Big sequential insert:\n";
	std::cout << "\tL2 namespace:  " << testBigSeqInsert<L2Spec>() << '\n';
	std::cout << "\tAbsl flat map: " << testBigSeqInsert<AbslFlatSpec>() << '\n';
	std::cout << "\tAbsl node map: " << testBigSeqInsert<AbslNodeSpec>() << '\n';
	std::cout << "\tunordered map: " << testBigSeqInsert<UnorderedMapSpec>() << '\n';
	std::cout << "\tmap:           " << testBigSeqInsert<MapSpec>() << '\n';

	std::cout << "Big sequential lookup:\n";
	std::cout << "\tL2 namespace:  " << testBigSeqLookup<L2Spec>() << '\n';
	std::cout << "\tAbsl flat map: " << testBigSeqLookup<AbslFlatSpec>() << '\n';
	std::cout << "\tAbsl node map: " << testBigSeqLookup<AbslNodeSpec>() << '\n';
	std::cout << "\tunordered map: " << testBigSeqLookup<UnorderedMapSpec>() << '\n';
	std::cout << "\tmap:           " << testBigSeqLookup<MapSpec>() << '\n';

	std::cout << "Small rand insert:\n";
	std::cout << "\tL2 namespace:  " << testSmallRandInsert<L2Spec>() << '\n';
	std::cout << "\tAbsl flat map: " << testSmallRandInsert<AbslFlatSpec>() << '\n';
	std::cout << "\tAbsl node map: " << testSmallRandInsert<AbslNodeSpec>() << '\n';
	std::cout << "\tunordered map: " << testSmallRandInsert<UnorderedMapSpec>() << '\n';
	std::cout << "\tmap:           " << testSmallRandInsert<MapSpec>() << '\n';

	std::cout << "Small rand lookup:\n";
	std::cout << "\tL2 namespace:  " << testSmallRandLookup<L2Spec>() << '\n';
	std::cout << "\tAbsl flat map: " << testSmallRandLookup<AbslFlatSpec>() << '\n';
	std::cout << "\tAbsl node map: " << testSmallRandLookup<AbslNodeSpec>() << '\n';
	std::cout << "\tunordered map: " << testSmallRandLookup<UnorderedMapSpec>() << '\n';
	std::cout << "\tmap:           " << testSmallRandLookup<MapSpec>() << '\n';
}
