#include "HuaEngine/ECS/Runtime/OwnedValue.h"
#include "ECSLifecycleFixtures.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <type_traits>

#if defined(HUAENGINE_ASAN_REQUIRED) && !defined(__SANITIZE_ADDRESS__)
#error The standalone lifetime target must actually enable AddressSanitizer.
#endif

namespace {
	using namespace HE::Ecs;
	using namespace ECSLifecycleFixtures;

	void Require(bool condition, const std::string& message) {
		if (!condition) {
			std::cerr << "[ECSLifecycleSmoke] " << message << std::endl;
			std::exit(1);
		}
	}

	template<typename T>
	T Take(Result<T> result, const char* message) {
		if (!result.HasValue()) {
			Require(false, std::string(message) + ": " + result.GetError().Message);
		}
		return std::move(result).Value();
	}

	template<typename T>
	void ExpectError(const Result<T>& result, ErrorCode code, const char* message) {
		Require(!result.HasValue(), message);
		Require(result.GetError().Code == code, std::string(message) + " (unexpected error code)");
		Require(!result.GetError().Operation.empty(), "Expected operation context in the error");
		Require(!result.GetError().Message.empty(), "Expected a diagnostic for a rejected operation");
	}

	template<typename T>
	const RegisteredType& Register(TypeRegistry& registry, const char* name, bool tag = false) {
		const auto id = Take(registry.Register<T>(TypeGuid::FromName(name), name, tag), "Expected fixture registration");
		const auto* registered = registry.Find(id);
		Require(registered != nullptr, "Expected registration to publish a descriptor");
		Require(registered == registry.Find<T>(), "Expected native lookup to return the registered descriptor");
		Require(registered == registry.Find(TypeGuid::FromName(name)), "Expected Guid lookup to return the registered descriptor");
		Require(registered->Descriptor.Size == sizeof(T), "Expected actual component size");
		Require(registered->Descriptor.Alignment == alignof(T), "Expected actual component alignment");
		return *registered;
	}

	template<typename T>
	void RequireDestroyed() {
		Require(T::Live == 0, "Expected no remaining live fixture objects");
		Require(T::Constructed == T::Destroyed, "Expected every constructed fixture to be destroyed exactly once");
	}

