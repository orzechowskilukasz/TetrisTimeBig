// Aplite is tight enough on RAM that the assertion strings matter. Everywhere
// else the safety net is worth its weight.
#if defined(PBL_PLATFORM_APLITE)
    #define USE_ASSERTS 0
#else
    #define USE_ASSERTS 1
#endif

#if USE_ASSERTS == 1

#define ASSERT2(condition, message, ...) if (!(condition)) { \
    APP_LOG(APP_LOG_LEVEL_ERROR, "Assertion failure at %s:%d : " message, __FILE__, __LINE__, ##__VA_ARGS__); \
    int* crash = 0; \
    *crash = 1; \
}

#else

// sizeof does not evaluate its operand, so the condition still costs nothing
// at runtime while anything it mentions still counts as used.
#define ASSERT2(condition, message, ...) ((void)sizeof(condition))

#endif

#define ASSERT(condition) ASSERT2(condition, #condition)

#define ARRAY_SIZE(a) ((sizeof(a) / sizeof(*(a))) / (size_t)(!(sizeof(a) % sizeof(*(a)))))

#define STATIC_ASSERT(condition) _Static_assert(condition, #condition)
