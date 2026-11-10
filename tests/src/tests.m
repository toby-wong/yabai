unsigned char __src_osax_payload[1];
unsigned int __src_osax_payload_len;
unsigned char __src_osax_loader[1];
unsigned int __src_osax_loader_len;

#include "../../src/manifest.m"

#define TEST_SIG(name) bool test_##name(void)
typedef TEST_SIG(function);

#define TEST_FUNC(name, code) static TEST_SIG(name) { char *test_name = #name; bool result = true; {code} return result; }
#define TEST_CHECK(r, e) if ((r) != (e)) { printf("                   \e[1;33m%s\e[m\e[1;31m#%d %s == %s\e[m \e[1;31m(%d == %d)\e[m\n", test_name, __LINE__, #r, #e, r, e); result = false; }

#include "area.c"
#include "window_ordered.c"
#include "native_tabs.c"

#define TEST_ENTRY(name) { #name, test_##name },
#define TEST_LIST                                              \
    TEST_ENTRY(display_area_is_in_direction)                   \
    TEST_ENTRY(closest_display_in_direction)                   \
    TEST_ENTRY(window_ordered_out_leaves_the_tree)             \
    TEST_ENTRY(window_ordered_out_stays_unmanaged)             \
    TEST_ENTRY(window_ordered_in_is_tiled_again)               \
    TEST_ENTRY(window_ordered_in_keeps_float)                  \
    TEST_ENTRY(window_reordered_stays_tiled)                   \
    TEST_ENTRY(window_visible_event_while_still_ordered_out)   \
    TEST_ENTRY(macos_version_newer_than_listed)                \
    TEST_ENTRY(native_tab_preserves_bsp_node)                 \
    TEST_ENTRY(native_tab_preserves_stack_order)              \
    TEST_ENTRY(native_tab_rejects_missing_node)               \
    TEST_ENTRY(native_tab_release_preserves_ordered_out)      \
    TEST_ENTRY(native_tab_hidden_event_keeps_handover_node)   \
    TEST_ENTRY(native_tab_order_in_does_not_tile_hidden_tab)  \
    TEST_ENTRY(native_tabs_keep_ordinary_hide_on_close)       \
    TEST_ENTRY(native_tab_removal_ring_deduplicates)          \
    TEST_ENTRY(native_tab_float_handover_preserves_focus)

static struct {
    char *name;
    test_function *func;
} tests[] = {
    TEST_LIST
};

int main(int argc, char **argv)
{
    int succeeded = 0;
    int failed = 0;
    int total = array_count(tests);
    printf("\e[1;34m -- Running %d tests\e[m\n\n", total);

    uint64_t cpu_freq  = read_cpu_freq();
    uint64_t begin_tsc = read_cpu_timer();

    for (int i = 0; i < total; ++i) {
        uint64_t tsc = read_cpu_timer();
        bool result = tests[i].func();
        double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - tsc) / (double)cpu_freq;

        printf("(%0.4fms) %s \e[1;33m%s\e[m\n", ms_elapsed, result ? "\e[1;32msuccess\e[m" : " \e[1;31mfailed\e[m", tests[i].name);
        if (result) ++succeeded; else ++failed;
    }

    double ms_elapsed = 1000.0 * (double)(read_cpu_timer() - begin_tsc) / (double)cpu_freq;
    printf("\n\e[1;34m -- Completed (%0.4fms)\e[m\n", ms_elapsed);
    printf("\t%d \e[1;32msucceeded\e[m\n", succeeded);
    printf("\t%d \e[1;31mfailed\e[m\n", failed);
    printf("\t%d \e[1;33mtotal\e[m\n", total);

    return total == succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