	void VerifyRegistration() {
		const TypeGuid literal{0x1234567890abcdefULL, 0xfedcba0987654321ULL};
		Require(TypeGuid::FromString(ToString(literal)) == literal, "Expected stable Guid text round-trip");
		Require(TypeGuid::FromString("12345678-90AB-CDEF-FEDC-BA0987654321") == literal, "Expected canonical Guid parsing");
		Require(!TypeGuid::FromString("12345678-90AB-CDEF-FEDC-BA098765432x"), "Expected malformed Guid rejection");
		Require(!TypeGuid::FromString("1234"), "Expected short Guid rejection");

		TypeRegistry registry;
		const auto& first = Register<PlainValue>(registry, "Fixture.Plain");
		const auto* originalAddress = &first;
		const auto count = registry.All().size();
		Require(Take(registry.Register<PlainValue>(first.Descriptor.Guid, "Fixture.Plain"), "Expected idempotent registration") == first.Id,
			"Expected an identical descriptor to retain its local identity");
		Require(registry.All().size() == count, "Expected identical registration not to add another type");
		ExpectError(registry.Register<EmptyValue>(first.Descriptor.Guid, "Fixture.ConflictingGuid"), ErrorCode::DuplicateType,
			"Expected conflicting Guid rejection");
		const auto otherGuid = TypeGuid::FromName("Fixture.OtherGuid");
		ExpectError(registry.Register<EmptyValue>(otherGuid, "Fixture.Plain"), ErrorCode::DuplicateType,
			"Expected duplicate name rejection");
		ExpectError(registry.Register<PlainValue>(otherGuid, "Fixture.OtherNative"), ErrorCode::DuplicateType,
			"Expected a native type not to acquire a second Guid");
		Require(registry.All().size() == count && registry.Find(otherGuid) == nullptr,
			"Expected failed registration not to publish a partial entry");
		Require(registry.FindByName("Fixture.ConflictingGuid") == nullptr && registry.FindByName("Fixture.OtherNative") == nullptr,
			"Expected failed registration not to publish name aliases");
		ExpectError(registry.Register<EmptyValue>({}, "Fixture.EmptyGuid"), ErrorCode::InvalidType, "Expected missing Guid rejection");
		ExpectError(registry.Register<PlainValue>(otherGuid, "Fixture.InvalidTag", true), ErrorCode::InvalidType,
			"Expected nonempty components not to become zero-storage tags");
		ExpectError(registry.Register<ThrowingDestructor>(TypeGuid::FromName("Fixture.ThrowingDestructor"), "Fixture.ThrowingDestructor"),
			ErrorCode::InvalidType, "Expected throwing destruction to be rejected");
		for (int invalidCase = 0; invalidCase < 3; ++invalidCase) {
			auto descriptor = MakeTypeDescriptor<PlainValue>(TypeGuid::FromName("Fixture.InvalidLayout"), "Fixture.InvalidLayout");
			if (invalidCase == 0) descriptor.Size = 1;
			if (invalidCase == 1) descriptor.Alignment = 1;
			if (invalidCase == 2) descriptor.Storage = static_cast<StorageKind>(73);
			ExpectError(registry.Register(std::move(descriptor)), ErrorCode::InvalidType, "Expected malformed descriptor rejection");
			Require(registry.FindByName("Fixture.InvalidLayout") == nullptr && registry.All().size() == count,
				"Expected malformed descriptor rejection without partial registration");
		}
		auto copiedHandle = first;
		Require(!registry.Owns(copiedHandle), "Expected copied metadata not to become a registered ownership handle");
		ExpectError(OwnedValue::Construct<PlainValue>(copiedHandle), ErrorCode::InvalidType,
			"Expected owned construction to reject an unregistered metadata copy");

		// Distinct opaque native keys exercise growth without thousands of template instantiations.
		std::array<unsigned char, 4096> nativeKeys{};
		for (size_t index = 0; index < nativeKeys.size(); ++index) {
			auto descriptor = MakeTypeDescriptor<PlainValue>({0x21e90cce553547b1ULL, index + 1}, "Fixture.Dynamic." + std::to_string(index));
			descriptor.NativeKey = &nativeKeys[index];
			(void)Take(registry.Register(std::move(descriptor)), "Expected descriptor growth registration");
		}
		Require(registry.Find(first.Id) == originalAddress && registry.FindByName("Fixture.Plain") == originalAddress,
			"Expected descriptor addresses to remain stable after registry growth");
		Require(first.Descriptor.Guid == TypeGuid::FromName("Fixture.Plain"), "Expected original descriptor content to remain valid");

		bool rejectedOnWorker = false;
		std::thread worker([&] {
			const auto result = registry.Register<EmptyValue>(TypeGuid::FromName("Fixture.Worker"), "Fixture.Worker");
			rejectedOnWorker = !result.HasValue() && result.GetError().Code == ErrorCode::WrongThread;
		});
		worker.join();
		Require(rejectedOnWorker && registry.FindByName("Fixture.Worker") == nullptr, "Expected owner-thread-only registration");

		TypeRegistry reversed;
		const auto& otherFirst = Register<EmptyValue>(reversed, "Fixture.Empty");
		const auto& reversedPlain = Register<PlainValue>(reversed, "Fixture.Plain");
		Require(first.Id == otherFirst.Id && first.Descriptor.Guid != otherFirst.Descriptor.Guid,
			"Expected local TypeIds to be interpreted within their registry");
		Require(first.Id != reversedPlain.Id && first.Descriptor.Guid == reversedPlain.Descriptor.Guid,
			"Expected stable Guid identity across differing registration orders");
		Require(registry.Owns(first) && !reversed.Owns(first), "Expected foreign registered handles to be distinguishable");
		ExpectError(OwnedValue::Construct<PlainValue>(otherFirst), ErrorCode::InvalidType,
			"Expected a foreign local TypeId resolved as another native type to be rejected");
		Require(registry.Find<PlainValue>() == originalAddress, "Expected a second registry not to overwrite native lookup");
	}

