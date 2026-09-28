#include "HuaEngine/ECS/Runtime/EcsContext.h"
#include "HuaEngine/ECS/Runtime/Query.h"
#include "HuaEngine/ECS/Runtime/WorkerPool.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace HE::Ecs {
	namespace Detail {
		struct GeneratedQueryCache {
			struct Entry {
				std::unique_ptr<Query> Instance;
				std::vector<std::weak_ptr<const WorldLifetime>> RandomWorlds;
			};
			std::map<std::string, Entry> Entries;
		};
	}
	namespace {
		size_t DefaultWorkerCount() noexcept {
			const unsigned logical = std::thread::hardware_concurrency();
			return logical > 1 ? static_cast<size_t>(logical - 1) : 1;
		}
		std::string GeneratedQueryKey(std::string_view declaration, const QuerySpec& spec) {
			std::string key;
			key.reserve(declaration.size() + 128);
			const auto number = [&](uint64_t value) { key += std::to_string(value); key += ';'; };
			const auto resource = [&](ResourceHandle value) {
				number(value.Id);
				number(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value.Owner)));
			};
			number(declaration.size());
			key.append(declaration);
			key += ';';
			number(spec.Columns.size());
			for (const auto& term : spec.Columns) {
				number(term.Type);
				number(static_cast<uint64_t>(term.Match));
				number(static_cast<uint64_t>(term.Access));
			}
			const auto types = [&](const std::vector<TypeId>& values) {
				number(values.size());
				for (TypeId type : values) number(type);
			};
			types(spec.Required);
			types(spec.Exclude);
			types(spec.ChangedTypes);
			number(spec.SharedBindings.size());
			for (const auto& binding : spec.SharedBindings) {
				number(binding.Type);
				resource(binding.Object);
			}
			number(spec.RandomAccesses.size());
			for (const auto& access : spec.RandomAccesses) {
				number(access.Target ? access.Target->Id() : access.TargetId);
				number(access.Type);
				number(static_cast<uint64_t>(access.Access));
			}
			number(spec.ResourceAccesses.size());
			for (const auto& access : spec.ResourceAccesses) {
				resource(access.Resource);
				number(static_cast<uint64_t>(access.Access));
			}
			number(spec.IncludeDisabledEntities);
			number(spec.IgnoreComponentEnabled);
			return key;
		}
	}
	EcsContext::EcsContext(EcsContextOptions options)
		: m_WorkerCount(options.WorkerCount ? options.WorkerCount : DefaultWorkerCount()) {}
	EcsContext::~EcsContext() = default;
	Result<Query*> EcsContext::FindOrCreateGeneratedQuery(std::string_view declaration, QuerySpec spec) {
		if (!IsMainThread()) return Error{ErrorCode::WrongThread, "GeneratedQuery", "Query construction requires the Context owner thread"};
		if (declaration.empty()) return Error{ErrorCode::InvalidArgument, "GeneratedQuery", "The declaration identity is empty"};
		try {
			if (!m_GeneratedQueryCache) m_GeneratedQueryCache = std::make_unique<Detail::GeneratedQueryCache>();
			auto& entries = m_GeneratedQueryCache->Entries;
			std::erase_if(entries, [](const auto& pair) {
				return std::any_of(pair.second.RandomWorlds.begin(), pair.second.RandomWorlds.end(), [](const auto& weak) {
					const auto lifetime = weak.lock();
					return !lifetime || !lifetime->Owner.load(std::memory_order_acquire);
				});
			});
			const auto key = GeneratedQueryKey(declaration, spec);
			if (const auto found = entries.find(key); found != entries.end()) return found->second.Instance.get();
			std::vector<std::weak_ptr<const Detail::WorldLifetime>> randomWorlds;
			randomWorlds.reserve(spec.RandomAccesses.size());
			for (const auto& access : spec.RandomAccesses) {
				if (access.Target) randomWorlds.push_back(access.Target->Lifetime());
				else randomWorlds.push_back(access.Lifetime);
			}
			auto created = Query::Create(*this, std::move(spec));
			if (!created) return created.GetError();
			auto instance = std::make_unique<Query>(std::move(created).Value());
			auto* result = instance.get();
			entries.emplace(key, Detail::GeneratedQueryCache::Entry{std::move(instance), std::move(randomWorlds)});
			return result;
		}
		catch (const std::exception& exception) {
			return Error{ErrorCode::ConstructionFailed, "GeneratedQuery", exception.what()};
		}
	}
	Detail::WorkerPool& EcsContext::Workers() {
		if (!m_Workers) m_Workers = std::make_unique<Detail::WorkerPool>(m_WorkerCount);
		return *m_Workers;
	}
	std::shared_ptr<Detail::TimelineControl> EcsContext::TimelineSnapshot() const {
		std::lock_guard lock(m_TimelineMutex);
		return m_TimelineControl;
	}
}
