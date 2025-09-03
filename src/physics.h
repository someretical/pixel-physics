#ifndef PIXELS_PHYSICS_H
#define PIXELS_PHYSICS_H

#include <glm/detail/type_vec2.hpp>
#include <pcg_random.hpp>

#include <array>
#include <random>

namespace physics {
constexpr static int sx_min{ 0 };
constexpr static int sx_max{ 7 };
constexpr static int sy_min{ 0 };
constexpr static int sy_max{ 7 };
constexpr static int a{ 3 };
constexpr static int b{ 7 };
constexpr static int k{ 16 };

using svec2 = glm::tvec2<short, glm::packed_highp>;

struct rng {
    pcg32 rng_s{ pcg_extras::seed_seq_from<std::random_device>{} };
    std::array<std::uniform_int_distribution<short>, 8> shorts{
        std::uniform_int_distribution<short>(0, 1), std::uniform_int_distribution<short>(0, 2),
        std::uniform_int_distribution<short>(0, 3), std::uniform_int_distribution<short>(0, 4),
        std::uniform_int_distribution<short>(0, 5), std::uniform_int_distribution<short>(0, 6),
        std::uniform_int_distribution<short>(0, 7), std::uniform_int_distribution<short>(0, 8),
    };

    pcg32 rng_f{ pcg_extras::seed_seq_from<std::random_device>{} };
    std::uniform_real_distribution<float> floats{ 0.0f, 1.0f };
};
} // namespace physics

#endif // PIXELS_PHYSICS_H