	void VerifyOwnershipAndCapabilities() {
		TypeRegistry registry;
		const auto& nonPod = Register<NonPod>(registry, "Fixture.NonPod");
		const auto& moveOnly = Register<MoveOnly>(registry, "Fixture.MoveOnly");
		const auto& noDefault = Register<NoDefault>(registry, "Fixture.NoDefault");
		const auto& immovable = Register<Immovable>(registry, "Fixture.Immovable");
		const auto& throwingMove = Register<ThrowingMove>(registry, "Fixture.ThrowingMove");
		const auto& aligned = Register<OverAligned>(registry, "Fixture.OverAligned");
		const auto& indirectAligned = Register<IndirectAligned>(registry, "Fixture.IndirectAligned");
		const auto& tag = Register<ExplicitTag>(registry, "Fixture.Tag", true);
		const auto& empty = Register<EmptyValue>(registry, "Fixture.EmptyValue");
		Require(nonPod.Descriptor.Storage == StorageKind::Direct && !nonPod.Descriptor.TriviallyCopyable,
			"Expected a non-POD component with no-throw relocation to use direct storage");
		Require(nonPod.Descriptor.CanDefaultConstruct() && nonPod.Descriptor.CanCopy() && nonPod.Descriptor.CanRelocate(),
			"Expected non-POD lifecycle operations");
		Require(moveOnly.Descriptor.Storage == StorageKind::Direct && !moveOnly.Descriptor.CanCopy(),
			"Expected move-only components to remain usable without copy capability");
		Require(!noDefault.Descriptor.CanDefaultConstruct(), "Expected non-default construction capability to be explicit");
		Require(immovable.Descriptor.Storage == StorageKind::Indirect && !immovable.Descriptor.CanRelocate(),
			"Expected immovable components to use owning indirect storage");
		Require(throwingMove.Descriptor.Storage == StorageKind::Indirect && !throwingMove.Descriptor.CanRelocate(),
			"Expected potentially throwing relocation to use indirect storage");
		Require(immovable.Descriptor.SlotSize() == sizeof(void*) && immovable.Descriptor.SlotAlignment() == alignof(void*),
			"Expected indirect slots to have pointer storage size and alignment");
		Require(aligned.Descriptor.SlotSize() == sizeof(OverAligned) && aligned.Descriptor.SlotAlignment() == 128,
			"Expected direct slots to preserve over-alignment");
		Require(tag.Descriptor.Storage == StorageKind::Tag && tag.Descriptor.SlotSize() == 0,
			"Expected explicitly registered tags to have no column storage");
		Require(empty.Descriptor.Storage == StorageKind::Direct && empty.Descriptor.SlotSize() == sizeof(EmptyValue),
			"Expected empty ordinary components not to be inferred as tags");
		ExpectError(OwnedValue::Default(tag), ErrorCode::InvalidType, "Expected tags not to allocate owned component data");
		ExpectError(OwnedValue::Default(noDefault), ErrorCode::UnsupportedOperation, "Expected unavailable default construction to fail explicitly");

		{
			auto value = Take(OwnedValue::Construct<NonPod>(nonPod, 53), "Expected non-POD construction");
			auto* object = static_cast<NonPod*>(value.Data());
			Require(object->Text == "owned-component-53" && object->Values == std::vector<int>({53, 54, 55}), "Expected non-POD payload");
			const auto resource = std::weak_ptr<Resource>(object->Reference);
			auto clone = Take(value.Clone(), "Expected non-POD cloning");
			auto* copied = static_cast<NonPod*>(clone.Data());
			Require(copied != object && copied->Reference == object->Reference && object->Reference.use_count() == 2,
				"Expected copied components to own independent values and a shared resource reference");
			copied->Text = "changed copy";
			copied->Values[0] = -1;
			Require(object->Text == "owned-component-53" && object->Values[0] == 53, "Expected copied string/vector independence");
			auto* originalData = value.Data();
			OwnedValue moved(std::move(value));
			Require(!value && value.Type() == nullptr && moved.Data() == originalData, "Expected ownership transfer to empty the source");
			clone.Reset();
			Require(!resource.expired() && object->Reference.use_count() == 1, "Expected remaining component to retain the resource");
			moved.Reset();
			moved.Reset();
			Require(resource.expired(), "Expected final component destruction to release the resource");
		}
		{
			auto value = Take(OwnedValue::Construct<MoveOnly>(moveOnly, 59), "Expected move-only construction");
			ExpectError(value.Clone(), ErrorCode::UnsupportedOperation, "Expected cloning a move-only value to fail explicitly");
			Require(*static_cast<MoveOnly*>(value.Data())->Value == 59, "Expected failed clone to preserve the source");
			auto replacement = Take(OwnedValue::Default(moveOnly), "Expected move-only default construction");
			replacement = std::move(value);
			Require(!value && MoveOnly::Live == 1 && *static_cast<MoveOnly*>(replacement.Data())->Value == 59,
				"Expected move assignment to destroy the replaced owned value once");
		}
		{
			auto value = Take(OwnedValue::Construct<NoDefault>(noDefault, 61), "Expected explicit construction without a default constructor");
			Require(static_cast<NoDefault*>(value.Data())->Value == 61, "Expected custom constructor arguments to reach the component");
		}
		{
			auto value = Take(OwnedValue::Construct<Immovable>(immovable, 67), "Expected direct construction of an immovable object in owned storage");
			const void* originalAddress = value.Data();
			OwnedValue moved(std::move(value));
			Require(moved.Data() == originalAddress && static_cast<Immovable*>(moved.Data())->Value == 67,
				"Expected indirect ownership to move without moving the object");
			ExpectError(moved.Clone(), ErrorCode::UnsupportedOperation, "Expected unavailable immovable copying to fail explicitly");
		}
		{
			auto value = Take(OwnedValue::Construct<ThrowingMove>(throwingMove, 71), "Expected in-place construction despite a throwing move constructor");
			OwnedValue moved(std::move(value));
			Require(ThrowingMove::MoveAttempts == 0 && static_cast<ThrowingMove*>(moved.Data())->Value == 71,
				"Expected indirect ownership transfers never to invoke throwing component moves");
		}
		{
			auto value = Take(OwnedValue::Construct<OverAligned>(aligned, 79), "Expected over-aligned construction");
			auto clone = Take(value.Clone(), "Expected over-aligned copying");
			Require(reinterpret_cast<uintptr_t>(value.Data()) % 128 == 0 && reinterpret_cast<uintptr_t>(clone.Data()) % 128 == 0,
				"Expected correct alignment for original and copied objects");
			Require(static_cast<OverAligned*>(clone.Data())->Value == 79, "Expected over-aligned copied payload");
			void* released = value.Release();
			Require(!value && value.Type() == nullptr, "Expected release to transfer all ownership");
			aligned.Descriptor.Destroy(released);
			::operator delete(released, std::align_val_t(aligned.Descriptor.Alignment));
			auto indirect = Take(OwnedValue::Construct<IndirectAligned>(indirectAligned, 83), "Expected indirect over-aligned construction");
			Require(indirectAligned.Descriptor.Storage == StorageKind::Indirect && reinterpret_cast<uintptr_t>(indirect.Data()) % 256 == 0,
				"Expected indirect slots to preserve the underlying object's stronger alignment");
		}
		RequireDestroyed<NonPod>();
		RequireDestroyed<Resource>();
		RequireDestroyed<MoveOnly>();
		RequireDestroyed<NoDefault>();
		RequireDestroyed<Immovable>();
		RequireDestroyed<ThrowingMove>();
		RequireDestroyed<OverAligned>();
		RequireDestroyed<IndirectAligned>();
	}

