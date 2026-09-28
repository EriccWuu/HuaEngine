#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ECSLifecycleFixtures {

template<typename Tag>
struct Lifetime {
	inline static int Live = 0;
	inline static int Constructed = 0;
	inline static int Destroyed = 0;

	Lifetime() noexcept { ++Live; ++Constructed; }
	Lifetime(const Lifetime&) noexcept : Lifetime() {}
	Lifetime(Lifetime&&) noexcept : Lifetime() {}
	Lifetime& operator=(const Lifetime&) = default;
	Lifetime& operator=(Lifetime&&) = default;
	~Lifetime() noexcept { --Live; ++Destroyed; }
};

struct Resource : Lifetime<Resource> {
	int Value = 73;
};

struct NonPod : Lifetime<NonPod> {
	std::string Text;
	std::vector<int> Values;
	std::shared_ptr<Resource> Reference;

	explicit NonPod(int value = 7)
		: Text("owned-component-" + std::to_string(value)), Values{value, value + 1, value + 2},
		  Reference(std::make_shared<Resource>()) {}
	NonPod(const NonPod&) = default;
	NonPod(NonPod&&) noexcept = default;
	NonPod& operator=(const NonPod&) = default;
	NonPod& operator=(NonPod&&) noexcept = default;
};

struct MoveOnly : Lifetime<MoveOnly> {
	std::unique_ptr<int> Value;

	explicit MoveOnly(int value = 11) : Value(std::make_unique<int>(value)) {}
	MoveOnly(const MoveOnly&) = delete;
	MoveOnly(MoveOnly&&) noexcept = default;
	MoveOnly& operator=(const MoveOnly&) = delete;
	MoveOnly& operator=(MoveOnly&&) noexcept = default;
};

struct NoDefault : Lifetime<NoDefault> {
	int Value;

	NoDefault() = delete;
	explicit NoDefault(int value) : Value(value) {}
	NoDefault(const NoDefault&) = default;
	NoDefault(NoDefault&&) noexcept = default;
};

struct Immovable : Lifetime<Immovable> {
	int Value;

	explicit Immovable(int value = 19) : Value(value) {}
	Immovable(const Immovable&) = delete;
	Immovable(Immovable&&) = delete;
	Immovable& operator=(const Immovable&) = delete;
	Immovable& operator=(Immovable&&) = delete;
};

struct ThrowingMove : Lifetime<ThrowingMove> {
	inline static int MoveAttempts = 0;
	int Value;

	explicit ThrowingMove(int value = 23) : Value(value) {}
	ThrowingMove(const ThrowingMove&) = delete;
	ThrowingMove(ThrowingMove&& source) : Value(source.Value) {
		++MoveAttempts;
		throw std::runtime_error("injected move failure");
	}
};

struct ThrowingDefault : Lifetime<ThrowingDefault> {
	inline static bool Fail = false;
	int Value = 29;

	ThrowingDefault() {
		if (Fail) throw std::runtime_error("injected default construction failure");
	}
	ThrowingDefault(const ThrowingDefault&) = default;
	ThrowingDefault(ThrowingDefault&&) noexcept = default;
};

struct ThrowingCopy : Lifetime<ThrowingCopy> {
	inline static bool Fail = false;
	inline static int CopyAttempts = 0;
	std::string Text;

	explicit ThrowingCopy(std::string text = "existing value") : Text(std::move(text)) {}
	ThrowingCopy(const ThrowingCopy& source) : Text(source.Text) {
		++CopyAttempts;
		if (Fail) throw std::runtime_error("injected copy failure");
	}
	ThrowingCopy(ThrowingCopy&&) noexcept = default;
};

struct alignas(128) OverAligned : Lifetime<OverAligned> {
	int Value;

	explicit OverAligned(int value = 31) : Value(value) {}
	OverAligned(const OverAligned&) = default;
	OverAligned(OverAligned&&) noexcept = default;
};

struct CopyOnly : Lifetime<CopyOnly> {
	inline static int CopyAttempts = 0;
	int Value = 41;

	CopyOnly() = default;
	CopyOnly(const CopyOnly& source) noexcept : Value(source.Value) { ++CopyAttempts; }
	CopyOnly(CopyOnly&&) = delete;
};

struct alignas(256) IndirectAligned : Lifetime<IndirectAligned> {
	int Value;

	explicit IndirectAligned(int value = 43) : Value(value) {}
	IndirectAligned(const IndirectAligned&) = delete;
	IndirectAligned(IndirectAligned&&) = delete;
};

struct ThrowingDestructor {
	~ThrowingDestructor() noexcept(false) {}
};

struct NonStandardThrow : Lifetime<NonStandardThrow> {
	explicit NonStandardThrow(bool fail) {
		if (fail) throw 17;
	}
};

struct ExplicitTag {};
struct EmptyValue {};
struct PlainValue { int Value = 37; };

}
