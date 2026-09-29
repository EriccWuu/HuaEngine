#include "HuaEngine/Serialization/Serialization.h"

#include "Fixtures/PlainReflection.h"

#include <FixtureEnums/GeneratedReflection.h>
#include <PlainReflection/GeneratedReflection.h>

#include "HuaEngine/Reflection/ReflectionRegistry.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace {
    void Require(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "[MetaReflectionOnlySmoke] " << message << '\n';
            std::exit(1);
        }
    }
}

int main() {
    HE::Refl::Registry first;
    HE::Refl::Registry second;
    Require(!HE::Generated::PlainReflection::RegisterReflection(first), "Expected standalone reflection registration");
    Require(!HE::Generated::PlainReflection::RegisterReflection(second), "Expected registration in another reflection registry");
    Require(!HE::Generated::FixtureEnums::RegisterReflection(first), "Expected repeated cross-module enum registration");

    const auto* firstType = first.Find<P6Fixture::IndependentValue>();
    const auto* secondType = second.Find<P6Fixture::IndependentValue>();
    const auto* firstEnum = first.FindEnum("P6Fixture::SharedMode");
    const auto* secondEnum = second.FindEnum("P6Fixture::SharedMode");
    Require(firstType && secondType && firstEnum && secondEnum, "Expected non-ECS type and enum metadata");
    Require(firstType != secondType && firstEnum != secondEnum, "Expected registry-owned reflection identities");
    Require(firstType->Fields.size() == 2, "Expected generated fields for an ordinary reflected type");
	const auto expectedTypeGuid = HE::Refl::TypeGuid::FromString("6e64049af5f0431ba7d734c4c0000011");
	const auto expectedEnumGuid = HE::Refl::TypeGuid::FromString("6e64049af5f0431ba7d734c4c0000012");
	Require(firstType->Guid == expectedTypeGuid && secondType->Guid == expectedTypeGuid &&
		first.FindType(expectedTypeGuid) == firstType,
		"Expected ordinary reflected types to keep their declared Guid without ECS");
	Require(firstEnum->Guid == expectedEnumGuid && first.FindEnum(expectedEnumGuid) == firstEnum,
		"Expected enum metadata to keep its declared Guid without ECS");
	const auto* idle = HE::Refl::FindRuntimeEnumValueByName(*firstEnum, "Idle");
	const auto* idleLabel = idle ? HE::Refl::FindRuntimeAttribute(idle->Attributes, "DisplayName") : nullptr;
	Require(idleLabel && idleLabel->Value == "Idle state" && idle->DisplayName == "Idle state",
		"Expected enum value attributes to remain available after registration");

    const auto& modeField = firstType->Fields[1];
    Require(modeField.EnumType == firstEnum, "Expected the field enum to belong to its reflection registry");
    P6Fixture::IndependentValue value;
    Require(HE::Refl::SetRuntimeEnumFieldValueByName(modeField, &value, "Active") &&
        value.Mode == P6Fixture::SharedMode::Active,
        "Expected ordinary reflected field editing without ECS");
    Require(!HE::Refl::SetRuntimeEnumFieldValueByName(modeField, &value, "Unknown") &&
        value.Mode == P6Fixture::SharedMode::Active,
        "Invalid enum names must preserve the ordinary reflected value");

	HE::Refl::Registry manual;
	const auto manualGuid = HE::Refl::TypeGuid::FromString("6e64049af5f0431ba7d734c4c0000013");
	const auto manualEnumGuid = HE::Refl::TypeGuid::FromString("6e64049af5f0431ba7d734c4c0000014");
	{
		std::string name = "Transient";
		std::string qualifiedName = "Test::Transient";
		std::string flag = "ScriptVisible";
		std::string attributeName = "Editor.Hint";
		std::string attributeValue = "Owned metadata";
		std::string enumName = "TransientEnum";
		std::string enumQualifiedName = "Test::TransientEnum";
		std::string enumValueName = "First";
		std::string enumValueFlag = "ScriptVisible";
		std::string enumValueAttributeName = "Inspector.Label";
		std::string enumValueAttributeValue = "Owned enum value";
		std::array<std::string_view, 1> enumValueFlags{enumValueFlag};
		std::array<HE::Refl::RuntimeAttribute, 1> enumValueAttributes{{{enumValueAttributeName, enumValueAttributeValue}}};
		std::array<HE::Refl::RuntimeEnumValueDescriptor, 1> enumValues{{{enumValueName, 1, "", enumValueFlags, enumValueAttributes}}};
		HE::Refl::RuntimeEnumDescriptor enumDescriptor{};
		enumDescriptor.Name = enumName;
		enumDescriptor.QualifiedName = enumQualifiedName;
		enumDescriptor.UnderlyingType = "int";
		enumDescriptor.Values = enumValues;
		enumDescriptor.Guid = manualEnumGuid;
		Require(!manual.RegisterEnum(enumDescriptor), "Expected manual enum registration");
		std::string fieldName = "Mode";
		std::string fieldEnumName = enumQualifiedName;
		std::string fieldFlag = "ScriptVisible";
		std::string fieldAttributeName = "Serialization.Alias";
		std::string fieldAttributeValue = "PreviousMode";
		std::array<std::string_view, 1> fieldFlags{fieldFlag};
		std::array<HE::Refl::RuntimeAttribute, 1> fieldAttributes{{{fieldAttributeName, fieldAttributeValue}}};
		std::array<HE::Refl::RuntimeFieldDescriptor, 1> fields{};
		fields[0].Name = fieldName;
		fields[0].Type = "int";
		fields[0].Size = sizeof(int);
		fields[0].EnumQualifiedName = fieldEnumName;
		fields[0].MetadataFlags = fieldFlags;
		fields[0].Attributes = fieldAttributes;
		std::array<std::string_view, 1> flags{flag};
		std::array<HE::Refl::RuntimeAttribute, 1> attributes{{{attributeName, attributeValue}}};
		HE::Refl::RuntimeTypeDescriptor descriptor{};
		descriptor.Name = name;
		descriptor.QualifiedName = qualifiedName;
		descriptor.Kind = "type";
		descriptor.Size = sizeof(int);
		descriptor.Guid = manualGuid;
		descriptor.Fields = fields;
		descriptor.Flags = flags;
		descriptor.Attributes = attributes;
		Require(!manual.RegisterType(descriptor), "Expected manual reflection registration");
		name.assign(name.size(), 'x');
		qualifiedName.assign(qualifiedName.size(), 'x');
		flag.assign(flag.size(), 'x');
		attributeName.assign(attributeName.size(), 'x');
		attributeValue.assign(attributeValue.size(), 'x');
		enumName.assign(enumName.size(), 'x');
		enumQualifiedName.assign(enumQualifiedName.size(), 'x');
		enumValueName.assign(enumValueName.size(), 'x');
		enumValueFlag.assign(enumValueFlag.size(), 'x');
		enumValueAttributeName.assign(enumValueAttributeName.size(), 'x');
		enumValueAttributeValue.assign(enumValueAttributeValue.size(), 'x');
		fieldName.assign(fieldName.size(), 'x');
		fieldEnumName.assign(fieldEnumName.size(), 'x');
		fieldFlag.assign(fieldFlag.size(), 'x');
		fieldAttributeName.assign(fieldAttributeName.size(), 'x');
		fieldAttributeValue.assign(fieldAttributeValue.size(), 'x');
	}
	const auto* owned = manual.FindType(manualGuid);
	const auto* hint = owned ? HE::Refl::FindRuntimeAttribute(owned->Attributes, "Editor.Hint") : nullptr;
	Require(owned && owned->QualifiedName == "Test::Transient" &&
		HE::Refl::HasRuntimeFlag(owned->Flags, "ScriptVisible") &&
		hint && hint->Value == "Owned metadata",
		"Expected a registry to own copied metadata and retain its stable Guid");
	const auto* ownedEnum = manual.FindEnum(manualEnumGuid);
	const auto* ownedValue = ownedEnum ? HE::Refl::FindRuntimeEnumValueByName(*ownedEnum, "First") : nullptr;
	const auto* ownedValueLabel = ownedValue ? HE::Refl::FindRuntimeAttribute(ownedValue->Attributes, "Inspector.Label") : nullptr;
	Require(ownedEnum && ownedEnum->QualifiedName == "Test::TransientEnum" && ownedValue &&
		HE::Refl::HasRuntimeFlag(ownedValue->Flags, "ScriptVisible") &&
		ownedValueLabel && ownedValueLabel->Value == "Owned enum value",
		"Expected copied enum value metadata to outlive its source");
	const auto* fieldAlias = owned && !owned->Fields.empty()
		? HE::Refl::FindRuntimeAttribute(owned->Fields[0].Attributes, "Serialization.Alias") : nullptr;
	Require(owned && owned->Fields.size() == 1 && owned->Fields[0].Name == "Mode" &&
		owned->Fields[0].EnumQualifiedName == "Test::TransientEnum" && owned->Fields[0].EnumType == ownedEnum &&
		HE::Refl::HasRuntimeFlag(owned->Fields[0].MetadataFlags, "ScriptVisible") &&
		fieldAlias && fieldAlias->Value == "PreviousMode",
		"Expected copied field metadata and enum binding to outlive their source");
	HE::Refl::RuntimeEnumDescriptor collision{};
	collision.Name = "Collision";
	collision.QualifiedName = "Test::Collision";
	collision.UnderlyingType = "int";
	collision.Guid = manualGuid;
	Require(manual.RegisterEnum(collision).has_value(),
		"Expected type and enum registrations to reject the same Guid");

    std::cout << "MetaReflectionOnlySmoke passed\n";
    return 0;
}