	void VerifyRelocation() {
		TypeRegistry registry;
		const auto& type = Register<NonPod>(registry, "Fixture.RelocateNonPod");
		alignas(NonPod) std::byte source[sizeof(NonPod)];
		alignas(NonPod) std::byte destination[sizeof(NonPod)];
		type.Descriptor.DefaultConstruct(source);
		static_cast<NonPod*>(static_cast<void*>(source))->Text = "relocated value";
		type.Descriptor.Relocate(destination, source);
		auto* relocated = static_cast<NonPod*>(static_cast<void*>(destination));
		Require(NonPod::Live == 1 && relocated->Text == "relocated value" && relocated->Reference,
			"Expected relocation to preserve owned data and end the source lifetime");
		type.Descriptor.Destroy(destination);
		RequireDestroyed<NonPod>();
		RequireDestroyed<Resource>();

		const auto& copiedType = Register<CopyOnly>(registry, "Fixture.RelocateCopyOnly");
		Require(copiedType.Descriptor.Storage == StorageKind::Direct && copiedType.Descriptor.CanRelocate(),
			"Expected no-throw copying to support direct relocation when moving is unavailable");
		alignas(CopyOnly) std::byte copySource[sizeof(CopyOnly)];
		alignas(CopyOnly) std::byte copyDestination[sizeof(CopyOnly)];
		copiedType.Descriptor.DefaultConstruct(copySource);
		copiedType.Descriptor.Relocate(copyDestination, copySource);
		Require(CopyOnly::CopyAttempts == 1 && CopyOnly::Live == 1 && static_cast<CopyOnly*>(static_cast<void*>(copyDestination))->Value == 41,
			"Expected copy relocation to construct one replacement and end the source lifetime");
		copiedType.Descriptor.Destroy(copyDestination);
		RequireDestroyed<CopyOnly>();
	}

