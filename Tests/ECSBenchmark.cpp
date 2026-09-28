#include "HuaEngine/ECS/Runtime/Timeline.h"
#include "HuaEngine/Scene/Scene.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <malloc.h>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    constinit std::atomic<uint64_t> CppNewCalls{0};

    void* Allocate(size_t size, size_t alignment = 0) {
        void* memory = alignment == 0 ? std::malloc(std::max(size, size_t{1}))
                                     : _aligned_malloc(std::max(size, size_t{1}), alignment);
        if (memory == nullptr) {
            throw std::bad_alloc();
        }
        CppNewCalls.fetch_add(1, std::memory_order_relaxed);
        return memory;
    }
}

// Count C++ heap allocations in this benchmark executable, including linked code.
void* operator new(size_t size) { return Allocate(size); }
void* operator new[](size_t size) { return Allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, size_t) noexcept { std::free(memory); }
void* operator new(size_t size, std::align_val_t alignment) {
    return Allocate(size, static_cast<size_t>(alignment));
}
void* operator new[](size_t size, std::align_val_t alignment) {
    return Allocate(size, static_cast<size_t>(alignment));
}
void operator delete(void* memory, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete(void* memory, size_t, std::align_val_t) noexcept { _aligned_free(memory); }
void operator delete[](void* memory, size_t, std::align_val_t) noexcept { _aligned_free(memory); }

namespace {
    using Clock = std::chrono::steady_clock;

    struct Position { float X = 0; float Y = 0; float Z = 0; };
    struct Velocity { float X = 0; float Y = 0; float Z = 0; };
    struct Payload { std::string Name; std::vector<float> Values; };
    struct Mutation { std::vector<float> Values; };
    // Keep the exact baseline workload's ten nonempty marker columns.
    template<unsigned Bit> struct GroupMarker { uint8_t Value = 0; };

    struct Options {
        size_t Entities = 1000;
        size_t Samples = 31;
        uint32_t Seed = 1729;
        std::string Scenario = "dense";
        std::string Mode = "parallel";
        std::string Access = "checked";
    };

    size_t ParseNumber(std::string_view text) {
        size_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size()) {
            throw std::invalid_argument("Expected an unsigned integer");
        }
        return value;
    }

    Options ParseOptions(int argc, char** argv) {
        Options options;
        for (int index = 1; index < argc; index += 2) {
            if (index + 1 >= argc) {
                throw std::invalid_argument("Each option requires a value");
            }
            const std::string_view name = argv[index];
            const std::string_view value = argv[index + 1];
            if (name == "--entities") options.Entities = ParseNumber(value);
            else if (name == "--samples") options.Samples = ParseNumber(value);
            else if (name == "--seed") {
                const size_t seed = ParseNumber(value);
                if (seed > UINT32_MAX) throw std::invalid_argument("Seed exceeds uint32_t");
                options.Seed = static_cast<uint32_t>(seed);
            }
            else if (name == "--scenario") options.Scenario = value;
            else if (name == "--mode") options.Mode = value;
            else if (name == "--access") options.Access = value;
            else throw std::invalid_argument("Unknown benchmark option");
        }
        if (options.Entities == 0 || options.Entities > 10000000 ||
            options.Samples == 0 || options.Samples > 10000) {
            throw std::invalid_argument("Entities must be 1..10000000; samples must be 1..10000");
        }
        if (options.Scenario != "dense" && options.Scenario != "fragmented" &&
            options.Scenario != "nonpod" && options.Scenario != "structural") {
            throw std::invalid_argument("Scenario must be dense, fragmented, nonpod or structural");
        }
        if (options.Mode != "serial" && options.Mode != "parallel") throw std::invalid_argument("Mode must be serial or parallel");
        if (options.Access != "checked" && options.Access != "span" &&
            options.Access != "span-guarded" && options.Access != "span-guarded-once") {
            throw std::invalid_argument("Access must be checked, span, span-guarded or span-guarded-once");
        }
        return options;
    }

    float InitialPosition(size_t index) { return static_cast<float>(index & 1023); }

    float InitialVelocity(size_t index, uint32_t seed) {
        const uint32_t value = seed ^ (static_cast<uint32_t>(index) * 2654435761u);
        return static_cast<float>(value % 17 + 1) * 0.25f;
    }

    template<typename T> T Take(HE::Ecs::Result<T> result) {
        if (!result) throw std::runtime_error(result.GetError().Operation + ": " + result.GetError().Message);
        return std::move(result).Value();
    }
    void Check(HE::Ecs::Result<void> result) {
        if (!result) throw std::runtime_error(result.GetError().Operation + ": " + result.GetError().Message);
    }
    template<unsigned Bit = 0>
    void RegisterMarkers(HE::Ecs::EcsContext& context) {
        Take(context.Types().Register<GroupMarker<Bit>>("Benchmark.Marker" + std::to_string(Bit)));
        if constexpr (Bit < 9) RegisterMarkers<Bit + 1>(context);
    }
    template<unsigned Bit = 0>
    void AddGroupMarkers(HE::Ecs::World& world, HE::EntityId entity, size_t combination) {
        if ((combination & (size_t{1} << Bit)) != 0) Check(world.Emplace<GroupMarker<Bit>>(entity));
        if constexpr (Bit < 9) AddGroupMarkers<Bit + 1>(world, entity, combination);
    }

    double Milliseconds(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }

    double Percentile(std::vector<double> samples, double fraction) {
        std::sort(samples.begin(), samples.end());
        const size_t rank = static_cast<size_t>(std::ceil(samples.size() * fraction));
        return samples[std::max(size_t{1}, rank) - 1];
    }

    void Run(const Options& options) {
        HE::Ecs::EcsContext context;
        HE::Scene scene(context);
        auto& world = scene.GetWorld();
        const auto positionType = Take(context.Types().Register<Position>("Benchmark.Position"));
        const auto velocityType = Take(context.Types().Register<Velocity>("Benchmark.Velocity"));
        Take(context.Types().Register<Payload>("Benchmark.Payload"));
        const auto mutationType = Take(context.Types().Register<Mutation>("Benchmark.Mutation"));
        RegisterMarkers(context);
        std::vector<HE::EntityId> entities;
        entities.reserve(options.Entities);
        std::vector<double> samples;
        samples.reserve(options.Samples);

        const auto setupAllocations = CppNewCalls.load(std::memory_order_relaxed);
        const auto setupStart = Clock::now();
        for (size_t index = 0; index < options.Entities; ++index) {
            const auto entity = Take(scene.CreateEntity("Benchmark"));
            entities.push_back(entity);
            Check(world.Emplace<Position>(entity, Position{InitialPosition(index), 0, 0}));
            Check(world.Emplace<Velocity>(entity, Velocity{InitialVelocity(index, options.Seed), 0, 0}));
            if (options.Scenario == "fragmented") AddGroupMarkers(world, entity, index & 1023);
            if (options.Scenario == "nonpod") {
                Check(world.Emplace<Payload>(entity, Payload{std::string(64, 'p'), std::vector<float>(8, 1.0f)}));
            }
        }
        const double setupMs = Milliseconds(setupStart);
        const auto setupNewCalls = CppNewCalls.load(std::memory_order_relaxed) - setupAllocations;
        const auto coldAllocations = CppNewCalls.load(std::memory_order_relaxed);
        const auto coldStart = Clock::now();
        auto query = Take(HE::Ecs::Query::Create(context, {.Columns = {
            {positionType, HE::Ecs::Presence::Required, HE::Ecs::AccessMode::Write}, {velocityType}}}));
        HE::Ecs::Timeline timeline(context, {.Mode = options.Mode == "serial" ? HE::Ecs::TimelineMode::Serial : HE::Ecs::TimelineMode::Parallel});
        size_t steps = 0;
        std::atomic<size_t> visited{0};
        double dispatchMs = 0;
        double waitMs = 0;
        double mutationMs = 0;

        auto step = [&]() {
            if (options.Scenario == "structural") {
                const auto start = Clock::now();
                const size_t changed = std::max(size_t{1}, options.Entities / 20);
                for (size_t index = 0; index < changed; ++index) {
                    if ((steps & 1) == 0) {
                        Check(world.Emplace<Mutation>(entities[index], Mutation{std::vector<float>(8, 1.0f)}));
                    }
                    else Check(world.Remove(entities[index], mutationType));
                }
                mutationMs += Milliseconds(start);
            }
            visited.store(0, std::memory_order_relaxed);
            const auto dispatchStart = Clock::now();
            Take(timeline.Submit(query, world, [&](HE::Ecs::TaskBatch& task) -> HE::Ecs::Result<void> {
                auto positions = Take(task.View().Column<Position>(0));
                auto velocities = Take(task.View().Column<const Velocity>(1));
                if (options.Access == "span" || options.Access == "span-guarded" || options.Access == "span-guarded-once") {
                    auto positionSpan = positions.TryAsSpan();
                    auto velocitySpan = velocities.TryAsSpan();
                    if (positionSpan && velocitySpan && positionSpan->size() == velocitySpan->size()) {
                        if (options.Access == "span-guarded") {
                            for (size_t row = 0; row < positionSpan->size(); ++row) {
                                if (!task.View().Valid()) return HE::Ecs::Error{HE::Ecs::ErrorCode::InvalidState, "Benchmark", "The query batch expired"};
                                (*positionSpan)[row].X += (*velocitySpan)[row].X;
                                if (!task.View().Valid()) return HE::Ecs::Error{HE::Ecs::ErrorCode::InvalidState, "Benchmark", "The query batch expired"};
                            }
                        }
                        else if (options.Access == "span-guarded-once") {
                            for (size_t row = 0; row < positionSpan->size(); ++row) {
                                if (!task.View().Valid()) return HE::Ecs::Error{HE::Ecs::ErrorCode::InvalidState, "Benchmark", "The query batch expired"};
                                (*positionSpan)[row].X += (*velocitySpan)[row].X;
                            }
                            if (!task.View().Valid()) return HE::Ecs::Error{HE::Ecs::ErrorCode::InvalidState, "Benchmark", "The query batch expired"};
                        }
                        else {
                            for (size_t row = 0; row < positionSpan->size(); ++row) (*positionSpan)[row].X += (*velocitySpan)[row].X;
                        }
                    }
                    else {
                        for (size_t row = 0; row < task.View().Size(); ++row) positions.At(row).X += velocities.At(row).X;
                    }
                }
                else {
                    for (size_t row = 0; row < task.View().Size(); ++row) positions.At(row).X += velocities.At(row).X;
                }
                visited.fetch_add(task.View().Size(), std::memory_order_relaxed);
                return {};
            }));
            dispatchMs += Milliseconds(dispatchStart);
            const auto waitStart = Clock::now();
            Check(timeline.Finish());
            waitMs += Milliseconds(waitStart);
            ++steps;
            if (visited.load(std::memory_order_relaxed) != options.Entities) throw std::runtime_error("Query visited an unexpected number of entities");
        };

        step();
        const double coldMs = Milliseconds(coldStart);
        const auto coldNewCalls = CppNewCalls.load(std::memory_order_relaxed) - coldAllocations;
        constexpr size_t Warmup = 4;
        for (size_t index = 0; index < Warmup; ++index) step();

        mutationMs = 0;
        dispatchMs = 0;
        waitMs = 0;
        const auto warmStats = query.Stats();
        const auto hotAllocations = CppNewCalls.load(std::memory_order_relaxed);
        for (size_t index = 0; index < options.Samples; ++index) {
            const auto start = Clock::now();
            step();
            samples.push_back(Milliseconds(start));
        }
        const auto hotNewCalls = CppNewCalls.load(std::memory_order_relaxed) - hotAllocations;

        double checksum = 0;
        double expected = 0;
        const Payload expectedPayload{std::string(64, 'p'), std::vector<float>(8, 1.0f)};
        for (size_t index = 0; index < options.Entities; ++index) {
            const auto* position = world.TryGet<Position>(entities[index]);
            const float value = InitialPosition(index) + static_cast<float>(steps) * InitialVelocity(index, options.Seed);
            if (position == nullptr || position->X != value) throw std::runtime_error("Position validation failed");
            if (options.Scenario == "nonpod") {
                const auto* payload = world.TryGet<Payload>(entities[index]);
                if (payload == nullptr || payload->Name != expectedPayload.Name ||
                    payload->Values != expectedPayload.Values) {
                    throw std::runtime_error("Non-POD payload validation failed");
                }
            }
            checksum += position->X;
            expected += value;
        }
        if (checksum != expected) throw std::runtime_error("Checksum validation failed");

        const auto hotStats = query.Stats();
        if ((options.Scenario == "dense" || options.Scenario == "nonpod") &&
            (hotStats.GroupMatchCount != warmStats.GroupMatchCount || hotStats.LayoutBindCount != warmStats.LayoutBindCount))
            throw std::runtime_error("Stable hot query rebuilt group or layout bindings");
        const auto beforeCompact = world.Stats();
        const auto compactStart = Clock::now();
        const auto compact = Take(world.CompactAll());
        const auto compactMs = Milliseconds(compactStart);
        const auto afterCompact = world.Stats();

        std::cout << std::setprecision(12)
                  << "{\n  \"schema\": 1,\n  \"backend\": \"hua-runtime\",\n"
                  << "  \"mode\": \"" << options.Mode << "\",\n"
                   << "  \"access\": \"" << options.Access << "\",\n"
                  << "  \"scenario\": \"" << options.Scenario << "\",\n"
                  << "  \"entities\": " << options.Entities << ",\n"
                  << "  \"samples\": " << options.Samples << ",\n"
                  << "  \"warmup_samples\": " << Warmup << ",\n"
                  << "  \"verified_iterations\": " << steps << ",\n"
                  << "  \"seed\": " << options.Seed << ",\n"
                  << "  \"hardware_threads\": " << std::thread::hardware_concurrency() << ",\n"
                  << "  \"worker_threads\": " << (options.Mode == "parallel" ? context.WorkerCount() : 0) << ",\n"
                  << "  \"configured_workers\": " << context.WorkerCount() << ",\n"
#ifdef _MSC_FULL_VER
                  << "  \"msvc_full_version\": " << _MSC_FULL_VER << ",\n"
#endif
#ifdef NDEBUG
                  << "  \"configuration\": \"Release\",\n"
#else
                  << "  \"configuration\": \"Debug\",\n"
#endif
                  << "  \"setup_ms\": " << setupMs << ",\n"
                  << "  \"cold_ms\": " << coldMs << ",\n"
                  << "  \"median_ms\": " << Percentile(samples, 0.5) << ",\n"
                  << "  \"p95_ms\": " << Percentile(samples, 0.95) << ",\n"
                  << "  \"mutation_total_ms\": " << mutationMs << ",\n"
                  << "  \"setup_cpp_new_calls\": " << setupNewCalls << ",\n"
                  << "  \"cold_cpp_new_calls\": " << coldNewCalls << ",\n"
                  << "  \"hot_cpp_new_calls\": " << hotNewCalls << ",\n"
                  << "  \"dispatch_total_ms\": " << dispatchMs << ",\n"
                  << "  \"wait_total_ms\": " << waitMs << ",\n"
                  << "  \"hot_group_match_delta\": " << hotStats.GroupMatchCount - warmStats.GroupMatchCount << ",\n"
                  << "  \"hot_layout_bind_delta\": " << hotStats.LayoutBindCount - warmStats.LayoutBindCount << ",\n"
                  << "  \"chunks_before_compaction\": " << beforeCompact.Chunks << ",\n"
                  << "  \"chunks_after_compaction\": " << afterCompact.Chunks << ",\n"
                  << "  \"compaction_ms\": " << compactMs << ",\n"
                  << "  \"compaction_moved_bytes\": " << compact.MovedBytes << ",\n"
                  << "  \"compaction_returned_bytes\": " << compact.ReturnedBytes << ",\n"
                  << "  \"pool_bytes\": " << afterCompact.PoolBytes << ",\n"
                  << "  \"occupancy\": " << compact.Occupancy << ",\n"
                  << "  \"checksum\": " << checksum << ",\n"
                  << "  \"samples_ms\": [";
        for (size_t index = 0; index < samples.size(); ++index) {
            if (index != 0) std::cout << ", ";
            std::cout << samples[index];
        }
        std::cout << "]\n}\n";
    }
}

int main(int argc, char** argv) {
    try {
        Run(ParseOptions(argc, argv));
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "ECSBenchmark: " << error.what() << '\n';
        return 1;
    }
}