	void VerifyFailures() {
		TypeRegistry registry;
		const auto& defaultType = Register<ThrowingDefault>(registry, "Fixture.ThrowingDefault");
		const auto& copyType = Register<ThrowingCopy>(registry, "Fixture.ThrowingCopy");
		const auto& nonStandardType = Register<NonStandardThrow>(registry, "Fixture.NonStandardThrow");
		for (int attempt = 0; attempt < 128; ++attempt) {
			ExpectError(OwnedValue::Construct<NonStandardThrow>(nonStandardType, true), ErrorCode::ConstructionFailed,
				"Expected non-standard construction exceptions to become failures");
			Require(NonStandardThrow::Live == 0, "Expected non-standard exceptions to unwind constructed subobjects");
		}
		{
			auto existing = Take(OwnedValue::Default(defaultType), "Expected successful construction before injected failure");
			const void* oldAddress = existing.Data();
			ThrowingDefault::Fail = true;
			for (int attempt = 0; attempt < 128; ++attempt) {
				ExpectError(OwnedValue::Default(defaultType), ErrorCode::ConstructionFailed, "Expected constructor failure to be returned");
				Require(ThrowingDefault::Live == 1, "Expected constructor unwinding to leave only the existing object");
			}
			ThrowingDefault::Fail = false;
			Require(existing.Data() == oldAddress && static_cast<ThrowingDefault*>(existing.Data())->Value == 29,
				"Expected constructor failures not to alter an existing value");
		}
		{
			auto source = Take(OwnedValue::Construct<ThrowingCopy>(copyType, "source payload"), "Expected copy source construction");
			auto destination = Take(OwnedValue::Construct<ThrowingCopy>(copyType, "preserved payload"), "Expected replacement destination construction");
			const void* oldAddress = destination.Data();
			ThrowingCopy::Fail = true;
			for (int attempt = 0; attempt < 128; ++attempt) {
				auto prepared = source.Clone();
				ExpectError(prepared, ErrorCode::ConstructionFailed, "Expected copy failure to be returned");
				Require(destination.Data() == oldAddress && static_cast<ThrowingCopy*>(destination.Data())->Text == "preserved payload" && ThrowingCopy::Live == 2,
					"Expected failed replacement preparation to preserve both existing values");
			}
			ThrowingCopy::Fail = false;
			destination = Take(source.Clone(), "Expected successful replacement after failure recovery");
			Require(static_cast<ThrowingCopy*>(destination.Data())->Text == "source payload" && ThrowingCopy::Live == 2,
				"Expected successful replacement to release its previous value once");
			ExpectError(OwnedValue::Copy(copyType, nullptr), ErrorCode::InvalidArgument, "Expected null copying source rejection");
		}
		OwnedValue empty;
		ExpectError(empty.Clone(), ErrorCode::InvalidState, "Expected empty value cloning to fail explicitly");
		RequireDestroyed<ThrowingDefault>();
		RequireDestroyed<ThrowingCopy>();
		RequireDestroyed<NonStandardThrow>();
	}
}

int main() {
	VerifyRegistration();
	VerifyOwnershipAndCapabilities();
	VerifyRelocation();
	VerifyFailures();
#if defined(__SANITIZE_ADDRESS__)
	std::cout << "AddressSanitizer instrumentation is enabled" << std::endl;
#endif
	std::cout << "ECSLifecycleSmoke passed" << std::endl;
	return 0;
}
